#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename qw(dirname);
use File::Path qw(make_path);
use JSON::PP qw(decode_json);

sub usage {
    die "usage: $0 <family-schema.json> <output-header>\n";
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

my ($schema_path, $output_path) = @ARGV;
usage() unless defined($schema_path) && defined($output_path) && @ARGV == 2;

my $document = decode_json(read_text($schema_path));
require_hash($document, 'schema');
die "unsupported DSP family schema\n" unless ($document->{'$schema'} // '') eq 'apg.dsp-type-family.v1';

my $source = require_name($document->{source}, 'source', qr{\Aschema/atoms/[a-z0-9_]+\.json\z});
my $category = require_name($document->{category}, 'category', qr/\A[a-z][a-z0-9_]*\z/);
my $header = require_hash($document->{header}, 'header');
my $guard = require_name($header->{guard}, 'header.guard', qr/\A[A-Z][A-Z0-9_]*\z/);
my $include = require_name($header->{include}, 'header.include', qr{\Aatom/types/[a-z0-9_]+\.h\z});
my $table_macro = require_name($header->{table_macro}, 'header.table_macro', qr/\AAPG_[A-Z0-9_]+\z/);

my $empty = require_hash($document->{empty_layout}, 'empty_layout');
my $empty_type = require_name($empty->{c_type}, 'empty_layout.c_type', qr/\Auint8_t\z/);
my $empty_name = require_name($empty->{name}, 'empty_layout.name', qr/\A_[a-z][a-z0-9_]*\z/);
my $atoms = require_array($document->{atoms}, 'atoms');
die "atoms must not be empty\n" unless @$atoms;

my %allowed_c_type = map { $_ => 1 } ('float', 'int', 'uint32_t', 'float *', 'float **');
my %allowed_metadata = map { $_ => 1 } ('float', 'int', 'buffer', 'signal', 'signal_array', 'float_matrix');
my %allowed_ownership = map { $_ => 1 } ('value', 'borrowed', 'runtime_owned', 'external');
my %allowed_dispatch = map { $_ => 1 } ('PROCESS', 'FFT', 'IFFT', 'MULTIPLY', 'WINDOW', 'OVERLAP_ADD', 'OVERLAP_SAVE');
my %allowed_capability = map { $_ => 1 } ('PORTABLE', 'WASM', 'WASM_ANTIALIASED', 'EXPERIMENTAL');
my %allowed_maturity = map { $_ => 1 } ('SAFE_SCALAR', 'MUSICAL', 'EXPERIMENTAL');

sub validate_fields {
    my ($fields, $label) = @_;
    require_array($fields, $label);
    my %seen;
    for my $field (@$fields) {
        require_hash($field, "$label field");
        my $name = require_name($field->{name}, "$label field name", qr/\A[a-z][a-z0-9_]*\z/);
        die "$label contains duplicate field $name\n" if $seen{$name}++;
        my $c_type = $field->{c_type} // '';
        my $metadata = $field->{metadata_type} // '';
        my $ownership = $field->{ownership} // '';
        die "$label field $name has unsupported C type\n" unless $allowed_c_type{$c_type};
        die "$label field $name has unsupported metadata type\n" unless $allowed_metadata{$metadata};
        die "$label field $name has unsupported ownership\n" unless $allowed_ownership{$ownership};
        if ($c_type =~ /\*/ && $ownership eq 'value') {
            die "$label pointer field $name requires explicit non-value ownership\n";
        }
        if ($c_type !~ /\*/ && $ownership ne 'value') {
            die "$label scalar field $name must use value ownership\n";
        }
        if (exists $field->{capacity}) {
            die "$label field $name capacity must be a non-negative integer\n"
                unless $field->{capacity} =~ /\A[0-9]+\z/;
        }
    }
}

my %seen_atom;
my @atom_names;
for my $atom (@$atoms) {
    require_hash($atom, 'atom');
    my $name = require_name($atom->{name}, 'atom.name', qr/\A[a-z][a-z0-9_]*\z/);
    die "atom $name does not belong to category $category\n" unless $name =~ /^\Q$category\E_/;
    die "duplicate atom $name\n" if $seen_atom{$name}++;
    push @atom_names, $name;
    require_name($atom->{output_profile}, "$name output_profile", qr/\A[A-Z][A-Z0-9_]*\z/);
    require_name($atom->{input_profile}, "$name input_profile", qr/\A[A-Z][A-Z0-9_]*\z/);
    die "$name has unsupported dispatch\n" unless $allowed_dispatch{$atom->{dispatch} // ''};
    die "$name has unsupported capability profile\n"
        unless $allowed_capability{$atom->{capability_profile} // ''};
    die "$name has unsupported maturity\n" unless $allowed_maturity{$atom->{maturity} // ''};
    validate_fields($atom->{params}, "$name params");
    validate_fields($atom->{state}, "$name state");

    my $registry = require_hash($atom->{registry}, "$name registry");
    for my $key (qw(input_fields config_fields state_fields)) {
        die "$name registry.$key must be a non-negative integer\n"
            unless defined($registry->{$key}) && $registry->{$key} =~ /\A[0-9]+\z/;
    }
    die "$name registry config count differs from params\n"
        unless $registry->{config_fields} == @{$atom->{params}};
    die "$name registry state count differs from state\n"
        unless $registry->{state_fields} == @{$atom->{state}};
}

my @sorted_names = sort @atom_names;
die "atoms must be sorted by name for deterministic output\n" unless join("\n", @atom_names) eq join("\n", @sorted_names);

sub render_fields {
    my ($fields) = @_;
    return "{ $empty_type $empty_name; }" unless @$fields;
    my @members;
    for my $field (@$fields) {
        my $c_type = $field->{c_type};
        my $separator = $c_type =~ /\*\z/ ? '' : ' ';
        push @members, "$c_type$separator$field->{name};";
    }
    return '{ ' . join(' ', @members) . ' }';
}

my @body = (
    "#ifndef $guard",
    "#define $guard",
    '',
    "#include <$include>",
    '',
    '// clang-format off',
    "#define $table_macro(X) \\",
);

for my $index (0 .. $#$atoms) {
    my $atom = $atoms->[$index];
    my $continuation = $index == $#$atoms ? '' : ' \\';
    push @body, sprintf(
        '    X(%s, %s, %s, %s, %s)%s',
        $atom->{name},
        $atom->{output_profile},
        $atom->{input_profile},
        render_fields($atom->{params}),
        render_fields($atom->{state}),
        $continuation,
    );
}

push @body, (
    '// clang-format on',
    '',
    "$table_macro(APG_DECLARE_DSP_TYPES)",
    '',
    "#endif // $guard",
);

my $output_dir = dirname($output_path);
make_path($output_dir) unless -d $output_dir;
open my $output, '>', $output_path or die "cannot write $output_path: $!\n";
print {$output} "/* Generated candidate from $source; do not edit this output. */\n";
print {$output} join("\n", @body), "\n";
close $output or die "cannot close $output_path: $!\n";
