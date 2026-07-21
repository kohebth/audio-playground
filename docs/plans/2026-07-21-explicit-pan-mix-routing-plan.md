# Explicit Pan/Mix Routing Plan

Status: implemented on 2026-07-21. Native and browser contracts, strict validation, guided editing, legacy migration,
always-active faders, fixtures, and focused regression tests are complete.

## Goal

Make every project-level split and merge explicit, safe, and editable by surrounding parallel paths with always-active
routing units. Atom graphs are unchanged.

## Contract

- Unit metadata may declare `routing.role: panner|mixer` and an ordered `routing.paths` list. Each path binds one audio
  port to one dB level parameter.
- Project helper instances declare `routing.section`. A section contains exactly one panner and one mixer with the same
  ordered paths.
- The shipped system library provides only two-path helpers (`path_panner_2` and `path_mixer_2`), while the metadata
  shape remains extensible for future N-way helpers.
- Routing helpers are always active: they cannot be bypassed directly, in scenes, or through batch actions.

## DSP

- Add the portable internal `amplitude_gain_db` atom. It converts dB with `10^(dB/20)`, accepts `-120..+24 dB`,
  sanitizes non-finite values, and respects the process frame count.
- `path_panner_2` has one input, two independently controlled outputs, `0 dB` defaults, and 10 ms parameter smoothing.
- `path_mixer_2` has two independently controlled inputs, one summed output, `-6.0206 dB` defaults, and 10 ms
  parameter smoothing. Public helper controls expose `-60..+6 dB`.

## Validation and editing

- Raw source fan-out is rejected with an actionable “Add in parallel” error.
- Ordinary connected project effects must have one mono audio input and one mono audio output.
- Each panner path must be used once and reach the same-named input on its section mixer. Crossed, leaking, orphaned,
  cyclic, and incomplete paths are rejected. Nested sections are validated recursively as serial macros.
- The guided route transaction replaces one route with a panner, a dry branch, an effect branch, and a mixer. It is
  available in Simple and Pro mode and remains a single undoable edit.
- Routing helper cards remain visible and expose two vertical keyboard-accessible dB faders. Their layout remains
  deterministic and locked.
- Remove, cut, paste, and replace remain regular actions, but the completed transaction is accepted only when the
  resulting routing topology is valid. Replacement must preserve routing role and path contract.

## Browser migration

- On Studio open/import, recognize the existing unambiguous raw-fanout plus `wet_dry_mix` topology.
- Reuse the mixer instance id, add a panner and section, map dry/wet to path 1/path 2, and translate mix ratios to
  independent dB levels for node parameters and scenes.
- The migration is idempotent and persists once. Ambiguous legacy graphs remain unchanged and surface the strict
  validation error. Native/API loading stays strict and performs no content migration.

## Verification

- Atom numeric, finite, frame-limit, catalog, and profile tests.
- Unit routing metadata and project serial, empty, split, nested, mismatch, orphan, raw fan-out, scene bypass, compile,
  and runtime tests.
- TypeScript transform, validation, migration, idempotence, undo/redo, reload/import, fader, bypass, and interaction
  tests, followed by native and web build/lint/typecheck/test gates.

Completed gates:

- `./build-and-test.sh` — 72/72 CTest targets passed.
- `cd web-tools && npm run typecheck`
- `cd web-tools && npm run lint`
- `cd web-tools && npm test && npm run build`
