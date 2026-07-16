#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename qw(dirname);
use File::Path qw(make_path);
use File::Spec;
use JSON::PP qw(decode_json);
use Scalar::Util qw(looks_like_number);

sub usage {
    die "usage: $0 [--check] <atom-schema.json> <output-root>\n";
}

sub read_text {
    my ($path) = @_;
    open my $file, '<', $path or die "cannot read $path: $!\n";
    local $/;
    my $text = <$file>;
    close $file or die "cannot close $path: $!\n";
    return $text;
}

sub require_hash {
    my ($value, $label) = @_;
    die "$label must be an object\n" unless ref($value) eq 'HASH';
    return $value;
}

sub require_array {
    my ($value, $label) = @_;
    die "$label must be an array\n" unless ref($value) eq 'ARRAY';
    return $value;
}

sub require_name {
    my ($value, $label, $pattern) = @_;
    die "$label is invalid\n" unless defined($value) && !ref($value) && $value =~ $pattern;
    return $value;
}

sub require_relative_path {
    my ($value, $label, $pattern) = @_;
    require_name($value, $label, $pattern);
    die "$label must not contain parent traversal\n" if $value =~ m{(?:\A|/)\.\.(?:/|\z)};
    return $value;
}

sub generated_c_banner {
    my ($source) = @_;
    return "/* Generated from $source by tools/generate_atom_artifacts.pl. Do not edit. */";
}

sub generated_ts_banner {
    my ($source) = @_;
    return "// Generated from $source by tools/generate_atom_artifacts.pl. Do not edit.";
}

sub c_member_declaration {
    my ($field) = @_;
    return $field->{c_type} =~ /\*\z/
        ? "$field->{c_type}$field->{name};"
        : "$field->{c_type} $field->{name};";
}

sub c_field_block {
    my ($fields) = @_;
    return '{ ' . join(' ', map { c_member_declaration($_) } @$fields) . ' }';
}

my $check = 0;
if (@ARGV && $ARGV[0] eq '--check') {
    shift @ARGV;
    $check = 1;
}
my ($schema_path, $output_root) = @ARGV;
usage() unless defined($schema_path) && defined($output_root) && @ARGV == 2;

my $document = decode_json(read_text($schema_path));
require_hash($document, 'schema');
die "unsupported atom definition schema\n"
    unless ($document->{'$schema'} // '') eq 'apg.atom-definitions.v1';

my $source = require_relative_path(
    $document->{source}, 'source', qr{\Aschema/atoms/[a-z0-9_]+\.json\z}
);
my $empty_layout = require_hash($document->{empty_layout}, 'empty_layout');
die "empty_layout must remain uint8_t _reserved\n"
    unless ($empty_layout->{c_type} // '') eq 'uint8_t' && ($empty_layout->{name} // '') eq '_reserved';

my %allowed_c_type = map { $_ => 1 } ('float', 'float *', 'float **', 'int', 'uint32_t', 'uint8_t');
my %allowed_ownership = map { $_ => 1 } ('value', 'borrowed', 'runtime_owned', 'external');
my %allowed_field_type = map { $_ => 1 } ('FIELD_FLOAT', 'FIELD_INT', 'FIELD_SIGNAL', 'FIELD_BUFFER', 'FIELD_FLOAT_PP');
my %allowed_contract_type = map { $_ => 1 }
    ('signal', 'signal_optional', 'signal_array', 'scalar', 'float', 'int', 'buffer', 'float_matrix');
my %allowed_dispatch = map { $_ => 1 }
    ('PROCESS', 'FFT', 'IFFT', 'MULTIPLY', 'WINDOW', 'OVERLAP_ADD', 'OVERLAP_SAVE', 'STREAM');
my %allowed_capability = map { $_ => 1 } ('PORTABLE', 'WASM', 'WASM_ANTIALIASED', 'EXPERIMENTAL');
my %allowed_maturity = map { $_ => 1 } ('SAFE_SCALAR', 'MUSICAL', 'EXPERIMENTAL');
my %allowed_visibility = map { $_ => 1 } ('public', 'advanced', 'internal');
my %allowed_parameter_type = map { $_ => 1 } ('float', 'int', 'bool', 'enum', 'buffer', 'float_matrix');
my %allowed_parameter_unit = map { $_ => 1 } ('hz', 'ms', 'db', 'ratio', 'samples');
my %allowed_parameter_scale = map { $_ => 1 } ('linear', 'logarithmic');

sub validate_c_fields {
    my ($fields, $label, $role) = @_;
    require_array($fields, $label);
    my %seen;
    for my $field (@$fields) {
        require_hash($field, "$label field");
        my $name = require_name($field->{name}, "$label field.name", qr/\A_?[a-z][a-z0-9_]*\z/);
        die "$label contains duplicate field $name\n" if $seen{$name}++;
        my $c_type = $field->{c_type} // '';
        my $ownership = $field->{ownership} // '';
        die "$label field $name has unsupported C type\n" unless $allowed_c_type{$c_type};
        die "$label field $name has unsupported ownership\n" unless $allowed_ownership{$ownership};
        if ($c_type =~ /\*/ && $ownership eq 'value') {
            die "$label pointer field $name requires explicit non-value ownership\n";
        }
        if ($c_type !~ /\*/ && $ownership ne 'value') {
            die "$label scalar field $name must use value ownership\n";
        }
        if (exists $field->{descriptor}) {
            my $descriptor = require_hash($field->{descriptor}, "$label field $name descriptor");
            my $field_type = $descriptor->{field_type} // '';
            die "$label field $name has unsupported descriptor type\n" unless $allowed_field_type{$field_type};
            if (exists $descriptor->{capacity}) {
                die "$label field $name has invalid capacity expression\n"
                    unless !ref($descriptor->{capacity}) &&
                        $descriptor->{capacity} =~ /\A(?:[0-9]+u|APG_[A-Z0-9_]+)\z/;
                die "$label field $name capacity is only valid for FIELD_BUFFER\n"
                    unless $field_type eq 'FIELD_BUFFER';
            }
            if ($field_type eq 'FIELD_BUFFER' && $role eq 'state') {
                die "$label state buffer $name requires a capacity\n" unless exists $descriptor->{capacity};
                die "$label state buffer $name must be runtime_owned\n" unless $ownership eq 'runtime_owned';
            }
        }
        if (exists $field->{catalog}) {
            if (ref($field->{catalog}) eq 'HASH') {
                my $catalog = $field->{catalog};
                die "$label field $name has unsupported catalog type\n"
                    unless $allowed_contract_type{$catalog->{type} // ''};
                die "$label field $name catalog.required must be boolean\n"
                    unless exists $catalog->{required} && JSON::PP::is_bool($catalog->{required});
            } else {
                die "$label field $name catalog override must be an object or false\n"
                    unless JSON::PP::is_bool($field->{catalog}) && !$field->{catalog};
            }
        }
    }
}

my $families = require_array($document->{families}, 'families');
my (%family_by_name, %family_category, %constant_by_name);
for my $family (@$families) {
    require_hash($family, 'family');
    my $name = require_name($family->{name}, 'family.name', qr/\A[a-z][a-z0-9_]*\z/);
    die "duplicate family $name\n" if $family_by_name{$name};
    my $category = require_name($family->{category}, "$name category", qr/\A[a-z][a-z0-9_]*\z/);
    require_relative_path(
        $family->{header}, "$name header", qr{\Ainc/atom/types/[a-z0-9_]+_types\.h\z}
    );
    require_relative_path(
        $family->{descriptor_source}, "$name descriptor_source",
        qr{\Asrc/atom/[a-z0-9_]+/[a-z0-9_]+_field_descriptors\.c\z}
    );
    require_name($family->{guard}, "$name guard", qr/\A[A-Z][A-Z0-9_]*\z/);
    require_name($family->{include}, "$name include", qr{\Aatom/types/[a-z0-9_]+\.h\z});
    require_name($family->{table_macro}, "$name table_macro", qr/\AAPG_[A-Z0-9_]+_DSP_TYPE_TABLE\z/);
    my $constants = $family->{constants} // [];
    require_array($constants, "$name constants");
    for my $constant (@$constants) {
        require_hash($constant, "$name constant");
        my $constant_name = require_name(
            $constant->{name}, "$name constant.name", qr/\AAPG_[A-Z][A-Z0-9_]*\z/
        );
        die "duplicate constant $constant_name\n" if $constant_by_name{$constant_name};
        my $value = require_name($constant->{value}, "$name constant.value", qr/\A[1-9][0-9]*u\z/);
        $constant_by_name{$constant_name} = $value;
    }
    $family->{constants} = $constants;
    $family_by_name{$name} = $family;
    $family_category{$category} = 1;
}
die "families must not be empty\n" unless %family_by_name;

my $profiles = require_array($document->{io_profiles}, 'io_profiles');
my (%profile_by_name, @profile_names);
for my $profile (@$profiles) {
    require_hash($profile, 'I/O profile');
    my $name = require_name($profile->{name}, 'I/O profile.name', qr/\A[A-Z][A-Z0-9_]*\z/);
    die "duplicate I/O profile $name\n" if $profile_by_name{$name};
    my $fields = require_array($profile->{fields}, "$name fields");
    my %seen;
    for my $field (@$fields) {
        require_hash($field, "$name field");
        my $field_name = require_name($field->{name}, "$name field.name", qr/\A_?[a-z][a-z0-9_]*\z/);
        die "$name contains duplicate field $field_name\n" if $seen{$field_name}++;
        die "$name field $field_name has unsupported C type\n" unless $allowed_c_type{$field->{c_type} // ''};
        die "$name field $field_name has unsupported contract type\n"
            unless $allowed_contract_type{$field->{contract_type} // ''};
        die "$name field $field_name required must be boolean\n"
            unless exists $field->{required} && JSON::PP::is_bool($field->{required});
    }
    $profile_by_name{$name} = $profile;
    push @profile_names, $name;
}
die "EMPTY I/O profile is required\n" unless $profile_by_name{EMPTY};

my $atoms = require_array($document->{atoms}, 'atoms');
die "atoms must not be empty\n" unless @$atoms;
my (%atom_by_name, %atoms_by_family);
for my $atom (@$atoms) {
    require_hash($atom, 'atom');
    my $name = require_name($atom->{name}, 'atom.name', qr/\A[a-z][a-z0-9_]*\z/);
    die "duplicate atom $name\n" if $atom_by_name{$name};
    my $family_name = require_name($atom->{family}, "$name family", qr/\A[a-z][a-z0-9_]*\z/);
    my $family = $family_by_name{$family_name} or die "$name references unknown family $family_name\n";
    die "$name category differs from family category\n"
        unless ($atom->{category} // '') eq $family->{category};
    my $out_profile = $atom->{output_profile} // '';
    my $in_profile = $atom->{input_profile} // '';
    die "$name references unknown output profile $out_profile\n" unless $profile_by_name{$out_profile};
    die "$name references unknown input profile $in_profile\n" unless $profile_by_name{$in_profile};
    die "$name has unsupported dispatch\n" unless $allowed_dispatch{$atom->{dispatch} // ''};
    die "$name has unsupported capability\n" unless $allowed_capability{$atom->{capability} // ''};
    die "$name has unsupported maturity\n" unless $allowed_maturity{$atom->{maturity} // ''};
    validate_c_fields($atom->{params}, "$name params", 'params');
    validate_c_fields($atom->{state}, "$name state", 'state');
    for my $field (@{$atom->{state}}) {
        next unless exists $field->{descriptor};
        next unless exists $field->{descriptor}{capacity};
        my $capacity = $field->{descriptor}{capacity};
        die "$name state field $field->{name} references unknown capacity constant $capacity\n"
            if $capacity =~ /\AAPG_/ && !exists $constant_by_name{$capacity};
    }

    my %input_members = map { $_->{name} => $_ } @{$profile_by_name{$in_profile}{fields}};
    my %seen_input_descriptor;
    for my $descriptor (@{require_array($atom->{input_descriptors}, "$name input_descriptors")}) {
        require_hash($descriptor, "$name input descriptor");
        my $field_name = require_name(
            $descriptor->{name}, "$name input descriptor.name", qr/\A[a-z][a-z0-9_]*\z/
        );
        die "$name has duplicate input descriptor $field_name\n" if $seen_input_descriptor{$field_name}++;
        my $member = require_name(
            $descriptor->{member}, "$name input descriptor.member", qr/\A[a-z][a-z0-9_]*\z/
        );
        die "$name input descriptor $field_name references unknown member $member\n"
            unless $input_members{$member};
        die "$name input descriptor $field_name has unsupported type\n"
            unless $allowed_field_type{$descriptor->{field_type} // ''};
        die "$name input descriptor capacity is not supported\n" if exists $descriptor->{capacity};
    }

    die "$name params must not contain sample_rate\n"
        if grep { $_->{name} eq 'sample_rate' } @{$atom->{params}};
    my @state_buffers = grep {
        exists $_->{descriptor} && $_->{descriptor}{field_type} eq 'FIELD_BUFFER'
    } @{$atom->{state}};
    if (@state_buffers) {
        my ($length) = grep {
            $_->{name} eq 'buffer_len' && $_->{c_type} eq 'uint32_t' && exists $_->{descriptor} &&
                $_->{descriptor}{field_type} eq 'FIELD_INT'
        } @{$atom->{state}};
        die "$name state buffers require an explicit uint32_t buffer_len descriptor\n" unless $length;
    }

    $atom_by_name{$name} = $atom;
    push @{$atoms_by_family{$family_name}}, $atom;
}
for my $family (@$families) {
    die "family $family->{name} has no atoms\n" unless $atoms_by_family{$family->{name}};
}

my $visibility_groups = require_hash($document->{visibility_groups}, 'visibility_groups');
my %atom_visibility;
for my $visibility (sort keys %$visibility_groups) {
    die "unsupported visibility group $visibility\n" unless $allowed_visibility{$visibility};
    my $names = require_array($visibility_groups->{$visibility}, "$visibility visibility group");
    for my $name (@$names) {
        require_name($name, "$visibility atom", qr/\A[a-z][a-z0-9_]*\z/);
        die "$visibility visibility references unknown atom $name\n" unless $atom_by_name{$name};
        die "atom $name has duplicate visibility\n" if exists $atom_visibility{$name};
        $atom_visibility{$name} = $visibility;
    }
}
for my $atom (@$atoms) {
    my $visibility = $atom_visibility{$atom->{name}};
    die "$atom->{name} has no visibility\n" unless defined $visibility;
    $atom->{visibility} = $visibility;
}

my $parameter_metadata = require_hash($document->{parameter_metadata}, 'parameter_metadata');

my %capability_expression = (
    PORTABLE => 'APG_ATOM_FLAGS_PORTABLE',
    WASM => 'APG_ATOM_FLAGS_WASM',
    WASM_ANTIALIASED => 'APG_ATOM_FLAGS_WASM | APG_ATOM_ANTIALIASED',
    EXPERIMENTAL => 'APG_ATOM_FLAGS_EXPERIMENTAL',
);
my %contract_enum = (
    signal => 'APG_ATOM_FIELD_SIGNAL',
    signal_optional => 'APG_ATOM_FIELD_SIGNAL_OPTIONAL',
    signal_array => 'APG_ATOM_FIELD_SIGNAL_ARRAY',
    scalar => 'APG_ATOM_FIELD_SCALAR',
    float => 'APG_ATOM_FIELD_FLOAT',
    int => 'APG_ATOM_FIELD_INT',
    buffer => 'APG_ATOM_FIELD_BUFFER',
    float_matrix => 'APG_ATOM_FIELD_FLOAT_MATRIX',
);
my %descriptor_contract_type = (
    FIELD_FLOAT => 'float',
    FIELD_INT => 'int',
    FIELD_SIGNAL => 'signal',
    FIELD_BUFFER => 'buffer',
    FIELD_FLOAT_PP => 'float_matrix',
);

sub logical_fields {
    my ($fields) = @_;
    return [grep { $_->{name} ne '_reserved' && $_->{name} ne 'unused' } @$fields];
}

sub descriptor_fields {
    my ($fields) = @_;
    return [grep { exists $_->{descriptor} } @$fields];
}

sub catalog_config_fields {
    my ($atom) = @_;
    my @result;
    for my $field (@{$atom->{params}}) {
        if (exists $field->{catalog}) {
            next unless ref($field->{catalog}) eq 'HASH';
            push @result, {
                name => $field->{name},
                type => $field->{catalog}{type},
                required => $field->{catalog}{required} ? 1 : 0,
                metadata => $field->{parameter_metadata},
            };
            next;
        }
        next unless exists $field->{descriptor};
        my $type = $descriptor_contract_type{$field->{descriptor}{field_type}}
            or die "$atom->{name} config field $field->{name} cannot map to a catalog type\n";
        push @result, {
            name => $field->{name},
            type => $type,
            required => 1,
            metadata => $field->{parameter_metadata},
        };
    }
    return \@result;
}

sub inferred_parameter_type {
    my ($contract_type) = @_;
    return 'float' if $contract_type eq 'float' || $contract_type eq 'scalar';
    return $contract_type if $contract_type eq 'int' || $contract_type eq 'buffer' || $contract_type eq 'float_matrix';
    die "cannot infer parameter type from contract type $contract_type\n";
}

sub validate_float_matrix {
    my ($value, $label) = @_;
    my $rows = require_array($value, $label);
    die "$label must not be empty\n" unless @$rows;
    for my $row (@$rows) {
        my $values = require_array($row, "$label row");
        die "$label rows must not be empty\n" unless @$values;
        for my $item (@$values) {
            die "$label values must be numbers\n" if ref($item) || !looks_like_number($item);
        }
    }
}

sub validate_parameter_metadata {
    my ($key, $metadata, $contract_type) = @_;
    require_hash($metadata, "$key parameter metadata");
    my %allowed_key = map { $_ => 1 }
        qw(default type min max unit scale realtime smoothing_ms structural options option_values);
    for my $name (keys %$metadata) {
        die "$key parameter metadata has unsupported key $name\n" unless $allowed_key{$name};
    }

    die "$key parameter metadata requires default\n" unless exists $metadata->{default};
    for my $name (qw(realtime structural)) {
        die "$key parameter metadata requires boolean $name\n"
            unless exists $metadata->{$name} && JSON::PP::is_bool($metadata->{$name});
    }
    die "$key structural parameter cannot be realtime\n"
        if $metadata->{structural} && $metadata->{realtime};

    my $base_type = inferred_parameter_type($contract_type);
    my $type = $metadata->{type} // $base_type;
    die "$key parameter metadata has unsupported type $type\n" unless $allowed_parameter_type{$type};
    my $compatible =
        ($type eq $base_type) ||
        (($type eq 'enum' || $type eq 'bool') && $base_type eq 'int');
    die "$key parameter type $type is incompatible with contract type $contract_type\n" unless $compatible;

    my $default = $metadata->{default};
    if ($type eq 'float') {
        die "$key default must be a number\n" if ref($default) || !looks_like_number($default);
    } elsif ($type eq 'int' || $type eq 'enum') {
        die "$key default must be an integer\n"
            if ref($default) || !looks_like_number($default) || int($default) != $default;
    } elsif ($type eq 'bool') {
        die "$key default must be a boolean\n" unless JSON::PP::is_bool($default);
    } elsif ($type eq 'buffer') {
        die "$key buffer default must be a string binding\n" if !defined($default) || ref($default);
    } elsif ($type eq 'float_matrix') {
        validate_float_matrix($default, "$key default");
    }

    for my $name (qw(min max smoothing_ms)) {
        next unless exists $metadata->{$name};
        die "$key $name must be a number\n"
            if ref($metadata->{$name}) || !looks_like_number($metadata->{$name});
    }
    die "$key min/max are only valid for numeric parameters\n"
        if (exists($metadata->{min}) || exists($metadata->{max})) &&
            $type ne 'float' && $type ne 'int' && $type ne 'enum';
    die "$key min exceeds max\n"
        if exists($metadata->{min}) && exists($metadata->{max}) && $metadata->{min} > $metadata->{max};
    die "$key default is below min\n"
        if !ref($default) && looks_like_number($default) && exists($metadata->{min}) && $default < $metadata->{min};
    die "$key default is above max\n"
        if !ref($default) && looks_like_number($default) && exists($metadata->{max}) && $default > $metadata->{max};
    die "$key smoothing_ms must be non-negative\n"
        if exists($metadata->{smoothing_ms}) && $metadata->{smoothing_ms} < 0;
    die "$key smoothing_ms is only valid for realtime parameters\n"
        if exists($metadata->{smoothing_ms}) && !$metadata->{realtime};

    if (exists $metadata->{unit}) {
        die "$key has unsupported unit $metadata->{unit}\n" unless $allowed_parameter_unit{$metadata->{unit}};
    }
    if (exists $metadata->{scale}) {
        die "$key has unsupported scale $metadata->{scale}\n" unless $allowed_parameter_scale{$metadata->{scale}};
        die "$key logarithmic scale requires a positive min\n"
            if $metadata->{scale} eq 'logarithmic' &&
                (!exists($metadata->{min}) || $metadata->{min} <= 0);
    }

    if ($type eq 'enum') {
        my $options = require_array($metadata->{options}, "$key options");
        die "$key enum options must not be empty\n" unless @$options;
        my %seen_option;
        for my $option (@$options) {
            die "$key enum option must be a non-empty string\n"
                if !defined($option) || ref($option) || $option eq '';
            die "$key has duplicate enum option $option\n" if $seen_option{$option}++;
        }
        my $values = $metadata->{option_values};
        if (defined $values) {
            require_array($values, "$key option_values");
            die "$key option_values length differs from options\n" unless @$values == @$options;
        } else {
            $values = [0 .. $#$options];
        }
        my %seen_value;
        for my $value (@$values) {
            die "$key enum option value must be an integer\n"
                if ref($value) || !looks_like_number($value) || int($value) != $value;
            die "$key has duplicate enum option value $value\n" if $seen_value{$value}++;
        }
        die "$key enum default is not an option value\n" unless $seen_value{$default};
        $metadata->{option_values} = $values;
    } elsif (exists($metadata->{options}) || exists($metadata->{option_values})) {
        die "$key options are only valid for enum parameters\n";
    }

    $metadata->{type} = $type;
    return $metadata;
}

my %expected_parameter_metadata;
for my $atom (@$atoms) {
    for my $field (@{catalog_config_fields($atom)}) {
        my $key = "$atom->{name}.$field->{name}";
        die "duplicate exposed parameter $key\n" if $expected_parameter_metadata{$key};
        my $metadata = $parameter_metadata->{$key};
        die "$key has no parameter metadata\n" unless defined $metadata;
        $expected_parameter_metadata{$key} = 1;
        my $validated = validate_parameter_metadata($key, $metadata, $field->{type});
        for my $source_field (@{$atom->{params}}) {
            if ($source_field->{name} eq $field->{name}) {
                $source_field->{parameter_metadata} = $validated;
                last;
            }
        }
    }
}
for my $key (sort keys %$parameter_metadata) {
    die "parameter metadata references unexposed field $key\n" unless $expected_parameter_metadata{$key};
}

sub profile_catalog_fields {
    my ($profile) = @_;
    return [map {
        {name => $_->{name}, type => $_->{contract_type}, required => $_->{required} ? 1 : 0}
    } @{logical_fields($profile->{fields})}];
}

sub render_type_macros_header {
    my @lines = (
        generated_c_banner($source),
        '#ifndef AUDIO_PLAYGROUND_DSP_TYPE_MACROS_H',
        '#define AUDIO_PLAYGROUND_DSP_TYPE_MACROS_H',
        '',
        '#include <stdint.h>',
        '',
        '/* I/O profiles describe C member layouts, not YAML binding contracts. */',
        '// clang-format off',
    );
    for my $profile (@$profiles) {
        push @lines, sprintf(
            '#define APG_IO_FIELDS_%-22s %s', $profile->{name}, c_field_block($profile->{fields})
        );
    }
    push @lines, (
        '// clang-format on',
        '',
        '#define APG_EXPAND_IO_FIELDS(profile)       APG_EXPAND_IO_FIELDS_INNER(profile)',
        '#define APG_EXPAND_IO_FIELDS_INNER(profile) APG_IO_FIELDS_##profile',
        '',
        '#define APG_DECLARE_DSP_TYPES(atom_name, out_profile, in_profile, params_fields, state_fields) \\',
        '    typedef struct APG_EXPAND_IO_FIELDS(out_profile) atom_name##_out_t;                        \\',
        '    typedef struct APG_EXPAND_IO_FIELDS(in_profile) atom_name##_in_t;                          \\',
        '    typedef struct params_fields atom_name##_params_t;                                         \\',
        '    typedef struct state_fields  atom_name##_state_t;',
        '',
        '#endif // AUDIO_PLAYGROUND_DSP_TYPE_MACROS_H',
    );
    return join("\n", @lines) . "\n";
}

sub render_family_header {
    my ($family) = @_;
    my $family_atoms = $atoms_by_family{$family->{name}};
    my @lines = (
        generated_c_banner($source),
        "#ifndef $family->{guard}",
        "#define $family->{guard}",
        '',
        "#include <$family->{include}>",
        '',
    );
    if (@{$family->{constants}}) {
        my $width = 0;
        for my $constant (@{$family->{constants}}) {
            $width = length($constant->{name}) if length($constant->{name}) > $width;
        }
        for my $constant (@{$family->{constants}}) {
            push @lines, sprintf('#define %-*s %s', $width, $constant->{name}, $constant->{value});
        }
        push @lines, '';
    }
    push @lines, '// clang-format off', "#define $family->{table_macro}(X) \\";
    for my $index (0 .. $#$family_atoms) {
        my $atom = $family_atoms->[$index];
        my $continuation = $index == $#$family_atoms ? '' : ' \\';
        push @lines, sprintf(
            '    X(%s, %s, %s, %s, %s)%s',
            $atom->{name},
            $atom->{output_profile},
            $atom->{input_profile},
            c_field_block($atom->{params}),
            c_field_block($atom->{state}),
            $continuation,
        );
    }
    push @lines, (
        '// clang-format on',
        '',
        "$family->{table_macro}(APG_DECLARE_DSP_TYPES)",
        '',
        "#endif // $family->{guard}",
    );
    return join("\n", @lines) . "\n";
}

sub render_atom_definitions {
    my @lines = (
        generated_c_banner($source),
        '#ifndef AUDIO_PLAYGROUND_ATOM_DEFINITIONS_GENERATED_H',
        '#define AUDIO_PLAYGROUND_ATOM_DEFINITIONS_GENERATED_H',
        '',
        '/* name, category, input fields, config fields, state fields, capabilities, maturity, dispatch */',
        '#define APG_ATOM_DEFINITIONS(X) \\',
    );
    for my $index (0 .. $#$atoms) {
        my $atom = $atoms->[$index];
        my $input_count = scalar @{$atom->{input_descriptors}};
        my $config_count = scalar @{descriptor_fields($atom->{params})};
        my $state_count = scalar @{descriptor_fields($atom->{state})};
        my $continuation = $index == $#$atoms ? '' : ' \\';
        push @lines, sprintf(
            '    X(%s, %s, %d, %d, %d, %s, APG_ATOM_MATURITY_%s, %s)%s',
            $atom->{name}, $atom->{category}, $input_count, $config_count, $state_count,
            $capability_expression{$atom->{capability}}, $atom->{maturity}, $atom->{dispatch}, $continuation
        );
    }
    push @lines, ('', '#endif // AUDIO_PLAYGROUND_ATOM_DEFINITIONS_GENERATED_H');
    return join("\n", @lines) . "\n";
}

sub render_process_prototype {
    my ($atom, $context_type, $return_type, $suffix) = @_;
    my $name = $atom->{name};
    return join("\n",
        "$return_type ${name}${suffix}(",
        "    ${name}_out_t *out,",
        "    const ${name}_in_t *in,",
        "    const ${name}_params_t *params,",
        "    ${name}_state_t *state,",
        "    const $context_type *context",
        ');',
        ''
    );
}

sub render_dsp_declarations {
    my @parts = (
        generated_c_banner($source),
        '#ifndef AUDIO_PLAYGROUND_DSP_ATOMS_GENERATED_H',
        '#define AUDIO_PLAYGROUND_DSP_ATOMS_GENERATED_H',
        ''
    );
    for my $atom (@$atoms) {
        my $dispatch = $atom->{dispatch};
        if ($dispatch eq 'STREAM') {
            push @parts, render_process_prototype($atom, 'apg_stream_context_t', 'apg_stream_result_t', '_process');
        } elsif ($dispatch eq 'FFT' || $dispatch eq 'IFFT' || $dispatch eq 'MULTIPLY') {
            push @parts, render_process_prototype($atom, 'apg_spectral_info_t', 'void', '_process');
        } else {
            push @parts, render_process_prototype($atom, 'apg_process_context_t', 'void', '_process');
            if ($dispatch eq 'WINDOW' || $dispatch eq 'OVERLAP_ADD' || $dispatch eq 'OVERLAP_SAVE') {
                push @parts, render_process_prototype($atom, 'apg_spectral_info_t', 'void', '_spectral_process');
            }
        }
    }
    push @parts, '#endif // AUDIO_PLAYGROUND_DSP_ATOMS_GENERATED_H', '';
    return join("\n", @parts);
}

sub render_descriptor_entry {
    my ($atom_name, $role, $field) = @_;
    my ($name, $member, $field_type, $capacity);
    if ($role eq 'in') {
        ($name, $member, $field_type) = @{$field}{qw(name member field_type)};
        $capacity = $field->{capacity};
    } else {
        $name = $member = $field->{name};
        $field_type = $field->{descriptor}{field_type};
        $capacity = $field->{descriptor}{capacity};
    }
    my $type_role = $role eq 'config' ? 'params' : $role;
    my $entry = sprintf(
        '    {"%s", %s, offsetof(%s_%s_t, %s)', $name, $field_type, $atom_name, $type_role, $member
    );
    $entry .= ", $capacity" if defined $capacity;
    return "$entry},";
}

sub render_field_descriptors {
    my ($family) = @_;
    my @lines = (
        generated_c_banner($source),
        '#include "atom/atom_field_descriptors.h"',
        '#include "atom/dsp_atoms.h"',
        '',
        '#include <stddef.h>',
        '',
        '#define FIELD_COUNT(name) const int name##_count = (int)(sizeof(name) / sizeof((name)[0]))',
        '',
        '// clang-format off',
    );
    for my $atom (@{$atoms_by_family{$family->{name}}}) {
        my @sections = (
            ['in', $atom->{input_descriptors}],
            ['config', descriptor_fields($atom->{params})],
            ['state', descriptor_fields($atom->{state})],
        );
        for my $section (@sections) {
            my ($role, $fields) = @$section;
            next unless @$fields;
            my $array_name = "$atom->{name}_${role}_fields";
            push @lines, "const atom_field_desc_t $array_name\[\] = {";
            push @lines, map { render_descriptor_entry($atom->{name}, $role, $_) } @$fields;
            push @lines, '};', "FIELD_COUNT($array_name);", '';
        }
    }
    push @lines, '// clang-format on', '', '#undef FIELD_COUNT';
    return join("\n", @lines) . "\n";
}

my $canonical_json = JSON::PP->new->canonical(1)->allow_nonref(1);

sub json_literal {
    my ($value) = @_;
    return $canonical_json->encode($value);
}

sub c_quote {
    my ($value) = @_;
    $value =~ s/\\/\\\\/g;
    $value =~ s/"/\\"/g;
    $value =~ s/\n/\\n/g;
    $value =~ s/\r/\\r/g;
    $value =~ s/\t/\\t/g;
    return qq{"$value"};
}

sub c_bool {
    my ($value) = @_;
    return $value ? 'true' : 'false';
}

sub c_optional_string {
    my ($value) = @_;
    return defined($value) ? c_quote($value) : 'NULL';
}

sub render_catalog_array {
    my ($name, $fields) = @_;
    return () unless @$fields;
    my @lines;
    my %option_arrays;
    for my $field (@$fields) {
        my $metadata = $field->{metadata};
        next unless $metadata && $metadata->{type} eq 'enum';
        my $prefix = "${name}_$field->{name}";
        my $labels = join(', ', map { c_quote($_) } @{$metadata->{options}});
        my $values = join(', ', @{$metadata->{option_values}});
        push @lines,
            "static const char *const ${prefix}_options\[\] = {$labels};",
            "static const int ${prefix}_option_values\[\] = {$values};";
        $option_arrays{$field->{name}} = $prefix;
    }
    push @lines, '' if @lines;
    push @lines, "static const apg_atom_contract_field_t $name\[\] = {";
    for my $field (@$fields) {
        my $required = c_bool($field->{required});
        my $metadata = $field->{metadata};
        if (!$metadata) {
            push @lines, sprintf(
                '    {%s, %s, %s},', c_quote($field->{name}), $contract_enum{$field->{type}}, $required
            );
            next;
        }
        my $has_min = exists $metadata->{min};
        my $has_max = exists $metadata->{max};
        my $has_smoothing = exists $metadata->{smoothing_ms};
        my $prefix = $option_arrays{$field->{name}};
        my $options = $prefix ? "${prefix}_options" : 'NULL';
        my $option_values = $prefix ? "${prefix}_option_values" : 'NULL';
        my $options_len = $prefix ? 'FIELD_COUNT(' . $options . ')' : '0u';
        push @lines, sprintf(
            '    {%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s},',
            c_quote($field->{name}),
            $contract_enum{$field->{type}},
            $required,
            c_quote($metadata->{type}),
            c_quote(json_literal($metadata->{default})),
            c_bool($has_min),
            $has_min ? $metadata->{min} : '0.0',
            c_bool($has_max),
            $has_max ? $metadata->{max} : '0.0',
            c_optional_string($metadata->{unit}),
            c_optional_string($metadata->{scale}),
            c_bool($metadata->{realtime}),
            c_bool($has_smoothing),
            $has_smoothing ? $metadata->{smoothing_ms} : '0.0',
            c_bool($metadata->{structural}),
            $options,
            $option_values,
            $options_len,
        );
    }
    push @lines, '};', '';
    return @lines;
}

sub render_catalog_contracts {
    my @lines = (generated_c_banner($source), '');
    my @rows;
    for my $atom (@$atoms) {
        my $input = profile_catalog_fields($profile_by_name{$atom->{input_profile}});
        my $output = profile_catalog_fields($profile_by_name{$atom->{output_profile}});
        my $config = catalog_config_fields($atom);
        my %arrays;
        for my $section (
            ['inputs', $input], ['outputs', $output], ['config', $config]
        ) {
            my ($kind, $fields) = @$section;
            my $array_name = "catalog_$atom->{name}_${kind}";
            push @lines, render_catalog_array($array_name, $fields);
            $arrays{$kind} = @$fields ? "$array_name, FIELD_COUNT($array_name)" : 'NULL, 0u';
        }
        push @rows, sprintf(
            '    {"%s", APG_ATOM_VISIBILITY_%s, %s, %s, %s},',
            $atom->{name}, uc($atom->{visibility}), $arrays{inputs}, $arrays{outputs}, $arrays{config}
        );
    }
    push @lines, 'static const apg_catalog_contract_t catalog_contracts[] = {', @rows, '};';
    return join("\n", @lines) . "\n";
}

sub ts_quote_list {
    my ($values) = @_;
    return '[' . join(', ', map { json_literal($_) } @$values) . ']';
}

sub render_typescript_catalog {
    my @categories = map { $_->{category} } @$families;
    my %seen_category;
    @categories = grep { !$seen_category{$_}++ } @categories;
    my $category_union = join(' | ', map { "'$_'" } @categories);
    my @lines = (
        generated_ts_banner($source),
        '',
        "export type AtomVisibility = 'public' | 'advanced' | 'internal';",
        "export type ParameterValue = number | boolean | string | number[] | number[][];",
        '',
        'export type FieldDef = {',
        '  name: string;',
        "  type: 'float' | 'int' | 'bool' | 'enum' | 'buffer' | 'float_matrix';",
        '  required: boolean;',
        '  default: ParameterValue;',
        '  min?: number;',
        '  max?: number;',
        "  unit?: 'hz' | 'ms' | 'db' | 'ratio' | 'samples';",
        "  scale?: 'linear' | 'logarithmic';",
        '  realtime: boolean;',
        '  smoothingMs?: number;',
        '  structural: boolean;',
        '  options?: string[];',
        '  optionValues?: number[];',
        '};',
        '',
        'export type AtomDef = {',
        '  name: string;',
        "  category: $category_union;",
        '  visibility: AtomVisibility;',
        "  dispatch: 'process' | 'fft' | 'ifft' | 'multiply' | 'window' | 'overlap_add' | 'overlap_save' | 'stream';",
        '  ins: string[];',
        '  outs: string[];',
        '  config: FieldDef[];',
        '};',
        '',
        'export const ATOM_CATALOG: AtomDef[] = [',
    );
    for my $atom (@$atoms) {
        my $input = profile_catalog_fields($profile_by_name{$atom->{input_profile}});
        my $output = profile_catalog_fields($profile_by_name{$atom->{output_profile}});
        my $config = catalog_config_fields($atom);
        my @config_items = map {
            my $metadata = $_->{metadata};
            my @parts = (
                'name: ' . json_literal($_->{name}),
                'type: ' . json_literal($metadata->{type}),
                'required: ' . ($_->{required} ? 'true' : 'false'),
                'default: ' . json_literal($metadata->{default}),
            );
            push @parts, 'min: ' . $metadata->{min} if exists $metadata->{min};
            push @parts, 'max: ' . $metadata->{max} if exists $metadata->{max};
            push @parts, 'unit: ' . json_literal($metadata->{unit}) if exists $metadata->{unit};
            push @parts, 'scale: ' . json_literal($metadata->{scale}) if exists $metadata->{scale};
            push @parts, 'realtime: ' . ($metadata->{realtime} ? 'true' : 'false');
            push @parts, 'smoothingMs: ' . $metadata->{smoothing_ms} if exists $metadata->{smoothing_ms};
            push @parts, 'structural: ' . ($metadata->{structural} ? 'true' : 'false');
            push @parts, 'options: ' . json_literal($metadata->{options}) if exists $metadata->{options};
            push @parts, 'optionValues: ' . json_literal($metadata->{option_values})
                if exists $metadata->{option_values};
            '{ ' . join(', ', @parts) . ' }'
        } @$config;
        my $dispatch = lc($atom->{dispatch});
        push @lines, '  {',
            "    name: '$atom->{name}',",
            "    category: '$atom->{category}',",
            "    visibility: '$atom->{visibility}',",
            "    dispatch: '$dispatch',",
            '    ins: ' . ts_quote_list([map { $_->{name} } @$input]) . ',',
            '    outs: ' . ts_quote_list([map { $_->{name} } @$output]) . ',',
            '    config: [' . join(', ', @config_items) . '],',
            '  },';
    }
    push @lines, (
        '];',
        '',
        "export const PUBLIC_ATOM_CATALOG = ATOM_CATALOG.filter(atom => atom.visibility === 'public');",
        "export const ADVANCED_ATOM_CATALOG = ATOM_CATALOG.filter(atom => atom.visibility === 'advanced');",
        '',
        'export const ATOM_MAP = new Map<string, AtomDef>(ATOM_CATALOG.map(atom => [atom.name, atom]));',
    );
    return join("\n", @lines) . "\n";
}

sub json_value_schema {
    my ($field) = @_;
    my $type = $field->{type};
    my $schema;
    $schema = {type => 'string'} if $type eq 'signal' || $type eq 'signal_optional' || $type eq 'buffer';
    $schema = {type => 'number'} if $type eq 'float' || $type eq 'scalar';
    $schema = {type => 'integer'} if $type eq 'int';
    $schema = {type => 'array', minItems => 1, items => {type => 'string'}} if $type eq 'signal_array';
    $schema = {
        type => 'array', minItems => 1,
        items => {type => 'array', minItems => 1, items => {type => 'number'}},
    } if $type eq 'float_matrix';
    die "cannot map contract type $type to JSON Schema\n" unless $schema;

    my $metadata = $field->{metadata};
    return $schema unless $metadata;
    $schema->{default} = $metadata->{default};
    $schema->{minimum} = $metadata->{min} if exists $metadata->{min};
    $schema->{maximum} = $metadata->{max} if exists $metadata->{max};
    $schema->{enum} = $metadata->{option_values} if exists $metadata->{option_values};
    $schema->{'x-apg-field-type'} = $metadata->{type};
    $schema->{'x-apg-unit'} = $metadata->{unit} if exists $metadata->{unit};
    $schema->{'x-apg-scale'} = $metadata->{scale} if exists $metadata->{scale};
    $schema->{'x-apg-realtime'} = $metadata->{realtime};
    $schema->{'x-apg-smoothing-ms'} = $metadata->{smoothing_ms} if exists $metadata->{smoothing_ms};
    $schema->{'x-apg-structural'} = $metadata->{structural};
    $schema->{'x-apg-options'} = $metadata->{options} if exists $metadata->{options};
    return $schema;
}

sub json_section_schema {
    my ($fields) = @_;
    my %properties = map { $_->{name} => json_value_schema($_) } @$fields;
    my @required = map { $_->{name} } grep { $_->{required} } @$fields;
    my $schema = {
        type => 'object',
        additionalProperties => JSON::PP::false,
        properties => \%properties,
    };
    $schema->{required} = \@required if @required;
    return $schema;
}

sub render_json_schema {
    my @one_of;
    for my $atom (@$atoms) {
        my $input = profile_catalog_fields($profile_by_name{$atom->{input_profile}});
        my $output = profile_catalog_fields($profile_by_name{$atom->{output_profile}});
        my $config = catalog_config_fields($atom);
        push @one_of, {
            title => $atom->{name},
            type => 'object',
            additionalProperties => JSON::PP::false,
            properties => {
                atom => {const => $atom->{name}},
                in => json_section_schema($input),
                out => json_section_schema($output),
                config => json_section_schema($config),
            },
            required => ['atom'],
            'x-apg-category' => $atom->{category},
            'x-apg-dispatch' => lc($atom->{dispatch}),
            'x-apg-visibility' => $atom->{visibility},
        };
    }
    my $schema = {
        '$schema' => 'https://json-schema.org/draft/2020-12/schema',
        '$id' => 'https://audio-playground.dev/schema/atom.schema.json',
        title => 'Audio Playground Atom Binding',
        description => "Generated atom binding contracts from $source.",
        oneOf => \@one_of,
    };
    return JSON::PP->new->canonical(1)->pretty(1)->encode($schema);
}

my %outputs;
$outputs{'inc/atom/types/dsp_type_macros.h'} = render_type_macros_header();
for my $family (@$families) {
    $outputs{$family->{header}} = render_family_header($family);
    $outputs{$family->{descriptor_source}} = render_field_descriptors($family);
}
$outputs{'inc/atom/generated/atom_definitions.generated.h'} = render_atom_definitions();
$outputs{'inc/atom/generated/dsp_atoms.generated.h'} = render_dsp_declarations();
$outputs{'src/apgcore/metadata/atom_catalog_contracts.generated.inc'} = render_catalog_contracts();
$outputs{'web-tools/unit-editor/src/atoms/atomCatalog.generated.ts'} = render_typescript_catalog();
$outputs{'schema/atoms/atom.schema.json'} = render_json_schema();

my @stale;
for my $relative (sort keys %outputs) {
    my $path = File::Spec->catfile($output_root, split m{/}, $relative);
    if ($check) {
        if (!-f $path || read_text($path) ne $outputs{$relative}) {
            push @stale, $relative;
        }
        next;
    }
    my $directory = dirname($path);
    make_path($directory) unless -d $directory;
    open my $file, '>', $path or die "cannot write $path: $!\n";
    print {$file} $outputs{$relative};
    close $file or die "cannot close $path: $!\n";
}

if (@stale) {
    die "generated atom artifacts are stale:\n  " . join("\n  ", @stale) . "\n";
}
