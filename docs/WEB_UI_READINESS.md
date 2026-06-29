# Web UI Readiness Checklist

This checklist defines what must be true before the v2 web UI becomes the main workstream. The goal is to give the frontend stable backend contracts for visual editing, validation, preview, and export.

## Current Backend Status

- APGCore v2 loader, compiler, scheduler, runtime MVP, fixtures, host bridge, control-to-param routing, atom catalog export, project schema validation, and resolved project unit loading are implemented through Phase T.
- `unit.v2.yaml` is executable and tested, and optional unit/param UI metadata is parsed and validated.
- Reusable unit fixtures exist in `units-v2/`.
- Project/session schema, deterministic fixtures, and referenced-unit resolution exist; project compilation is still missing.
- Atom catalog JSON is available through the APGCore catalog writer; unit inspection, project inspection, and structured validation output are still missing.

## Ready To Start Web UI When

- [x] Unit metadata can render a parameter panel without frontend hardcoding.
- [x] Atom catalog metadata can render an atom palette without frontend hardcoding.
- [x] Project files can reference units and define routes between unit instances at the schema-validation level.
- [ ] Validation returns structured errors and warnings with stable file/path fields.
- [ ] A guitar pedalboard fixture validates, compiles, runs, and renders deterministically.
- [ ] Runtime supports the first live UI controls: parameter changes, bypass, and meters.
- [ ] Sample JSON outputs are committed for frontend tests and UI mock data.

## Backend Contracts Needed

### Unit Inspect Contract

The UI needs a structured view of one unit:

- `name`, `version`, `meta`, `compatibility`
- public params with defaults, ranges, smoothing, and UI hints
- input/output ports and channel layout
- graph nodes and binding errors for unit-internals editing

### Atom Catalog Contract

The UI needs a structured atom palette:

- atom name and category
- input, output, config, and state fields
- supported target profiles
- constraints such as fixed-size or stateful behavior

### Project Contract

The UI needs a full pedalboard/session model:

- unit references
- unit instances
- routes between instances
- scenes and preset parameter values
- target backend settings

### Validation Contract

The UI needs stable diagnostics:

```json
{
  "ok": false,
  "errors": [
    {
      "code": "APG_ATOM_UNKNOWN",
      "file": "units-v2/example.unit.v2.yaml",
      "path": "graph.nodes[2].atom",
      "message": "Unknown atom 'filter_biquadd'"
    }
  ],
  "warnings": []
}
```

### Runtime Preview Contract

The UI needs a way to drive live or offline preview:

- set parameter by stable instance path
- bypass unit instance
- read meters
- compile/swap a valid graph without replacing active audio on failure

## Phase Gates

- **Phase Q:** Unit schema validates UI metadata.
- **Phase R:** Atom catalog is exportable.
- **Phase S:** Project schema and fixtures exist.
- **Phase T:** Project loader resolves multi-file units safely. Complete.
- **Phase U:** Project compiler creates a single runtime plan.
- **Phase V:** CLI or equivalent tooling emits JSON inspect/validate output.
- **Phase W:** Runtime supports product controls and meters.
- **Phase X:** Guitar pedalboard fixture proves the workflow.
- **Phase Y:** Web handoff package freezes sample contracts.

## First Web UI Scope After Gate

Start with the actual product workflow, not a landing page:

- project browser using committed sample projects
- pedalboard canvas for unit instances and routes
- inspector for validation errors and compatibility
- parameter panel from unit metadata
- atom-level unit editor after project-level workflow is usable
