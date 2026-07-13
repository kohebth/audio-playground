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
const projectSidebar = read('web-tools/unit-editor/src/components/ProjectSidebar.tsx');
const projectInspector = read('web-tools/unit-editor/src/components/ProjectInspector.tsx');
const atomPalette = read('web-tools/unit-editor/src/components/AtomCatalogPanel.tsx');
const contractCanvas = read('web-tools/unit-editor/src/components/ContractGraphCanvas.tsx');
const previewPanel = read('web-tools/unit-editor/src/components/PreviewPanel.tsx');
const wasmFacade = read('wasm-tools/web/facade.ts');
const processorWorklet = read('wasm-tools/web/processor.worklet.js');
const compatibility = read('web-tools/unit-editor/src/components/CompatibilityExportPanel.tsx');

// AC: Contract-accurate web data is sourced from a frozen backend atom catalog fixture.
assert(atomCatalog.schema === 'apg.atom_catalog.v2', 'atom catalog fixture schema changed');
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
  /\"schema\"\s*:\s*\"apg.atom_catalog.v2\"/.test(read('test/golden/v2-inspect-atoms.json')),
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
includesContent(projectInspector, 'Graph edit blocked', 'invalid atom binding feedback is missing');
includesContent(projectInspector, 'onAddAtom', 'contract graph view should expose insert/add action');
includesContent(projectInspector, 'onCopyAtom', 'contract graph view should expose copy action');
includesContent(projectInspector, 'onCutAtom', 'contract graph view should expose cut action');
includesContent(projectInspector, 'onPasteAtom', 'contract graph view should expose paste action');
includesContent(app, 'serializeUnitGraphNodeUpdate(content, node, originalId)', 'atom config edits should update draft YAML');
includesContent(app, 'onOpenAtomInspector', 'double-click contract nodes should switch to Contract inspector');
includesContent(contractCanvas, 'onNodeDoubleClick', 'contract node interaction should open atom inspector');
includesContent(contractCanvas, 'onNodeClick', 'contract node click should select atom');
includesContent(contractCanvas, 'onConnect={connect}', 'contract handles must create structured YAML connections');
includesContent(contractCanvas, 'onEdgesDelete={deleteEdges}', 'contract edges must support structural disconnection');
includesContent(contractCanvas, 'onEdgesChange={onEdgesChange}', 'controlled contract edges must apply delete and reconnect events');
includesContent(contractCanvas, 'onReconnect={reconnect}', 'contract edges must support structural moves');
includesContent(contractCanvas, 'onNodesChange={onNodesChange}', 'contract node positions must remain UI-only state');
includesContent(app, 'setGraphEditError', 'graph edit failures should be surfaced through workspace error feedback');
includesContent(app, 'resolveWorkspacePath(backendSamples.project.file, node.unit.file)', 'unit references must resolve against the project path');
includesContent(app, 'createUnitV2({ name })', 'workspace unit creation must use the structured YAML transformer');
includesContent(projectSidebar, 'onCreateUnit(unitName)', 'workspace sidebar must expose unit creation');

// AF: WASM preview facade and panel contract state machine.
assert(render.ok && render.frames === render.output.samples.length, 'render fixture must include deterministic samples');
assert(render.output.samples.length > 0, 'render fixture should include sample data');
includesContent(previewPanel, 'WasmBackend.create', 'preview must initialize the typed WASM facade');
includesContent(previewPanel, 'backend.replaceWorkspace', 'workspace revisions must be sent to WASM validation');
includesContent(previewPanel, 'backend.prepare', 'valid revisions must prepare a runtime image');
includesContent(previewPanel, 'window.setTimeout', 'workspace synchronization must be debounced');
includesContent(previewPanel, 'revision !== revisionRef.current', 'stale validation results must be ignored');
includesContent(previewPanel, 'navigator.mediaDevices.getUserMedia', 'live preview must support microphone input');
includesContent(previewPanel, 'decodeAudioData', 'live preview must decode uploaded audio files');
includesContent(previewPanel, 'createBufferSource', 'uploaded files must use a WebAudio buffer source');
includesContent(previewPanel, "type InputMode = 'file' | 'microphone'", 'file and microphone transports must remain separate');
includesContent(previewPanel, 'backend.reset()', 'live preview reset must use the WASM backend');
includesContent(previewPanel, 'backend.setParam', 'live parameter controls must use the WASM backend');
includesContent(previewPanel, 'previousOverridesRef', 'live parameter synchronization must detect reset values');
includesContent(previewPanel, 'override.originalValue', 'removed overrides must restore the original runtime value');
includesContent(projectInspector, 'clampValue(parsed, minValue, maxValue)', 'typed parameter values must clamp to metadata bounds');
includesContent(app, 'return changed ? next : files', 'equivalent parameter serialization must not create a new revision');
assert(
  projectInspector.indexOf('<PreviewPanel') < projectInspector.indexOf('{isProjectView && ('),
  'live preview must remain mounted across inspector views',
);
includesContent(previewPanel, 'backend.setBypass', 'live bypass controls must use the WASM backend');
includesContent(previewPanel, 'bypassByInstance', 'bypass UI state must be tracked per project instance');
includesContent(previewPanel, '.pollMeters()', 'preview meters must use throttled Worklet polling');
includesContent(wasmFacade, 'audioWorklet.addModule(this.options.processorWorkletUrl)', 'facade must load the explicit Worklet module');
includesContent(wasmFacade, 'fetch(this.options.processorWasmUrl)', 'processor WASM must be fetched outside the audio callback');
includesContent(wasmFacade, 'processorOptions: { moduleUrl: this.options.processorModuleUrl, wasmBinary }', 'WASM bytes must be transferred during Worklet construction');
includesContent(wasmFacade, 'setCurrentRevision(revision: number)', 'editor revisions must invalidate stale async work immediately');
includesContent(wasmFacade, 'Prepared revision ${revision} is stale', 'stale prepared images must be rejected');
includesContent(wasmFacade, "type: 'stage'", 'runtime hydration must be a separate processor message');
includesContent(wasmFacade, "type: 'commit'", 'runtime commit must be a separate processor message');
includesContent(wasmFacade, 'failedRevision: revision', 'runtime failures must identify their workspace revision');
includesContent(wasmFacade, 'bypassShadows', 'bypass controls must survive prepared runtime swaps');
includesContent(wasmFacade, 'muteShadow', 'mute control must survive prepared runtime swaps');
includesContent(processorWorklet, "import createApgProcessorModule from './apg_processor.mjs'", 'Worklet must use a static Emscripten import');
includesContent(processorWorklet, 'request.type === "commit"', 'Worklet must commit only through an explicit message');
includesContent(processorWorklet, 'request.type === "pollMeters"', 'meter snapshots must be copied outside process()');
assert(!processorWorklet.slice(processorWorklet.indexOf('process(inputs, outputs)')).includes('this.reply('), 'process() must not allocate and post meter messages');
assert(!processorWorklet.includes('HEAPF32.subarray'), 'process() must not allocate a typed-array view per block');
assert(!processorWorklet.includes('import(moduleUrl)'), 'dynamic import must not run in WorkletGlobalScope');
assert(!previewPanel.includes('createDeterministicPreviewAdapter'), 'deterministic preview adapter must not remain active');

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
