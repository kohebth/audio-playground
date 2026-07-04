import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, '../../..');

function read(path) {
  return readFileSync(resolve(repo, path), 'utf8');
}

function json(path) {
  return JSON.parse(read(path));
}

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

function includes(path, text, message) {
  assert(read(path).includes(text), message);
}

function includesContent(content, text, message) {
  assert(content.includes(text), message);
}

function assertAtLeast(content, needle, minimum, message) {
  const count = (content.match(new RegExp(needle, 'g')) ?? []).length;
  assert(count >= minimum, `${message} (found ${count}, need ${minimum})`);
}

const atomCatalog = json('test/golden/v2-inspect-atoms.json');
const project = json('test/golden/v2-inspect-project-guitar-pedalboard.json');
const unit = json('test/golden/v2-inspect-unit-simple_gain.json');
const render = json('test/golden/v2-render-project-guitar-pedalboard.json');

const app = read('web-tools/unit-editor/src/App.tsx');
const backendSamples = read('web-tools/unit-editor/src/lib/backendSamples.ts');
const projectTopbar = read('web-tools/unit-editor/src/components/ProjectTopbar.tsx');
const projectInspector = read('web-tools/unit-editor/src/components/ProjectInspector.tsx');
const unitGraph = read('web-tools/unit-editor/src/components/UnitGraphEditor.tsx');
const atomPalette = read('web-tools/unit-editor/src/components/AtomCatalogPanel.tsx');
const contractCanvas = read('web-tools/unit-editor/src/components/ContractGraphCanvas.tsx');
const previewPanel = read('web-tools/unit-editor/src/components/PreviewPanel.tsx');
const previewAdapter = read('web-tools/unit-editor/src/lib/previewAdapter.ts');
const compatibility = read('web-tools/unit-editor/src/components/CompatibilityExportPanel.tsx');

// AC: Contract-accurate web data is sourced from a frozen backend atom catalog fixture.
assert(atomCatalog.schema === 'apg.atom_catalog.v1', 'atom catalog fixture schema changed');
assert(atomCatalog.atoms.length > 40, 'atom catalog fixture is not the full backend catalog');
assert(
  atomCatalog.atoms.some(
    atom =>
      atom.name === 'generation_dc' &&
      atom.outputs.some(field => field.name === 'signal' && field.type === 'signal') &&
      atom.config.some(field => field.name === 'value' && field.type === 'scalar'),
  ),
  'atom catalog lacks generation_dc binding metadata',
);
assert(
  /\"schema\"\s*:\s*\"apg.atom_catalog.v1\"/.test(read('test/golden/v2-inspect-atoms.json')),
  'atom catalog schema assertion should fail visibly if fixture schema drifts',
);
includes(
  'web-tools/unit-editor/src/lib/backendSamples.ts',
  'v2-inspect-atoms.json?raw',
  'backendSamples must load the frozen backend atom catalog JSON',
);
assert(!backendSamples.includes('../atoms/atomCatalog'), 'project workbench backend samples must not use the local atom catalog fallback');
includes('web-tools/unit-editor/src/components/ProjectInspector.tsx', '<strong>{atomCatalog.schema}</strong>', 'contract view must expose atom catalog schema');
includesContent(atomPalette, 'catalog.atoms.map(', 'atom palette must render from backend atoms');
includesContent(atomPalette, 'aria-label="Atom palette"', 'atom palette should expose contract-backed rendering');
includesContent(atomPalette, 'unit.graph.nodes.map', 'unit inspect graph should drive contract view details');
assert(
  /localStorage\.getItem\(WORKSPACE_STORAGE_KEY\)/.test(app),
  'workspace autosave restore must read from localStorage',
);
includes(
  'web-tools/unit-editor/src/lib/backendSamples.ts',
  'test/golden/v2-inspect-atoms.json',
  'frozen backend contract source path should be explicit',
);

// AD: Workspace and autosave behavior remains draft-driven.
assert(project.units.length === 6, 'pedalboard workspace fixture should include six referenced units');
assert(
  project.routes.some(route => route.from === 'trem1.output' && route.to === 'blend1.dry'),
  'pedalboard route graph fixture changed',
);
assert((backendSamples.match(/role: 'unit'/g) ?? []).length === project.units.length, 'workspace bundle must include all referenced unit files');
includesContent(app, 'apg.unit-editor.workspace.v1', 'workspace autosave key is missing');
includesContent(app, "schema: 'apg.ui.workspace.v1'", 'workspace export schema is missing');
includesContent(app, 'JSON.parse(saved)', 'autosave restores JSON payloads from localStorage');
includesContent(
  app,
  '.map(({ path, role, content }) => ({ path, role, content }))',
  'workspace export should include every tracked file as path/role/content',
);
includesContent(projectTopbar, 'Drafts pending', 'dirty workspace state must be visible');
includesContent(projectTopbar, 'Backend synced', 'clean workspace state must be visible');
includesContent(projectInspector, 'Out of sync with local edits', 'validation/render should show stale state');
includesContent(projectInspector, 'Synchronized with local draft state', 'validation/render should show synced state');
includesContent(projectInspector, 'commandState = hasDirtyParamDrafts ? \'frozen\' : \'current\'', 'backend command path should switch when workspace is dirty');
assert(unit.graph.nodes.length > 0 && unit.graph.signals.includes('input'), 'unit inspect graph fixture is empty');

// AE: Contract editor applies graph edits against unit drafts and surfaces binding errors.
assert(unitGraph.length > 0 && /TODO_\$\{field\.type\.toUpperCase\(\)\}/.test(unitGraph), 'unit graph insertion should still use typed backend metadata');
includesContent(projectInspector, 'Graph edit blocked', 'invalid atom binding feedback is missing');
includesContent(projectInspector, 'onAddAtom', 'contract graph view should expose insert/add action');
includesContent(projectInspector, 'onCopyAtom', 'contract graph view should expose copy action');
includesContent(projectInspector, 'onCutAtom', 'contract graph view should expose cut action');
includesContent(projectInspector, 'onPasteAtom', 'contract graph view should expose paste action');
includesContent(app, 'serializeUnitGraphNodeUpdate(content, node, originalId)', 'atom config edits should update draft YAML');
includesContent(app, 'onOpenAtomInspector', 'double-click contract nodes should switch to Contract inspector');
includesContent(contractCanvas, 'onNodeDoubleClick', 'contract node interaction should open atom inspector');
includesContent(contractCanvas, 'onNodeClick', 'contract node click should select atom');
includesContent(app, 'setGraphEditError', 'graph edit failures should be surfaced through workspace error feedback');

// AF: Preview adapter and panel contract state machine.
assert(render.ok && render.frames === render.output.samples.length, 'render fixture must include deterministic samples');
assert(render.output.samples.length > 0, 'render fixture should include sample data');
includesContent(
  previewAdapter,
  'type PreviewState = \'idle\' | \'ready\' | \'running\' | \'error\';',
  'preview adapter should define the full preview state machine',
);
includesContent(previewAdapter, "compile: () => (render.ok ? 'ready' : 'error')", 'preview compile should transition to ready/error');
includesContent(previewAdapter, "start: () => (render.ok ? 'running' : 'error')", 'preview start should transition to running/error');
includesContent(previewAdapter, "stop: () => (render.ok ? 'ready' : 'error')", 'preview stop should transition to ready/error');
includesContent(previewAdapter, 'setParam: (path, value) => `setParam ${path}=${value}`', 'preview adapter setParam command name changed');
includesContent(previewAdapter, 'setBypass: (instanceId, enabled) => `setBypass ${instanceId}=${enabled}`', 'preview adapter setBypass command name changed');
includesContent(previewPanel, "useState<PreviewState>('idle')", 'preview panel should track adapter state');
includesContent(previewPanel, 'adapter.compile()', 'preview compile transition should be wired');
includesContent(previewPanel, 'adapter.start()', 'preview start transition should be wired');
includesContent(previewPanel, 'adapter.stop()', 'preview stop transition should be wired');
includesContent(previewPanel, 'WASM AudioWorklet preview backend is pending', 'missing-backend state is not visible');
includesContent(previewPanel, 'Preview command', 'preview command echo should be visible');

// AG: Compatibility and export actionability.
for (const profile of ['desktop_full', 'wasm_realtime', 'm7_static', 'offline_render']) {
  assert(compatibility.includes(profile), `compatibility matrix missing ${profile}`);
}
assert(compatibility.includes("'yes'") && compatibility.includes("'no'"), 'compatibility matrix must show both supported and unsupported states');
includesContent(compatibility, 'commands.exportWasm', 'WASM export command is not surfaced');
includesContent(compatibility, 'commands.exportM7', 'M7 export command is not surfaced');
includesContent(compatibility, 'commands.benchmarkProject', 'benchmark command is not surfaced');
assertAtLeast(compatibility, '<strong>blocked</strong>', 2, 'export panel should show multiple blocked targets with reason');
assert(compatibility.includes('Blocked:'), 'export action panel should expose blocking reason text for unavailable targets');

console.log('web contract tests passed');
