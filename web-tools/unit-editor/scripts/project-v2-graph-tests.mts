import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  addProjectInstance,
  addProjectRoute,
  duplicateProjectInstance,
  moveProjectInstance,
  moveProjectRoute,
  parseProjectGraphDraft,
  removeProjectInstance,
  removeProjectRoute,
  renameProjectInstance,
  replaceProjectRoute,
  setProjectInstancePosition,
  validateProjectRoutes,
  type ProjectPortCatalog,
} from '../src/lib/projectV2Graph.ts';

const repo = resolve(import.meta.dirname, '../../..');
const project = readFileSync(resolve(repo, 'test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml'), 'utf8');
const ports: ProjectPortCatalog = {
  noise_gate_unit: { inputs: ['input'], outputs: ['output'] },
  overdrive_unit: { inputs: ['input'], outputs: ['output'] },
  tone_stack_unit: { inputs: ['input'], outputs: ['output'] },
  tremolo_unit: { inputs: ['input'], outputs: ['output'] },
  delay_unit: { inputs: ['input'], outputs: ['output'] },
  wet_dry_mix_unit: { inputs: ['dry', 'wet'], outputs: ['output'] },
  reverb_unit: { inputs: ['input'], outputs: ['output'] },
};

const draft = parseProjectGraphDraft(project);
assert.equal(draft.nodes.length, 7);
assert.equal(draft.routes.length, 9);
assert.doesNotThrow(() => validateProjectRoutes(project, ports));

const added = addProjectInstance(project, 'overdrive_unit', 'drive2', { drive: '2.2' }, { x: 320, y: 180 });
assert.equal(parseProjectGraphDraft(added.content).nodes.at(-1)?.id, 'drive2');
assert.deepEqual(parseProjectGraphDraft(added.content).nodes.at(-1)?.ui?.position, { x: 320, y: 180 });
assert.throws(() => addProjectInstance(project, 'missing_unit', 'missing1'), /was not found/);
assert.throws(() => addProjectInstance(project, 'overdrive_unit', 'drive1'), /already exists/);

const duplicated = duplicateProjectInstance(project, 'drive1');
const duplicate = parseProjectGraphDraft(duplicated.content).nodes.find(node => node.id === duplicated.id);
assert.equal(duplicate?.unit, 'overdrive_unit');
assert.equal(duplicate?.params.drive, '2.2');

const renamed = parseProjectGraphDraft(renameProjectInstance(project, 'drive1', 'drive_main'));
assert(renamed.routes.some(route => route.from === 'drive_main.output'));
assert(renamed.routes.some(route => route.to === 'drive_main.input'));
assert.equal(renamed.scenes[0].params['drive_main.drive'], '1.4');
assert(!('drive1.drive' in renamed.scenes[0].params));
assert.throws(() => renameProjectInstance(project, 'drive1', 'tone1'), /already exists/);

const removed = parseProjectGraphDraft(removeProjectInstance(project, 'delay1'));
assert(!removed.nodes.some(node => node.id === 'delay1'));
assert(!removed.routes.some(route => route.from.startsWith('delay1.') || route.to.startsWith('delay1.')));
assert(!Object.keys(removed.scenes[1].params).some(path => path.startsWith('delay1.')));

const moved = parseProjectGraphDraft(moveProjectInstance(project, 'drive1', 4));
assert.equal(moved.nodes[4].id, 'drive1');
const positioned = parseProjectGraphDraft(setProjectInstancePosition(project, 'drive1', { x: 42, y: 84 }));
assert.deepEqual(positioned.nodes.find(node => node.id === 'drive1')?.ui?.position, { x: 42, y: 84 });

const disconnected = removeProjectRoute(project, 2);
assert.equal(parseProjectGraphDraft(disconnected).routes.length, 8);
const restored = addProjectRoute(disconnected, ports, { from: 'drive1.output', to: 'tone1.input' });
assert.equal(parseProjectGraphDraft(restored).routes.length, 9);
assert.throws(
  () => addProjectRoute(project, ports, { from: 'gate1.output', to: 'drive1.input' }),
  /already connected/,
);
assert.throws(
  () => addProjectRoute(removeProjectRoute(project, 1), ports, { from: 'tone1.output', to: 'drive1.input' }),
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
  to: 'blend1.dry',
}));
assert.equal(replaced.routes[5].from, 'drive1.output');
const reorderedRoutes = parseProjectGraphDraft(moveProjectRoute(project, 8, 0));
assert.equal(reorderedRoutes.routes[0].to, 'system.output');

console.log('project v2 graph transformer tests passed');
