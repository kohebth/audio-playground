import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, '../..');

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

const app = read('web-tools/src/App.tsx');
const backendSamples = read('web-tools/src/lib/backendSamples.ts');
const projectTopbar = read('web-tools/src/components/ProjectTopbar.tsx');
const projectSidebar = read('web-tools/src/components/ProjectSidebar.tsx');
const projectInspector = read('web-tools/src/components/ProjectInspector.tsx');
const projectCanvas = read('web-tools/src/components/ProjectCanvas.tsx');
const projectNode = read('web-tools/src/components/ProjectNode.tsx');
const paramKnob = read('web-tools/src/components/ParamKnob.tsx');
const atomPalette = read('web-tools/src/components/AtomCatalogPanel.tsx');
const contractCanvas = read('web-tools/src/components/ContractGraphCanvas.tsx');
const previewPanel = read('web-tools/src/components/PreviewPanel.tsx');
const liveLatencyBadge = read('web-tools/src/components/LiveLatencyBadge.tsx');
const audioIo = read('web-tools/src/lib/audioIo.ts');
const wasmFacade = read('wasm-tools/web/facade.ts');
const processorWorklet = read('wasm-tools/web/processor.worklet.js');
const compatibility = read('web-tools/src/components/CompatibilityExportPanel.tsx');
const workspacePersistence = read('web-tools/src/lib/workspacePersistence.ts');

// AC: Contract-accurate web data is sourced from a frozen backend atom catalog fixture.
assert(atomCatalog.schema === 'apg.atom_catalog.v2', 'atom catalog fixture schema changed');
assert(atomCatalog.atoms.length > 40, 'atom catalog fixture is not the full backend catalog');
assert(
  atomCatalog.atoms.some(
    atom =>
      atom.name === 'generation_dc' &&
      atom.outputs.some(field => field.name === 'signal' && field.type === 'signal') &&
      atom.config.some(field => field.name === 'value' && field.type === 'float' && field.default === 0),
  ),
  'atom catalog lacks generation_dc binding metadata',
);
assert(
  /\"schema\"\s*:\s*\"apg.atom_catalog.v2\"/.test(read('test/golden/v2-inspect-atoms.json')),
  'atom catalog schema assertion should fail visibly if fixture schema drifts',
);
includes(
  'web-tools/src/lib/backendSamples.ts',
  'v2-inspect-atoms.json?raw',
  'backendSamples must load the frozen backend atom catalog JSON',
);
assert(!backendSamples.includes('../atoms/atomCatalog'), 'project workbench backend samples must not use the local atom catalog fallback');
includes('web-tools/src/components/ProjectInspector.tsx', '<strong>{atomCatalog.schema}</strong>', 'contract view must expose atom catalog schema');
includesContent(atomPalette, 'filteredAtoms.map(', 'atom palette must render its filtered backend atoms');
includesContent(atomPalette, "atom.visibility === 'public'", 'atom palette must show public atoms by default');
includesContent(atomPalette, "atom.visibility === 'advanced'", 'atom palette must make advanced atoms opt-in');
includesContent(atomPalette, 'atom-palette-show-advanced', 'atom palette must expose its advanced visibility control');
includesContent(atomPalette, 'atom.name.toLowerCase().includes(query)', 'atom palette filtering must match atom names');
includesContent(atomPalette, 'atom.category.toLowerCase().includes(query)', 'atom palette filtering must match atom categories');
includesContent(atomPalette, 'aria-label="Atom palette"', 'atom palette should expose contract-backed rendering');
includesContent(atomPalette, 'unit.graph.nodes.map', 'unit inspect graph should drive contract view details');
includesContent(atomPalette, "ATOM_DRAG_TYPE = 'application/x-apg-atom'", 'atom palette must define a drag payload type');
includesContent(atomPalette, 'event.dataTransfer.setData(ATOM_DRAG_TYPE, atom.name)', 'atom palette items must be draggable');
includesContent(atomPalette, 'Add atom', 'atom palette must keep a click fallback for creation');
includesContent(contractCanvas, 'onAddAtomAt(atomName', 'unit graph canvas must create dropped atoms at a pointer position');
includesContent(contractCanvas, 'onInsertAtomAtEdge(atomName', 'edge drops must use the atomic insert transaction');
includesContent(contractCanvas, 'onNodeDragStop', 'unit graph canvas must persist atom moves');
includesContent(contractCanvas, "markComponentRender('ContractEdge'", 'unit graph canvas must expose edge render scope');
includesContent(contractCanvas, 'flow-shell--drop-${dropState}', 'unit graph canvas must expose valid/reject drop feedback');
includesContent(projectSidebar, "UNIT_DRAG_TYPE = 'application/x-apg-unit'", 'unit library must define a drag payload type');
includesContent(projectSidebar, 'event.dataTransfer.setData(UNIT_DRAG_TYPE, unit.id)', 'unit library items must be draggable');
includesContent(projectCanvas, 'onAddUnitAt(unitId', 'project canvas must create dropped units at a pointer position');
includesContent(projectCanvas, 'onInsertUnitAtRoute(unitId', 'project edge drops must use the atomic route insertion transaction');
includesContent(projectCanvas, 'onNodeDragStop', 'project canvas must persist unit moves');
includesContent(projectCanvas, 'flow-shell--drop-${dropState}', 'project canvas must expose valid/reject drop feedback');
includesContent(projectInspector, 'atom-type-lock', 'selected atom type must render as read-only');
assert(!projectInspector.includes('onSelectedAtomChange({ ...selectedAtom, atom:'), 'atom inspector must not directly mutate atom type');
includesContent(projectInspector, 'Replace atom...', 'atom type changes must route through explicit replacement workflow');
includesContent(projectInspector, 'previewAtomReplacement', 'atom replacement must preview affected compatibility');
includesContent(projectInspector, 'Confirm replacement', 'atom replacement must require confirmation');
includesContent(projectInspector, 'Preserve instance ID', 'atom replacement must make ID preservation explicit');
includesContent(app, 'replaceAtomNodeInUnit', 'atom replacement must apply through a dedicated graph transaction');
includesContent(app, 'undoStack', 'workspace graph edits must track undo history');
includesContent(app, 'redoStack', 'workspace graph edits must track redo history');
includesContent(app, 'setAtomNodePosition', 'atom moves must use a YAML-backed transaction');
includesContent(app, 'setProjectInstancePosition', 'unit moves must use a YAML-backed transaction');
includesContent(app, 'persistWorkspacePayload', 'workspace writes must use the testable persistence boundary');
includesContent(projectTopbar, 'workspace-save-status', 'workspace persistence failures must be visible');
includesContent(projectTopbar, 'title="Undo"', 'topbar must expose undo control');
includesContent(projectTopbar, 'title="Redo"', 'topbar must expose redo control');
assert(
  /localStorage\.getItem\(WORKSPACE_STORAGE_KEY\)/.test(app),
  'workspace autosave restore must read from localStorage',
);
includes(
  'web-tools/src/lib/backendSamples.ts',
  'test/golden/v2-inspect-atoms.json',
  'frozen backend contract source path should be explicit',
);

// AD: Workspace and autosave behavior remains draft-driven.
assert(project.units.length === 7, 'pedalboard workspace fixture should include seven referenced units');
assert(
  project.routes.some(route => route.from === 'trem1.output' && route.to === 'blend1.dry'),
  'pedalboard route graph fixture changed',
);
assert((backendSamples.match(/role: 'unit'/g) ?? []).length === project.units.length, 'workspace bundle must include all referenced unit files');
includesContent(app, 'apg.unit-editor.workspace.v2', 'versioned workspace autosave key is missing');
includesContent(app, 'apg.unit-editor.workspace.v1', 'legacy workspace migration key is missing');
includesContent(workspacePersistence, "WORKSPACE_SCHEMA = 'apg.ui.workspace.v2'", 'workspace export schema is missing');
includesContent(workspacePersistence, 'WORKSPACE_FORMAT_VERSION = 2', 'workspace format version is missing');
includesContent(app, 'parseWorkspacePayload(saved)', 'autosave restore must validate versioned workspace payloads');
includesContent(app, 'hydrateWorkspaceFiles(payload, initialWorkspaceFiles)', 'workspace restore must hydrate persisted files');
includesContent(
  app,
  'createWorkspacePayload(entryProject, workspaceFiles)',
  'workspace persistence should include the entry project and every tracked file',
);
includesContent(app, 'setEntryProject(payload.entryProject)', 'workspace import must restore the persisted entry project');
includesContent(workspacePersistence, "candidate.role !== 'project'", 'workspace import must validate file roles');
includesContent(workspacePersistence, 'normalizedPath', 'workspace import must confine file paths');
includesContent(projectTopbar, 'Unsaved edits', 'dirty workspace state must be visible');
includesContent(projectTopbar, 'Saved locally', 'clean workspace state must be visible');
includesContent(projectInspector, 'Unsaved local edits', 'validation/render should show stale state');
includesContent(projectInspector, 'Up to date', 'validation/render should show synced state');
includesContent(projectInspector, 'Developer Diagnostics', 'developer diagnostics panel should own raw YAML and command details');
includesContent(projectInspector, 'commands.validateProject', 'developer diagnostics should expose validation command details');
assert(unit.graph.nodes.length > 0 && unit.graph.signals.includes('input'), 'unit inspect graph fixture is empty');

// AE: Contract editor applies graph edits against unit drafts and surfaces binding errors.
includesContent(projectInspector, 'Edit blocked', 'invalid graph edit feedback is missing');
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
includesContent(app, 'resolveWorkspacePath(project.file, node.unit.file)', 'unit references must resolve against the project path');
includesContent(app, 'projectDraftToInspect', 'project UI must derive its model from current YAML');
includesContent(app, 'lastValidProjectDraft', 'invalid project YAML must retain the last valid canvas model');
includesContent(projectSidebar, 'onAddInstance(instanceUnit, instanceId)', 'project sidebar must add unit instances structurally');
includesContent(projectSidebar, 'onAddRoute({ from: routeSource, to: routeTarget })', 'project sidebar must add routes structurally');
includesContent(projectInspector, 'onRenameInstance', 'project inspector must expose safe instance rename');
includesContent(projectInspector, 'onRemoveRoute', 'project inspector must expose structural route disconnection');
includesContent(app, 'createUnitV2({ name })', 'workspace unit creation must use the structured YAML transformer');
includesContent(projectSidebar, 'onCreateUnit(unitName)', 'workspace sidebar must expose unit creation');

// AF: WASM preview facade and panel contract state machine.
assert(render.ok && render.frames === render.output.samples.length, 'render fixture must include deterministic samples');
assert(render.output.samples.length > 0, 'render fixture should include sample data');
includesContent(previewPanel, 'WasmBackend.create', 'preview must initialize the typed WASM facade');
includesContent(previewPanel, 'instance.replaceWorkspace', 'workspace revisions must be sent to WASM validation');
includesContent(previewPanel, 'instance.prepare', 'valid revisions must prepare a runtime image');
includesContent(previewPanel, 'window.setTimeout', 'workspace synchronization must be debounced');
includesContent(previewPanel, 'revision !== revisionRef.current', 'stale validation results must be ignored');
includesContent(previewPanel, 'navigator.mediaDevices.getUserMedia', 'live preview must support microphone input');
includesContent(previewPanel, 'createConfiguredAudioContext', 'live preview must use device-aware audio contexts');
includesContent(previewPanel, 'AUDIO_CALIBRATION_HINTS', 'live preview must calibrate browser latency hints');
includesContent(previewPanel, 'decodeAudioData', 'live preview must decode uploaded audio files');
includesContent(previewPanel, 'createBufferSource', 'uploaded files must use a WebAudio buffer source');
includesContent(previewPanel, "type InputMode = 'file' | 'microphone'", 'file and microphone transports must remain separate');
includesContent(previewPanel, 'backend.reset()', 'live preview reset must use the WASM backend');
includesContent(previewPanel, "running ? stop() : start()", 'live engine start and stop must share one control');
includesContent(previewPanel, 'backend.setParam', 'live parameter controls must use the WASM backend');
includesContent(previewPanel, 'previousOverridesRef', 'live parameter synchronization must detect reset values');
includesContent(previewPanel, 'override.originalValue', 'removed overrides must restore the original runtime value');
includesContent(paramKnob, 'clampValue(parsed, minValue, maxValue)', 'typed parameter values must clamp to metadata bounds');
includesContent(paramKnob, 'RANGE_FRACTION_PER_DRAG_PIXEL', 'knob drags must scale by the parameter range');
includesContent(projectNode, '<ParamKnob', 'unit cards must render their parameter knobs directly');
includesContent(projectNode, 'data.onParamChange?.', 'unit card knobs must use the shared YAML parameter update path');
includesContent(projectNode, "integer={control?.type === 'int'}", 'integer knob stepping must use declared parameter metadata');
includesContent(app, 'graphTopologySignature', 'scalar parameter edits must be separated from graph topology changes');
includesContent(projectCanvas, 'nodes={nodes}', 'project canvas must preserve node identities during scalar edits');
assert(!projectCanvas.includes('const displayedNodes'), 'project canvas must not recreate every node to inject callbacks');
includesContent(app, 'return changed ? next : files', 'equivalent parameter serialization must not create a new revision');
assert(
  projectInspector.indexOf('<PreviewPanel') < projectInspector.indexOf('{isProjectView && ('),
  'live preview must remain mounted across inspector views',
);
includesContent(previewPanel, 'backend.setBypass', 'live bypass controls must use the WASM backend');
includesContent(previewPanel, 'bypassByInstance', 'bypass UI state must be tracked per project instance');
includesContent(projectNode, 'project-node__bypass', 'unit cards must expose a bypass control');
includesContent(projectNode, 'controller?.setBypass(data.instance.id, !bypassed)', 'unit-card bypass must target the live instance');
includesContent(previewPanel, '.pollMeters()', 'preview meters must use throttled Worklet polling');
includesContent(previewPanel, 'outputLatencyMs', 'live preview must calculate browser-reported output latency');
includesContent(previewPanel, 'captureLatencyMs', 'live preview must read microphone latency when the browser exposes it');
includesContent(previewPanel, 'measureAcousticLatency', 'live preview must expose an acoustic latency calibration action');
includesContent(previewPanel, "'Latency chirp'", 'live preview latency calibration button is missing');
includesContent(app, '<LiveLatencyBadge />', 'live output latency must remain visible outside the inspector');
includesContent(liveLatencyBadge, "'Mic path est.'", 'live latency badge must distinguish microphone path estimates');
includesContent(liveLatencyBadge, 'micPathLatencySeverity(totalLatencyMs)', 'microphone path estimate must expose latency severity');
includesContent(liveLatencyBadge, 'Loopback ready', 'live latency badge must show measured loopback results');
includesContent(audioIo, "latencyHint: 'interactive'", 'live preview must request interactive browser latency');
includesContent(audioIo, 'latency: { ideal: 0 }', 'microphone preview must request the lowest available capture latency');
includesContent(wasmFacade, 'audioWorklet.addModule(this.options.processorWorkletUrl)', 'facade must load the explicit Worklet module');
includesContent(wasmFacade, 'fetch(this.options.processorWasmUrl)', 'processor WASM must be fetched outside the audio callback');
includesContent(wasmFacade, 'processorOptions: { moduleUrl: this.options.processorModuleUrl, wasmBinary }', 'WASM bytes must be transferred during Worklet construction');
includesContent(wasmFacade, 'setCurrentRevision(revision: number)', 'editor revisions must invalidate stale async work immediately');
includesContent(wasmFacade, 'Prepared revision ${revision} is stale', 'stale prepared images must be rejected');
includesContent(wasmFacade, "type: 'stage'", 'runtime hydration must be a separate processor message');
includesContent(wasmFacade, "type: 'commit'", 'runtime commit must be a separate processor message');
includesContent(wasmFacade, 'failedRevision: revision', 'runtime failures must identify their workspace revision');
includesContent(previewPanel, 'Live engine', 'preview must expose a user-facing live engine label');
includesContent(previewPanel, 'Project prepared for live audio.', 'preview must report prepared runtime readiness without revision details');
includesContent(previewPanel, 'backendDiagnostic.code', 'preview must expose structured diagnostic codes');
includesContent(previewPanel, 'backendDiagnostic.file', 'preview must expose diagnostic file paths');
includesContent(previewPanel, 'backendDiagnostic.path', 'preview must expose diagnostic schema paths');
includesContent(wasmFacade, 'bypassShadows', 'bypass controls must survive prepared runtime swaps');
includesContent(wasmFacade, 'muteShadow', 'mute control must survive prepared runtime swaps');
includesContent(previewPanel, 'backend.setMute(next)', 'preview must expose the runtime mute control');
includesContent(previewPanel, "'Mute output'", 'preview mute action is missing');
includesContent(processorWorklet, "import createApgProcessorModule from './apg_processor.mjs'", 'Worklet must use a static Emscripten import');
includesContent(processorWorklet, 'request.type === "commit"', 'Worklet must commit only through an explicit message');
includesContent(processorWorklet, 'request.type === "pollMeters"', 'meter snapshots must be copied outside process()');
includesContent(processorWorklet, 'callbackDeadlineMisses', 'Worklet meters must report callback deadline misses');
includesContent(processorWorklet, 'monotonicNow()', 'Worklet callbacks must use the high-resolution timer');
includesContent(processorWorklet, 'request.type === "startAudioTrace"', 'AudioWorklet must start bounded audio traces outside process()');
includesContent(processorWorklet, 'request.type === "pollAudioTrace"', 'AudioWorklet must build audio trace reports outside process()');
includesContent(wasmFacade, 'startAudioTrace()', 'WASM facade must expose audio trace start');
includesContent(wasmFacade, 'pollAudioTrace()', 'WASM facade must expose audio trace polling');
includesContent(projectInspector, 'data-testid="audio-trace-profile"', 'Developer Diagnostics must expose microphone profiling');
includesContent(projectInspector, 'exportAudioTraceReport', 'Developer Diagnostics must export audio trace JSON');
includesContent(projectInspector, 'data-testid="audio-io-panel"', 'right inspector must expose audio I/O controls');
includesContent(projectInspector, 'data-testid="audio-calibrate"', 'audio I/O controls must expose calibration');
includesContent(projectInspector, 'data-testid="audio-latency-chirp"', 'audio I/O controls must expose acoustic latency measurement');
includesContent(processorWorklet, 'request.type === "startLatencyProbe"', 'AudioWorklet must support acoustic latency probes');
includesContent(wasmFacade, 'measureLatencyProbe()', 'WASM facade must expose acoustic latency measurement');
assert(!processorWorklet.slice(processorWorklet.indexOf('process(inputs, outputs)')).includes('this.reply('), 'process() must not allocate and post meter messages');
assert(!processorWorklet.slice(processorWorklet.indexOf('process(inputs, outputs)')).includes('new Float'), 'process() must not allocate trace buffers');
assert(!processorWorklet.includes('HEAPF32.subarray'), 'process() must not allocate a typed-array view per block');
assert(!processorWorklet.includes('import(moduleUrl)'), 'dynamic import must not run in WorkletGlobalScope');
assert(!previewPanel.includes('createDeterministicPreviewAdapter'), 'deterministic preview adapter must not remain active');

// AG: Compatibility and export actionability.
for (const profile of ['desktop_full', 'wasm_realtime', 'm7_static', 'offline_render']) {
assert(compatibility.includes(profile), `compatibility matrix missing ${profile}`);
}
assert(compatibility.includes("'yes'") && compatibility.includes("'no'"), 'compatibility matrix must show both supported and unsupported states');
assertAtLeast(compatibility, '<strong>blocked</strong>', 2, 'export panel should show multiple blocked targets with reason');
assert(compatibility.includes('Not available in this build.'), 'export action panel should expose blocking reason text for unavailable targets');

console.log('web contract tests passed');
