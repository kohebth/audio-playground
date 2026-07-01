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

const atomCatalog = json('test/golden/v2-inspect-atoms.json');
const project = json('test/golden/v2-inspect-project-guitar-pedalboard.json');
const unit = json('test/golden/v2-inspect-unit-simple_gain.json');
const render = json('test/golden/v2-render-project-guitar-pedalboard.json');
const backendSamples = read('web-tools/unit-editor/src/lib/backendSamples.ts');

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
assert(backendSamples.includes("v2-inspect-atoms.json?raw"), 'backendSamples must load the frozen backend atom catalog JSON');
assert(!backendSamples.includes('../atoms/atomCatalog'), 'project workbench backend samples must not use the local atom catalog fallback');
includes('web-tools/unit-editor/src/components/ProjectInspector.tsx', '<strong>{atomCatalog.schema}</strong>', 'contract view must expose atom catalog schema drift');

assert(project.units.length === 6, 'pedalboard workspace fixture should include six referenced units');
assert(project.routes.some(route => route.from === 'trem1.output' && route.to === 'blend1.dry'), 'pedalboard route graph fixture changed');
assert((backendSamples.match(/role: 'unit'/g) ?? []).length === project.units.length, 'workspace bundle must include all referenced unit files');
includes('web-tools/unit-editor/src/App.tsx', 'apg.unit-editor.workspace.v1', 'workspace autosave key is missing');
includes('web-tools/unit-editor/src/App.tsx', "schema: 'apg.ui.workspace.v1'", 'workspace export schema is missing');
includes('web-tools/unit-editor/src/components/ProjectTopbar.tsx', 'Drafts pending', 'dirty workspace state must be visible');

assert(unit.graph.nodes.length > 0 && unit.graph.signals.includes('input'), 'unit inspect graph fixture is empty');
includes('web-tools/unit-editor/src/components/UnitGraphEditor.tsx', 'TODO_${field.type.toUpperCase()}', 'atom insertion must use backend atom field metadata');
includes('web-tools/unit-editor/src/components/UnitGraphEditor.tsx', 'lacks backend output binding metadata', 'invalid atom binding feedback is missing');

assert(render.ok && render.frames === render.output.samples.length, 'render fixture must include deterministic samples');
includes('web-tools/unit-editor/src/lib/previewAdapter.ts', 'setParam ${path}=${value}', 'preview adapter setParam command name changed');
includes('web-tools/unit-editor/src/lib/previewAdapter.ts', 'setBypass ${instanceId}=${enabled}', 'preview adapter setBypass command name changed');
includes('web-tools/unit-editor/src/components/PreviewPanel.tsx', 'WASM AudioWorklet preview backend is pending', 'preview missing-backend state is not visible');

const compatibility = read('web-tools/unit-editor/src/components/CompatibilityExportPanel.tsx');
for (const profile of ['desktop_full', 'wasm_realtime', 'm7_static', 'offline_render']) {
  assert(compatibility.includes(profile), `compatibility matrix missing ${profile}`);
}
includes('web-tools/unit-editor/src/components/CompatibilityExportPanel.tsx', 'commands.exportM7', 'M7 export command is not surfaced');
includes('web-tools/unit-editor/src/components/CompatibilityExportPanel.tsx', '<strong>blocked</strong>', 'blocked export readiness state is missing');

console.log('web contract tests passed');
