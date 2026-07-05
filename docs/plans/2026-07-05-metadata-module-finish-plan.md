# Metadata Module Finish Plan

## Goal

Metadata is the single reference source for atom descriptors, call pointers, contract fields, target profiles, and compatibility hints.

## Current Status

Functionally complete.

Done:

- Atom catalog owns known target profiles and compatibility hints.
- Atom catalog owns typed atom contract fields, field types, and required flags.
- Compiler consumes metadata for binding validation.
- Validator consumes metadata for known profile and atom reference checks.

## Remaining Implementation

- [ ] Do not add new metadata APIs unless compiler/registry cannot finish without them.
- [ ] Keep atom call pointers and registry descriptors isolated from parser and validator.
- [ ] If compatibility rules grow, replace fragile prefix-only logic with explicit atom metadata entries.
- [ ] If new atom fields are added, add catalog tests before consumers depend on them.

## Tests

- `test_atom_catalog` must cover every public metadata query.
- Catalog JSON and compiler validation must agree from the same metadata source.

## Exit Criteria

- Metadata remains the only source for atom contract/profile facts.
- No parser or validator code owns atom field truth.
- No compiler-local atom schema table returns.
