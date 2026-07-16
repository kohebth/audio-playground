#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename qw(dirname);
use File::Path qw(make_path);
use File::Spec;
use JSON::PP qw(decode_json);

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
            };
            next;
        }
        next unless exists $field->{descriptor};
        my $type = $descriptor_contract_type{$field->{descriptor}{field_type}}
            or die "$atom->{name} config field $field->{name} cannot map to a catalog type\n";
        push @result, {name => $field->{name}, type => $type, required => 1};
    }
    return \@result;
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

sub render_catalog_array {
    my ($name, $fields) = @_;
    return () unless @$fields;
    my @lines = ("static const apg_atom_contract_field_t $name\[\] = {");
    for my $field (@$fields) {
        my $macro = $field->{required} ? 'FIELD' : 'FIELD_OPT';
        push @lines, qq{    $macro("$field->{name}", $contract_enum{$field->{type}}),};
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
            '    {"%s", %s, %s, %s},', $atom->{name}, $arrays{inputs}, $arrays{outputs}, $arrays{config}
        );
    }
    push @lines, 'static const apg_catalog_contract_t catalog_contracts[] = {', @rows, '};';
    return join("\n", @lines) . "\n";
}

sub ts_quote_list {
    my ($values) = @_;
    return '[' . join(', ', map { "'$_'" } @$values) . ']';
}

sub ts_config_type {
    my ($contract_type) = @_;
    return 'float' if $contract_type eq 'float' || $contract_type eq 'scalar';
    return $contract_type;
}

sub render_typescript_catalog {
    my @categories = map { $_->{category} } @$families;
    my %seen_category;
    @categories = grep { !$seen_category{$_}++ } @categories;
    my $category_union = join(' | ', map { "'$_'" } @categories);
    my @lines = (
        generated_ts_banner($source),
        '',
        "export type FieldDef = { name: string; type: 'float' | 'int' | 'enum' | 'buffer' | 'float_matrix'; required: boolean; options?: string[] };",
        '',
        'export type AtomDef = {',
        '  name: string;',
        "  category: $category_union;",
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
            sprintf(
                "{ name: '%s', type: '%s', required: %s }",
                $_->{name}, ts_config_type($_->{type}), $_->{required} ? 'true' : 'false'
            )
        } @$config;
        my $dispatch = lc($atom->{dispatch});
        push @lines, '  {',
            "    name: '$atom->{name}',",
            "    category: '$atom->{category}',",
            "    dispatch: '$dispatch',",
            '    ins: ' . ts_quote_list([map { $_->{name} } @$input]) . ',',
            '    outs: ' . ts_quote_list([map { $_->{name} } @$output]) . ',',
            '    config: [' . join(', ', @config_items) . '],',
            '  },';
    }
    push @lines, (
        '];',
        '',
        'export const ATOM_MAP = new Map<string, AtomDef>(ATOM_CATALOG.map(atom => [atom.name, atom]));',
    );
    return join("\n", @lines) . "\n";
}

sub json_value_schema {
    my ($type) = @_;
    return {type => 'string'} if $type eq 'signal' || $type eq 'signal_optional' || $type eq 'buffer';
    return {type => 'number'} if $type eq 'float' || $type eq 'scalar';
    return {type => 'integer'} if $type eq 'int';
    return {type => 'array', minItems => 1, items => {type => 'string'}} if $type eq 'signal_array';
    return {
        type => 'array', minItems => 1,
        items => {type => 'array', minItems => 1, items => {type => 'number'}},
    } if $type eq 'float_matrix';
    die "cannot map contract type $type to JSON Schema\n";
}

sub json_section_schema {
    my ($fields) = @_;
    my %properties = map { $_->{name} => json_value_schema($_->{type}) } @$fields;
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
