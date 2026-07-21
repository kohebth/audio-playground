import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  addProjectInstance,
  addProjectRoute,
  addProjectUnitReference,
  applyProjectScene,
  copyProjectInstance,
  duplicateProjectInstance,
  insertProjectParallelOnRoute,
  insertProjectInstanceOnRoute,
  moveProjectInstance,
  moveProjectRoute,
  parseProjectGraphDraft,
  parseUnitPortNames,
  pasteProjectInstance,
  projectDraftToInspect,
  removeProjectInstance,
  removeProjectInstanceWithTopology,
  removeProjectRoute,
  removeProjectScene,
  renameProjectInstance,
  renameProjectScene,
  replaceProjectInstance,
  replaceProjectRoute,
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
const linearLayout = buildProjectGraph(projectInspect);
assert(linearLayout.edges.every(edge => edge.label === undefined));
assert(linearLayout.edges.every(edge => edge.type === 'projectRoute'));
assert.equal(linearLayout.edges[0].sourceHandle, 'input');
assert.equal(linearLayout.edges[0].targetHandle, 'input');
assert.equal(linearLayout.edges[3].sourceHandle, 'output');
assert.equal(linearLayout.edges[3].targetHandle, 'input');
assert(linearLayout.edges.every(edge => edge.data.points.every((point, index, points) => (
  index === 0 || point.x === points[index - 1].x || point.y === points[index - 1].y
))));
assert.equal(new Set(linearLayout.edges.flatMap(edge => [edge.data.points[0].y, edge.data.points.at(-1)!.y])).size, 1);
assert.deepEqual(buildProjectGraph(projectInspect), linearLayout);

const overdriveContent = readFileSync(resolve(repo, 'test/fixtures/units-v2/overdrive.unit.v2.yaml'), 'utf8');
assert.deepEqual(parseUnitPortNames(overdriveContent), {
  inputs: ['input'],
  outputs: ['output'],
  userPlaceable: true,
  reason: null,
});
const wetDryContent = readFileSync(resolve(repo, 'test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml'), 'utf8');
assert.equal(parseUnitPortNames(wetDryContent).userPlaceable, false);

const legacyPositionProject = project.replace(
  '    - id: gate1\n      unit: noise_gate_unit\n',
  '    - id: gate1\n      unit: noise_gate_unit\n      ui:\n        position:\n          x: 999\n          y: -400\n',
);
const legacyPositionDraft = parseProjectGraphDraft(legacyPositionProject);
assert.deepEqual(legacyPositionDraft.nodes[0].ui?.position, { x: 999, y: -400 });
assert.deepEqual(
  buildProjectGraph(projectDraftToInspect(legacyPositionDraft, projectInspect)).nodes.map(node => node.position),
  linearLayout.nodes.map(node => node.position),
);
const legacyPositionEdited = addProjectInstance(legacyPositionProject, 'overdrive_unit', 'legacy_added');
assert.deepEqual(
  parseProjectGraphDraft(legacyPositionEdited.content).nodes.find(node => node.id === 'gate1')?.ui?.position,
  { x: 999, y: -400 },
);

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

const added = addProjectInstance(project, 'overdrive_unit', 'drive2', { drive: '2.2' });
assert.equal(parseProjectGraphDraft(added.content).nodes.at(-1)?.id, 'drive2');
assert.equal(parseProjectGraphDraft(added.content).nodes.at(-1)?.ui, undefined);
assert.throws(() => addProjectInstance(project, 'missing_unit', 'missing1'), /was not found/);
assert.throws(() => addProjectInstance(project, 'overdrive_unit', 'drive1'), /already exists/);

const inserted = insertProjectInstanceOnRoute(
  project,
  ports,
  'tone_stack_unit',
  'tone_inserted',
  3,
  { bass: '1.0' },
);
const insertedDraft = parseProjectGraphDraft(inserted.content);
assert.equal(insertedDraft.nodes.find(node => node.id === inserted.id)?.unit, 'tone_stack_unit');
assert.equal(insertedDraft.nodes.find(node => node.id === inserted.id)?.ui, undefined);
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

const parallelSource = addProjectUnitReference(
  emptyProject,
  'wet_dry_mix_unit',
  '../units/wet_dry_mix.unit.v2.yaml',
);
const parallelRegistered = addProjectUnitReference(
  parallelSource,
  'overdrive_unit',
  '../units/overdrive.unit.v2.yaml',
);
const parallel = insertProjectParallelOnRoute(
  parallelRegistered,
  ports,
  'overdrive_unit',
  'parallel_drive',
  'wet_dry_mix_unit',
  'parallel_mix',
  0,
  { drive: '2.4' },
  { mix: '0.35' },
);
const parallelDraft = parseProjectGraphDraft(parallel.content);
assert.equal(parallelDraft.nodes.length, 2);
assert.deepEqual(parallelDraft.routes, [
  { from: 'system.input', to: 'parallel_drive.input' },
  { from: 'system.input', to: 'parallel_mix.dry' },
  { from: 'parallel_drive.output', to: 'parallel_mix.wet' },
  { from: 'parallel_mix.output', to: 'system.output' },
]);
assert.doesNotThrow(() => validateProjectRoutes(parallel.content, ports));

const overdriveUnit = projectInspect.units.find((unit: { id: string }) => unit.id === 'overdrive_unit');
assert(overdriveUnit);
const parallelInspect = {
  ...projectInspect,
  units: [
    overdriveUnit,
    { ...overdriveUnit, id: 'wet_dry_mix_unit', name: 'wet/dry mix' },
  ],
  nodes: [
    { id: 'parallel_drive', unit: 'overdrive_unit', params: [] },
    { id: 'parallel_mix', unit: 'wet_dry_mix_unit', params: [] },
  ],
  routes: parallelDraft.routes,
};
const parallelLayout = buildProjectGraph(parallelInspect);
const dryRoute = parallelLayout.edges[1];
const branchNode = parallelLayout.nodes.find(node => node.id === 'unit-parallel_drive');
assert(branchNode);
assert(dryRoute.data.points.length > 2);
assert(dryRoute.data.points.every((point, index, points) => (
  index === 0 || point.x === points[index - 1].x || point.y === points[index - 1].y
)));
const branchRect = {
  left: branchNode.position.x,
  right: branchNode.position.x + 140,
  top: branchNode.position.y,
  bottom: branchNode.position.y + 132,
};
for (let index = 1; index < dryRoute.data.points.length; index += 1) {
  const start = dryRoute.data.points[index - 1];
  const end = dryRoute.data.points[index];
  const crossesHorizontal = start.y === end.y
    && start.y > branchRect.top && start.y < branchRect.bottom
    && Math.max(start.x, end.x) > branchRect.left && Math.min(start.x, end.x) < branchRect.right;
  const crossesVertical = start.x === end.x
    && start.x > branchRect.left && start.x < branchRect.right
    && Math.max(start.y, end.y) > branchRect.top && Math.min(start.y, end.y) < branchRect.bottom;
  assert(!crossesHorizontal && !crossesVertical, 'parallel dry route must avoid the branch card');
}

const duplicated = duplicateProjectInstance(project, 'drive1');
const duplicate = parseProjectGraphDraft(duplicated.content).nodes.find(node => node.id === duplicated.id);
assert.equal(duplicate?.unit, 'overdrive_unit');
assert.equal(duplicate?.params.drive, '2.2');
const clipboard = copyProjectInstance(project, 'drive1');
const pasted = pasteProjectInstance(project, clipboard);
const pastedDraft = parseProjectGraphDraft(pasted.content);
assert.equal(pastedDraft.nodes.find(node => node.id === pasted.id)?.unit, 'overdrive_unit');
assert(!pastedDraft.routes.some(route => route.from.startsWith(`${pasted.id}.`) || route.to.startsWith(`${pasted.id}.`)));

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

const topologyRemoved = removeProjectInstanceWithTopology(project, ports, 'drive1');
assert.equal(topologyRemoved.mode, 'bridged');
assert.equal(topologyRemoved.bridgedRoutes, 1);
assert(parseProjectGraphDraft(topologyRemoved.content).routes.some(route => (
  route.from === 'phaser1.output' && route.to === 'tone1.input'
)));

const branchedProject = project.replace(
  '      to: tone1.input\n',
  '      to: tone1.input\n    - from: drive1.output\n      to: trem1.input\n',
).replace('    - from: tone1.output\n      to: trem1.input\n', '');
const branchedRemoval = removeProjectInstanceWithTopology(branchedProject, ports, 'drive1');
assert.equal(branchedRemoval.bridgedRoutes, 2);
assert.deepEqual(
  parseProjectGraphDraft(branchedRemoval.content).routes.filter(route => route.from === 'phaser1.output').map(route => route.to).sort(),
  ['tone1.input', 'trem1.input'],
);

const specialRegistered = addProjectUnitReference(emptyProject, 'wet_dry_mix_unit', '../units-v2/wet_dry_mix.unit.v2.yaml');
const specialAdded = addProjectInstance(specialRegistered, 'wet_dry_mix_unit', 'special_mix');
const specialRouted = addProjectRoute(
  removeProjectRoute(specialAdded.content, 0),
  ports,
  { from: 'special_mix.output', to: 'system.output' },
);
const specialRemoval = removeProjectInstanceWithTopology(specialRouted, ports, 'special_mix');
assert.equal(specialRemoval.mode, 'disconnected');
assert.equal(parseProjectGraphDraft(specialRemoval.content).routes.length, 0);

const replacementDefaults = { bass: '0.2', mid: '0.5' };
const replacedInstance = parseProjectGraphDraft(replaceProjectInstance(
  withScene,
  ports,
  'drive1',
  'tone_stack_unit',
  replacementDefaults,
));
assert.equal(replacedInstance.nodes.find(node => node.id === 'drive1')?.unit, 'tone_stack_unit');
assert.deepEqual(replacedInstance.nodes.find(node => node.id === 'drive1')?.params, replacementDefaults);
assert(replacedInstance.routes.some(route => route.from === 'drive1.output'));
assert.deepEqual(replacedInstance.scenes.at(-1)?.params['drive1.bass'], '0.2');
assert(!('drive1.drive' in (replacedInstance.scenes.at(-1)?.params ?? {})));

const renamedScene = renameProjectScene(withScene, 'Drive Check', 'Drive Ready');
assert.equal(parseProjectGraphDraft(renamedScene).scenes.at(-1)?.name, 'Drive Ready');
assert.equal(parseProjectGraphDraft(removeProjectScene(renamedScene, 'Drive Ready')).scenes.length, 2);

const moved = parseProjectGraphDraft(moveProjectInstance(project, 'drive1', 4));
assert.equal(moved.nodes[4].id, 'drive1');

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
assert.throws(
  () => validateProjectRoutes(project.replace(
    'from: phaser1.output\n      to: drive1.input',
    'from: tone1.output\n      to: drive1.input',
  ), ports),
  /create a cycle/,
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
