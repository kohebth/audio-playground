import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import type { AtomCatalog } from '../src/lib/backendSamples.ts';
import {
  addAtomNodeToUnit,
  createUnitV2,
  parseUnitGraphDraft,
  removeAtomNodeFromUnit,
  serializeUnitGraphNodeUpdate,
} from '../src/lib/unitV2Graph.ts';

const repo = resolve(import.meta.dirname, '../../..');
const catalog = JSON.parse(readFileSync(resolve(repo, 'test/golden/v2-inspect-atoms.json'), 'utf8')) as AtomCatalog;

const created = createUnitV2({ name: 'browser_gain', title: 'Browser Gain' });
const createdGraph = parseUnitGraphDraft(created);
assert.equal(createdGraph.name, 'browser_gain');
assert.deepEqual(createdGraph.nodes.map(node => node.id), ['gain_value', 'apply_gain']);
assert.throws(() => createUnitV2({ name: 'Not Valid' }), /lowercase snake_case/);

const configuredNode = { ...createdGraph.nodes[0], config: { ...createdGraph.nodes[0].config, value: '0.5' } };
const configured = serializeUnitGraphNodeUpdate(created, configuredNode);
assert.equal(parseUnitGraphDraft(configured).nodes[0].config.value, '0.5');

const renamedNode = { ...createdGraph.nodes[0], id: 'gain_source' };
const renamed = serializeUnitGraphNodeUpdate(created, renamedNode, 'gain_value');
assert.equal(parseUnitGraphDraft(renamed).nodes[0].id, 'gain_source');
assert.throws(
  () => serializeUnitGraphNodeUpdate(created, { ...createdGraph.nodes[0], id: 'apply_gain' }, 'gain_value'),
  /already used/,
);

assert.throws(() => removeAtomNodeFromUnit(created, 'apply_gain'), /unit output output references/);
assert.throws(() => removeAtomNodeFromUnit(created, 'gain_value'), /apply_gain consumes/);

const applyGain = { ...createdGraph.nodes[1], in: { ...createdGraph.nodes[1].in, signal_b: 'input' } };
const disconnected = serializeUnitGraphNodeUpdate(created, applyGain);
const removed = removeAtomNodeFromUnit(disconnected, 'gain_value');
const removedGraph = parseUnitGraphDraft(removed);
assert.deepEqual(removedGraph.nodes.map(node => node.id), ['apply_gain']);
assert(!removedGraph.signals.includes('gain_value'));

const added = addAtomNodeToUnit(created, catalog, 'amplitude_clip_hard');
const addedGraph = parseUnitGraphDraft(added.content);
const addedNode = addedGraph.nodes.find(node => node.id === added.id);
assert(addedNode);
assert.deepEqual(Object.keys(addedNode.in), ['signal']);
assert.deepEqual(Object.keys(addedNode.out), ['signal']);
assert.deepEqual(Object.keys(addedNode.config), ['threshold']);
const removedAdded = parseUnitGraphDraft(removeAtomNodeFromUnit(added.content, added.id));
assert(!removedAdded.nodes.some(node => node.id === added.id));
assert(!removedAdded.signals.some(signal => signal.startsWith(`${added.id}_`)));

const overdrive = readFileSync(resolve(repo, 'test/fixtures/units-v2/overdrive.unit.v2.yaml'), 'utf8');
assert.throws(() => removeAtomNodeFromUnit(overdrive, 'tone_filter'), /apply_level consumes/);
assert.throws(() => removeAtomNodeFromUnit(overdrive, 'apply_level'), /unit output output references/);

console.log('unit v2 graph transformer tests passed');
