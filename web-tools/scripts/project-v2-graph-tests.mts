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
  removeEmptyProjectRoutingSection,
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
  gain_unit: { inputs: ['input'], outputs: ['output'] },
  path_panner_2_unit: {
    inputs: ['input'],
    outputs: ['path_1', 'path_2'],
    routing: {
      role: 'panner',
      paths: [
        { port: 'path_1', levelParam: 'path_1_db' },
        { port: 'path_2', levelParam: 'path_2_db' },
      ],
    },
  },
  path_mixer_2_unit: {
    inputs: ['path_1', 'path_2'],
    outputs: ['output'],
    routing: {
      role: 'mixer',
      paths: [
        { port: 'path_1', levelParam: 'path_1_db' },
        { port: 'path_2', levelParam: 'path_2_db' },
      ],
    },
  },
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
  routing: undefined,
  userPlaceable: true,
  reason: null,
});
const wetDryContent = readFileSync(resolve(repo, 'test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml'), 'utf8');
assert.equal(parseUnitPortNames(wetDryContent).userPlaceable, false);
const pannerContent = readFileSync(resolve(repo, 'test/fixtures/units-v2/path_panner_2.unit.v2.yaml'), 'utf8');
const mixerContent = readFileSync(resolve(repo, 'test/fixtures/units-v2/path_mixer_2.unit.v2.yaml'), 'utf8');
assert.deepEqual(parseUnitPortNames(pannerContent).routing, ports.path_panner_2_unit.routing);
assert.deepEqual(parseUnitPortNames(mixerContent).routing, ports.path_mixer_2_unit.routing);

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

const parallelPanner = addProjectUnitReference(
  emptyProject,
  'path_panner_2_unit',
  '../units/path_panner_2.unit.v2.yaml',
);
const parallelMixer = addProjectUnitReference(
  parallelPanner,
  'path_mixer_2_unit',
  '../units/path_mixer_2.unit.v2.yaml',
);
const parallelRegistered = addProjectUnitReference(
  parallelMixer,
  'overdrive_unit',
  '../units/overdrive.unit.v2.yaml',
);
const parallel = insertProjectParallelOnRoute(
  parallelRegistered,
  ports,
  'overdrive_unit',
  'parallel_drive',
  'path_panner_2_unit',
  'parallel_pan',
  'path_mixer_2_unit',
  'parallel_mix',
  0,
  { drive: '2.4' },
  { path_1_db: '0', path_2_db: '-3' },
  { path_1_db: '-6.0206', path_2_db: '-9' },
);
const parallelDraft = parseProjectGraphDraft(parallel.content);
assert.equal(parallelDraft.nodes.length, 3);
assert.equal(parallelDraft.nodes.find(node => node.id === 'parallel_pan')?.routing?.section, 'parallel_1');
assert.equal(parallelDraft.nodes.find(node => node.id === 'parallel_mix')?.routing?.section, 'parallel_1');
assert.deepEqual(parallelDraft.routes, [
  { from: 'system.input', to: 'parallel_pan.input' },
  { from: 'parallel_pan.path_1', to: 'parallel_mix.path_1' },
  { from: 'parallel_pan.path_2', to: 'parallel_drive.input' },
  { from: 'parallel_drive.output', to: 'parallel_mix.path_2' },
  { from: 'parallel_mix.output', to: 'system.output' },
]);
assert.doesNotThrow(() => validateProjectRoutes(parallel.content, ports));
assert.throws(() => duplicateProjectInstance(parallel.content, 'parallel_pan'), /Add in parallel/);
assert.throws(() => pasteProjectInstance(parallel.content, copyProjectInstance(parallel.content, 'parallel_mix')), /unpaired/);
assert.throws(() => removeProjectInstanceWithTopology(parallel.content, ports, 'parallel_pan'), /section|panner/i);
assert.throws(() => removeProjectRoute(parallel.content, 1, ports), /every input|incomplete|connected/i);
assert.throws(
  () => removeEmptyProjectRoutingSection(parallel.content, ports, 'parallel_mix'),
  /Remove the effects inside both parallel paths/,
);
const emptyParallel = removeProjectInstanceWithTopology(parallel.content, ports, 'parallel_drive').content;
const collapsedParallel = parseProjectGraphDraft(
  removeEmptyProjectRoutingSection(emptyParallel, ports, 'parallel_pan').content,
);
assert.equal(collapsedParallel.nodes.length, 0);
assert.deepEqual(collapsedParallel.routes, [{ from: 'system.input', to: 'system.output' }]);
assert.throws(
  () => upsertProjectScene(parallel.content, 'Invalid', {}, { parallel_mix: true }),
  /always active/,
);
assert.throws(
  () => validateProjectRoutes(parallel.content.replace(
    'from: parallel_pan.path_1\n      to: parallel_mix.path_1',
    'from: parallel_pan.path_1\n      to: parallel_mix.path_2',
  ).replace(
    'from: parallel_drive.output\n      to: parallel_mix.path_2',
    'from: parallel_drive.output\n      to: parallel_mix.path_1',
  ), ports),
  /already connected|crossed|instead/,
);
const nestedParallel = readFileSync(resolve(repo, 'test/fixtures/projects-v2/nested-parallel.project.v2.yaml'), 'utf8');
assert.doesNotThrow(() => validateProjectRoutes(nestedParallel, ports));
const nestedInsert = insertProjectParallelOnRoute(
  parallel.content,
  ports,
  'overdrive_unit',
  'nested_drive',
  'path_panner_2_unit',
  'nested_pan',
  'path_mixer_2_unit',
  'nested_mix',
  2,
);
assert.equal(parseProjectGraphDraft(nestedInsert.content).nodes.find(node => node.id === 'nested_pan')?.routing?.section, 'parallel_2');
assert.doesNotThrow(() => validateProjectRoutes(nestedInsert.content, ports));

const overdriveUnit = projectInspect.units.find((unit: { id: string }) => unit.id === 'overdrive_unit');
assert(overdriveUnit);
const parallelInspect = {
  ...projectInspect,
  units: [
    overdriveUnit,
    { ...overdriveUnit, id: 'path_panner_2_unit', name: 'Pan 2' },
    { ...overdriveUnit, id: 'path_mixer_2_unit', name: 'Mix 2' },
  ],
  nodes: [
    { id: 'parallel_pan', unit: 'path_panner_2_unit', params: [], routing: { section: 'parallel_1' } },
    { id: 'parallel_drive', unit: 'overdrive_unit', params: [] },
    { id: 'parallel_mix', unit: 'path_mixer_2_unit', params: [], routing: { section: 'parallel_1' } },
  ],
  routes: parallelDraft.routes,
};
const parallelLayout = buildProjectGraph(parallelInspect, ports);
const dryRoute = parallelLayout.edges[1];
const branchNode = parallelLayout.nodes.find(node => node.id === 'unit-parallel_drive');
const parallelPannerNode = parallelLayout.nodes.find(node => node.id === 'unit-parallel_pan');
const parallelMixerNode = parallelLayout.nodes.find(node => node.id === 'unit-parallel_mix');
assert(branchNode);
assert(parallelPannerNode?.data.kind === 'unit');
assert(parallelMixerNode?.data.kind === 'unit');
assert(parallelPannerNode.data.routingLayout);
assert(parallelMixerNode.data.routingLayout);
assert.equal(parallelPannerNode.data.routingLayout.height, 266);
assert.equal(parallelMixerNode.data.routingLayout.height, 266);
assert.deepEqual(
  Object.values(parallelPannerNode.data.routingLayout.outputTops),
  Object.values(parallelPannerNode.data.routingLayout.controlTops),
);
assert.deepEqual(
  Object.values(parallelMixerNode.data.routingLayout.inputTops),
  Object.values(parallelMixerNode.data.routingLayout.controlTops),
);
assert.equal(dryRoute.data.points.length, 2);
assert.equal(
  dryRoute.data.points[0].y,
  parallelPannerNode.position.y + parallelPannerNode.data.routingLayout.outputTops.path_1,
);
assert.equal(
  dryRoute.data.points.at(-1)?.y,
  parallelMixerNode.position.y + parallelMixerNode.data.routingLayout.inputTops.path_1,
);
assert(parallelLayout.edges.every(edge => edge.data.points.length === 2));
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

const nestedLayout = buildProjectGraph(
  projectDraftToInspect(parseProjectGraphDraft(nestedParallel), {
    ...projectInspect,
    units: parallelInspect.units,
  }),
  ports,
);
const outerPanner = nestedLayout.nodes.find(node => node.id === 'unit-outer_pan');
const innerPanner = nestedLayout.nodes.find(node => node.id === 'unit-inner_pan');
assert(outerPanner?.data.kind === 'unit' && outerPanner.data.routingLayout);
assert(innerPanner?.data.kind === 'unit' && innerPanner.data.routingLayout);
assert(outerPanner.data.routingLayout.height > innerPanner.data.routingLayout.height);
assert(nestedLayout.edges.every(edge => edge.data.points.length === 2));

const fourPathContract = {
  paths: Array.from({ length: 4 }, (_, index) => ({
    port: `path_${index + 1}`,
    levelParam: `path_${index + 1}_db`,
  })),
};
const fourPathPorts: ProjectPortCatalog = {
  path_panner_2_unit: {
    inputs: ['input'],
    outputs: fourPathContract.paths.map(path => path.port),
    routing: { role: 'panner', paths: fourPathContract.paths },
  },
  path_mixer_2_unit: {
    inputs: fourPathContract.paths.map(path => path.port),
    outputs: ['output'],
    routing: { role: 'mixer', paths: fourPathContract.paths },
  },
};
const fourPathInspect = {
  ...parallelInspect,
  nodes: [
    { id: 'four_pan', unit: 'path_panner_2_unit', params: [], routing: { section: 'four' } },
    { id: 'four_mix', unit: 'path_mixer_2_unit', params: [], routing: { section: 'four' } },
  ],
  routes: [
    { from: 'system.input', to: 'four_pan.input' },
    ...fourPathContract.paths.map(path => ({
      from: `four_pan.${path.port}`,
      to: `four_mix.${path.port}`,
    })),
    { from: 'four_mix.output', to: 'system.output' },
  ],
};
const fourPathLayout = buildProjectGraph(fourPathInspect, fourPathPorts);
const fourPathPanner = fourPathLayout.nodes.find(node => node.id === 'unit-four_pan');
assert(fourPathPanner?.data.kind === 'unit');
assert(fourPathPanner.data.routingLayout);
assert(fourPathPanner.data.routingLayout.height > parallelPannerNode.data.routingLayout.height);
assert.equal(Object.keys(fourPathPanner.data.routingLayout.controlTops).length, 4);
assert.equal(new Set(fourPathLayout.edges.slice(1, 5).map(edge => edge.data.points[0].y)).size, 4);
assert(fourPathLayout.edges.slice(1, 5).every(edge => edge.data.points.length === 2));

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
assert.throws(
  () => validateProjectRoutes(branchedProject, ports),
  /Add in parallel/,
);

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
  () => addProjectRoute(
    removeProjectRoute(removeProjectRoute(project, 4), 2),
    ports,
    { from: 'tone1.output', to: 'drive1.input' },
  ),
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
  /create a cycle|Add in parallel|orphaned/,
);

const replaced = parseProjectGraphDraft(replaceProjectRoute(added.content, ports, 5, {
  from: 'trem1.output',
  to: 'drive2.input',
}));
assert.equal(replaced.routes[5].from, 'trem1.output');
assert.equal(replaced.routes[5].to, 'drive2.input');
const reorderedRoutes = parseProjectGraphDraft(moveProjectRoute(project, 8, 0));
assert.equal(reorderedRoutes.routes[0].to, 'system.output');

console.log('project v2 graph transformer tests passed');
