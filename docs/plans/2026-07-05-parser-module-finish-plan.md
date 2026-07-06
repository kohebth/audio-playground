# Parser Module Finish Plan

## Goal

Parser converts YAML source into a raw contract graph only. It must not perform semantic validation.

## Current Status

Complete and re-audited against the current core boundary.

Done:

- `apg_v2_parse_string(...)` and `apg_v2_parse_file(...)` emit raw YAML contract graphs.
- Parser tests prove unknown atoms, invalid routes, unsupported profiles, and other semantic-invalid data can still parse when YAML syntax is valid.
- Parser rejects malformed YAML syntax.

## Remaining Implementation

- [x] Do not add atom, route, compatibility, param, or binding semantics to parser.
- [x] Keep `apg_unit_v2_load_*` and `apg_project_v2_load_*` as parser-then-validator wrappers.
- [x] If loader code grows, move semantic checks back into validator.

## Tests

- Keep parser boundary tests for semantic-invalid but syntactically valid unit/project YAML.
- Keep malformed YAML negative tests in parser coverage.

## Exit Criteria

- Parser remains syntax-only.
- Loader wrappers stay thin and predictable.
- Parser tests fail if semantic checks leak into parsing.
