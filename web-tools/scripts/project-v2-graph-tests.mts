import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  addProjectInstance,
  addProjectRoute,
  addProjectUnitReference,
  applyProjectScene,
  duplicateProjectInstance,
  insertProjectInstanceOnRoute,
  moveProjectInstance,
  moveProjectRoute,
  parseProjectGraphDraft,
  removeProjectInstance,
  removeProjectRoute,
  removeProjectScene,
  renameProjectInstance,
  renameProjectScene,
  replaceProjectRoute,
  setProjectInstancePosition,
  upsertProjectScene,
  validateProjectRoutes,
  type ProjectPortCatalog,
} from '../src/lib/projectV2Graph.ts';
import { buildProjectGraph } from '../src/lib/projectGraph.ts';

const repo = resolve(import.meta.dirname, '../..');
const project = readFileSync(resolve(repo, 'test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml'), 'utf8');
const projectInspect = JSON.parse(
  readFileSync(resolve(repo, 'test/golden/v2-inspect-project-guitar-pedalboard.json'), 'utf8'),
);
const ports: ProjectPortCatalog = {
  noise_gate_unit: { inputs: ['input'], outputs: ['output'] },
  phaser_unit: { inputs: ['input'], outputs: ['output'] },
  overdrive_unit: { inputs: ['input'], outputs: ['output'] },
  tone_stack_unit: { inputs: ['input'], outputs: ['output'] },
  tremolo_unit: { inputs: ['input'], outputs: ['output'] },
  chorus_unit: { inputs: ['input'], outputs: ['output'] },
  delay_unit: { inputs: ['input'], outputs: ['output'] },
  wet_dry_mix_unit: { inputs: ['dry', 'wet'], outputs: ['output'] },
  reverb_unit: { inputs: ['input'], outputs: ['output'] },
};

const draft = parseProjectGraphDraft(project);
assert.equal(draft.nodes.length, 8);
assert.equal(draft.routes.length, 9);
assert.doesNotThrow(() => validateProjectRoutes(project, ports));
assert(buildProjectGraph(projectInspect).edges.every(edge => edge.label === undefined));

const emptyProject = `kind: apg.project
schema: apg.project.v2
name: empty
version: 2.0.0
units: []
chain:
  nodes: []
  routes:
    - from: system.input
      to: system.output
targets:
  default: desktop_full
`;
const registered = parseProjectGraphDraft(addProjectUnitReference(
  emptyProject,
  'overdrive_unit',
  '../units/overdrive.unit.v2.yaml',
));
assert.deepEqual(registered.units[0], { id: 'overdrive_unit', file: '../units/overdrive.unit.v2.yaml' });
assert.throws(
  () => addProjectUnitReference(project, 'overdrive_unit', '../units/other.unit.v2.yaml'),
  /already exists/,
);

const added = addProjectInstance(project, 'overdrive_unit', 'drive2', { drive: '2.2' }, { x: 320, y: 180 });
assert.equal(parseProjectGraphDraft(added.content).nodes.at(-1)?.id, 'drive2');
assert.deepEqual(parseProjectGraphDraft(added.content).nodes.at(-1)?.ui?.position, { x: 320, y: 180 });
assert.throws(() => addProjectInstance(project, 'missing_unit', 'missing1'), /was not found/);
assert.throws(() => addProjectInstance(project, 'overdrive_unit', 'drive1'), /already exists/);

const inserted = insertProjectInstanceOnRoute(
  project,
  ports,
  'tone_stack_unit',
  'tone_inserted',
  3,
  { bass: '1.0' },
  { x: 400, y: 200 },
);
const insertedDraft = parseProjectGraphDraft(inserted.content);
assert.equal(insertedDraft.nodes.find(node => node.id === inserted.id)?.unit, 'tone_stack_unit');
assert.deepEqual(insertedDraft.nodes.find(node => node.id === inserted.id)?.ui?.position, { x: 400, y: 200 });
assert.deepEqual(insertedDraft.routes.slice(3, 5), [
  { from: 'drive1.output', to: 'tone_inserted.input' },
  { from: 'tone_inserted.output', to: 'tone1.input' },
]);
assert.doesNotThrow(() => validateProjectRoutes(inserted.content, ports));
assert.throws(
  () => insertProjectInstanceOnRoute(
    project.replace(
      '  - id: reverb_unit\n',
      '  - id: wet_dry_mix_unit\n    file: ../units-v2/wet_dry_mix.unit.v2.yaml\n  - id: reverb_unit\n',
    ),
    ports,
    'wet_dry_mix_unit',
    'ambiguous_mix',
    3,
  ),
  /exactly one input and one output/,
);

const duplicated = duplicateProjectInstance(project, 'drive1');
const duplicate = parseProjectGraphDraft(duplicated.content).nodes.find(node => node.id === duplicated.id);
assert.equal(duplicate?.unit, 'overdrive_unit');
assert.equal(duplicate?.params.drive, '2.2');

const withScene = upsertProjectScene(project, 'Drive Check', { 'drive1.drive': '5.0' }, { drive1: true });
assert.equal(parseProjectGraphDraft(withScene).scenes.at(-1)?.bypass.drive1, true);
const appliedScene = applyProjectScene(withScene, 'Drive Check');
assert.equal(parseProjectGraphDraft(appliedScene.content).nodes.find(node => node.id === 'drive1')?.params.drive, '5.0');
assert.equal(appliedScene.bypass.drive1, true);

const renamed = parseProjectGraphDraft(renameProjectInstance(withScene, 'drive1', 'drive_main'));
assert(renamed.routes.some(route => route.from === 'drive_main.output'));
assert(renamed.routes.some(route => route.to === 'drive_main.input'));
assert.equal(renamed.scenes[0].params['drive_main.drive'], '1.4');
assert(!('drive1.drive' in renamed.scenes[0].params));
assert.equal(renamed.scenes.at(-1)?.bypass.drive_main, true);
assert.throws(() => renameProjectInstance(project, 'drive1', 'tone1'), /already exists/);

const removed = parseProjectGraphDraft(removeProjectInstance(project, 'delay1'));
assert(!removed.nodes.some(node => node.id === 'delay1'));
assert(!removed.routes.some(route => route.from.startsWith('delay1.') || route.to.startsWith('delay1.')));
assert(!Object.keys(removed.scenes[1].params).some(path => path.startsWith('delay1.')));
const removedSceneInstance = parseProjectGraphDraft(removeProjectInstance(withScene, 'drive1'));
assert(!('drive1' in (removedSceneInstance.scenes.at(-1)?.bypass ?? {})));

const renamedScene = renameProjectScene(withScene, 'Drive Check', 'Drive Ready');
assert.equal(parseProjectGraphDraft(renamedScene).scenes.at(-1)?.name, 'Drive Ready');
assert.equal(parseProjectGraphDraft(removeProjectScene(renamedScene, 'Drive Ready')).scenes.length, 2);

const moved = parseProjectGraphDraft(moveProjectInstance(project, 'drive1', 4));
assert.equal(moved.nodes[4].id, 'drive1');
const positioned = parseProjectGraphDraft(setProjectInstancePosition(project, 'drive1', { x: 42, y: 84 }));
assert.deepEqual(positioned.nodes.find(node => node.id === 'drive1')?.ui?.position, { x: 42, y: 84 });

const disconnected = removeProjectRoute(project, 3);
assert.equal(parseProjectGraphDraft(disconnected).routes.length, 8);
const restored = addProjectRoute(disconnected, ports, { from: 'drive1.output', to: 'tone1.input' });
assert.equal(parseProjectGraphDraft(restored).routes.length, 9);
assert.throws(
  () => addProjectRoute(project, ports, { from: 'gate1.output', to: 'phaser1.input' }),
  /already connected/,
);
assert.throws(
  () => addProjectRoute(removeProjectRoute(project, 2), ports, { from: 'tone1.output', to: 'drive1.input' }),
  /creates a cycle/,
);
assert.throws(
  () => addProjectRoute(disconnected, ports, { from: 'drive1.input', to: 'tone1.input' }),
  /not a unit output/,
);
assert.throws(
  () => validateProjectRoutes(project.replace('to: drive1.input', 'to: drive1.not_a_port'), ports),
  /not a unit input/,
);

const replaced = parseProjectGraphDraft(replaceProjectRoute(project, ports, 5, {
  from: 'drive1.output',
  to: 'chorus1.input',
}));
assert.equal(replaced.routes[5].from, 'drive1.output');
assert.equal(replaced.routes[5].to, 'chorus1.input');
const reorderedRoutes = parseProjectGraphDraft(moveProjectRoute(project, 8, 0));
assert.equal(reorderedRoutes.routes[0].to, 'system.output');

console.log('project v2 graph transformer tests passed');
