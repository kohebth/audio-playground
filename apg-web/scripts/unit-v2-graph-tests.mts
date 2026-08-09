import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import type { AtomCatalog } from '../src/lib/backendSamples.ts';
import {
  addAtomNodeToUnit,
  addUnitPort,
  assertUserPlaceableUnit,
  classifyUserEffectContent,
  connectUnitNodes,
  createUnitV2,
  disconnectUnitInput,
  insertAtomNodeOnConnection,
  moveUnitConnection,
  moveUnitParam,
  pasteAtomNodeIntoUnit,
  parseUnitGraphDraft,
  parseUnitPortsDraft,
  previewAtomReplacement,
  reconnectUnitConnection,
  removeAtomNodeFromUnit,
  removeAtomNodeWithTopology,
  removeUnitParam,
  replaceAtomNodeInUnit,
  replaceUnitConnection,
  serializeUnitGraphNodeUpdate,
  setAtomNodePosition,
  updateUnitCompatibility,
  updateUnitDefinition,
  updateUnitParam,
  updateUnitPort,
} from '../src/lib/unitV2Graph.ts';

const repo = resolve(import.meta.dirname, '../..');
const catalog = JSON.parse(readFileSync(resolve(repo, 'test/golden/v2-inspect-atoms.json'), 'utf8')) as AtomCatalog;

const created = createUnitV2({ name: 'browser_gain', title: 'Browser Gain' });
const createdGraph = parseUnitGraphDraft(created);
const createdPorts = parseUnitPortsDraft(created);
assert.equal(createdGraph.name, 'browser_gain');
assert.equal(createdGraph.meta.title, 'Browser Gain');
assert.deepEqual(createdPorts.inputs, [{ name: 'input', type: 'audio', channels: 1, signals: [] }]);
assert.deepEqual(createdPorts.outputs, [{ name: 'output', type: 'audio', channels: 1, signals: [] }]);
assert.deepEqual(createdGraph.nodes.map(node => node.id), ['gain_value', 'apply_gain']);
assert.throws(() => createUnitV2({ name: 'Not Valid' }), /lowercase snake_case/);
assert.equal(classifyUserEffectContent(created).userPlaceable, true);
assert.doesNotThrow(() => assertUserPlaceableUnit(created));

const described = parseUnitGraphDraft(updateUnitDefinition(created, {
  title: 'Studio Gain',
  category: 'dynamics',
  description: 'A structured unit.',
  version: '1.2.0',
}));
assert.equal(described.meta.title, 'Studio Gain');
assert.equal(described.version, '1.2.0');
assert.equal(parseUnitGraphDraft(updateUnitCompatibility(created, 'wasm_realtime', false)).compatibility.wasm_realtime, false);

const withToneParam = updateUnitParam(created, null, {
  name: 'tone',
  type: 'float',
  default: '0.5',
  min: '0',
  max: '1',
  smoothingMs: '8',
  ui: { label: 'Tone', control: 'knob', unit: '%', display_precision: '2' },
});
assert.equal(parseUnitGraphDraft(withToneParam).params.at(-1)?.ui?.label, 'Tone');
const toneParam = parseUnitGraphDraft(withToneParam).params.at(-1)!;
const renamedTone = updateUnitParam(withToneParam, 'tone', { ...toneParam, name: 'color' });
assert(parseUnitGraphDraft(renamedTone).params.some(param => param.name === 'color'));
assert.equal(parseUnitGraphDraft(removeUnitParam(renamedTone, 'color')).params.length, 1);
assert.throws(() => removeUnitParam(created, 'gain'), /still used by an atom/);

const renamedInput = updateUnitPort(created, 'inputs', 0, { ...createdPorts.inputs[0], name: 'source' });
assert.equal(parseUnitPortsDraft(renamedInput).inputs[0].name, 'source');
assert.equal(parseUnitGraphDraft(renamedInput).nodes.find(node => node.id === 'apply_gain')?.in.signal_a, 'source');
assert.throws(
  () => addUnitPort(created, 'inputs', { name: 'sidechain', type: 'audio', channels: 1, signals: [] }),
  /single audio input/,
);
const withSidechain = addUnitPort(created, 'inputs', { name: 'sidechain', type: 'control', signals: [] });
assert.equal(parseUnitPortsDraft(withSidechain).inputs.at(-1)?.name, 'sidechain');
assert.equal(classifyUserEffectContent(withSidechain).userPlaceable, true);

assert.throws(
  () => updateUnitPort(created, 'outputs', 0, { ...createdPorts.outputs[0], channels: 2 }),
  /must each be mono/,
);
const stereoOutput = created.replace(
  '  outputs:\n    - name: output\n      type: audio\n      channels: 1',
  '  outputs:\n    - name: output\n      type: audio\n      channels: 2',
);
assert.equal(classifyUserEffectContent(stereoOutput).userPlaceable, false);
assert.throws(() => assertUserPlaceableUnit(stereoOutput), /must each be mono/);

const toneStack = readFileSync(resolve(repo, 'test/fixtures/units-v2/tone_stack.unit.v2.yaml'), 'utf8');
assert.deepEqual(
  parseUnitGraphDraft(toneStack).params.map(param => param.name),
  ['gain', 'bass', 'mid', 'treble', 'presence', 'volume'],
);
const reorderedToneStack = moveUnitParam(toneStack, 'volume', 4);
const reorderedToneGraph = parseUnitGraphDraft(reorderedToneStack);
assert.deepEqual(
  reorderedToneGraph.params.map(param => param.name),
  ['gain', 'bass', 'mid', 'treble', 'volume', 'presence'],
);
assert.equal(reorderedToneGraph.params[4].default, '0.65');
assert.deepEqual(reorderedToneGraph.nodes.map(node => node.id), parseUnitGraphDraft(toneStack).nodes.map(node => node.id));
assert.throws(() => moveUnitParam(toneStack, 'missing_param', 0), /was not found/);

const configuredNode = { ...createdGraph.nodes[0], config: { ...createdGraph.nodes[0].config, value: '0.5' } };
const configured = serializeUnitGraphNodeUpdate(created, configuredNode);
assert.equal(parseUnitGraphDraft(configured).nodes[0].config.value, '0.5');
const positionedAtom = parseUnitGraphDraft(setAtomNodePosition(created, 'gain_value', { x: 15, y: 30 })).nodes[0];
assert.deepEqual(positionedAtom.ui?.position, { x: 15, y: 30 });

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

const added = addAtomNodeToUnit(created, catalog, 'amplitude_clip_hard', { x: 120, y: 240 });
const addedGraph = parseUnitGraphDraft(added.content);
const addedNode = addedGraph.nodes.find(node => node.id === added.id);
assert(addedNode);
assert.deepEqual(Object.keys(addedNode.in), ['signal']);
assert.deepEqual(Object.keys(addedNode.out), ['signal']);
assert.deepEqual(Object.keys(addedNode.config), ['threshold']);
assert.equal(addedNode.config.threshold, '1');
assert.deepEqual(addedNode.ui?.position, { x: 120, y: 240 });
const malformedLegacyFn = added.content.replace(
  'atom: amplitude_clip_hard',
  'atom: amplitude_clip_hard\n      fn: amplitude_clip_soft',
);
const malformedNode = parseUnitGraphDraft(malformedLegacyFn).nodes.find(node => node.id === added.id);
assert.equal(malformedNode?.atom, 'amplitude_clip_hard');
const normalizedMalformed = serializeUnitGraphNodeUpdate(malformedLegacyFn, {
  ...malformedNode!,
  config: { ...malformedNode!.config, threshold: '0.7' },
});
assert.equal(parseUnitGraphDraft(normalizedMalformed).nodes.find(node => node.id === added.id)?.atom, 'amplitude_clip_hard');
assert(!normalizedMalformed.includes('fn: amplitude_clip_soft'));
const removedAdded = parseUnitGraphDraft(removeAtomNodeFromUnit(added.content, added.id));
assert(!removedAdded.nodes.some(node => node.id === added.id));
assert(!removedAdded.signals.some(signal => signal.startsWith(`${added.id}_`)));

const pastedAdded = pasteAtomNodeIntoUnit(added.content, addedNode);
const pastedNode = parseUnitGraphDraft(pastedAdded.content).nodes.find(node => node.id === pastedAdded.id);
assert(pastedNode);
assert.deepEqual(pastedNode.in, { signal: '' });
assert.notEqual(pastedNode.out.signal, addedNode.out.signal);
assert.deepEqual(pastedNode.ui?.position, { x: 152, y: 272 });

const bridgeUnit = `kind: apg.unit
schema: apg.unit.v2
name: bridge_unit
version: 1.0.0
meta:
  unit_family: utility
ports:
  inputs:
    - name: input
      type: audio
      channels: 1
  outputs:
    - name: output
      type: audio
      channels: 1
graph:
  signals: [input, clipped, output]
  nodes:
    - id: first
      atom: amplitude_clip_hard
      in: { signal: input }
      out: { signal: clipped }
      config: { threshold: 1 }
    - id: second
      atom: amplitude_clip_soft
      in: { signal: clipped }
      out: { signal: output }
      config: { threshold: 1, curve: 0 }
`;
const bridgedAtom = removeAtomNodeWithTopology(bridgeUnit, catalog, 'first');
assert.equal(bridgedAtom.mode, 'bridged');
assert.equal(bridgedAtom.bridgedConnections, 1);
assert.equal(parseUnitGraphDraft(bridgedAtom.content).nodes[0].in.signal, 'input');
const bridgedPublicOutput = removeAtomNodeWithTopology(bridgeUnit, catalog, 'second');
assert.equal(bridgedPublicOutput.mode, 'bridged');
assert.deepEqual(parseUnitPortsDraft(bridgedPublicOutput.content).outputs[0].signals, ['clipped']);

const disconnectedSpecial = removeAtomNodeWithTopology(created, catalog, 'apply_gain');
assert.equal(disconnectedSpecial.mode, 'disconnected');
assert.deepEqual(parseUnitGraphDraft(disconnectedSpecial.content).nodes.map(node => node.id), ['gain_value']);

const replacementPreview = previewAtomReplacement(added.content, catalog, added.id, 'amplitude_clip_soft');
assert.deepEqual(replacementPreview.preservedInputs, ['signal']);
assert.deepEqual(replacementPreview.preservedOutputs, ['signal']);
assert.deepEqual(replacementPreview.preservedConfig, ['threshold']);
const replacedAtom = replaceAtomNodeInUnit(added.content, catalog, added.id, 'amplitude_clip_soft');
const replacedGraph = parseUnitGraphDraft(replacedAtom.content);
const replacementNode = replacedGraph.nodes.find(node => node.id === replacedAtom.id);
assert(replacementNode);
assert.notEqual(replacementNode.id, added.id);
assert.equal(replacementNode.atom, 'amplitude_clip_soft');
assert.equal(replacementNode.config.threshold, addedNode.config.threshold);
assert.equal(replacementNode.config.curve, '0');
assert.deepEqual(replacementNode.ui?.position, { x: 120, y: 240 });

const incompatibleReplacement = replaceAtomNodeInUnit(added.content, catalog, added.id, 'generation_dc', true);
assert.equal(incompatibleReplacement.id, added.id);
assert(incompatibleReplacement.preview.removedInputs.some(item => item.field === 'signal'));
assert(incompatibleReplacement.preview.removedConfig.some(item => item.field === 'threshold'));
const incompatibleNode = parseUnitGraphDraft(incompatibleReplacement.content).nodes.find(node => node.id === added.id);
assert(incompatibleNode);
assert.equal(incompatibleNode.atom, 'generation_dc');
assert.deepEqual(Object.keys(incompatibleNode.in), []);
assert.deepEqual(Object.keys(incompatibleNode.out), ['signal']);
assert.deepEqual(Object.keys(incompatibleNode.config), ['value']);
assert.equal(incompatibleNode.config.value, '0');

const matrix = addAtomNodeToUnit(created, catalog, 'mix_matrix');
const matrixNode = parseUnitGraphDraft(matrix.content).nodes.find(node => node.id === matrix.id);
assert(matrixNode);
assert.equal(matrixNode.config.coefficients, '[[1]]');

const overdrive = readFileSync(resolve(repo, 'test/fixtures/units-v2/overdrive.unit.v2.yaml'), 'utf8');
assert.throws(() => removeAtomNodeFromUnit(overdrive, 'tone_filter'), /apply_level consumes/);
assert.throws(() => removeAtomNodeFromUnit(overdrive, 'apply_level'), /unit output output references/);

const disconnectedInput = disconnectUnitInput(created, { nodeId: 'apply_gain', field: 'signal_a' });
assert.equal(parseUnitGraphDraft(disconnectedInput).nodes[1].in.signal_a, '');
const connectedInput = connectUnitNodes(
  disconnectedInput,
  catalog,
  { nodeId: 'gain_value', field: 'signal' },
  { nodeId: 'apply_gain', field: 'signal_a' },
);
assert.equal(parseUnitGraphDraft(connectedInput).nodes[1].in.signal_a, 'gain_value');
assert.throws(
  () => connectUnitNodes(created, catalog, { nodeId: 'gain_value', field: 'signal' }, { nodeId: 'apply_gain', field: 'signal_a' }),
  /already connected/,
);
assert.throws(
  () => connectUnitNodes(created, catalog, { nodeId: 'missing', field: 'signal' }, { nodeId: 'apply_gain', field: 'signal_a' }),
  /was not found/,
);

const alternateSource = addAtomNodeToUnit(created, catalog, 'generation_dc');
const originalApplyGainInput = parseUnitGraphDraft(created).nodes.find(node => node.id === 'apply_gain')?.in.signal_a;
const reconnectedInput = reconnectUnitConnection(
  alternateSource.content,
  catalog,
  { nodeId: 'apply_gain', field: 'signal_a' },
  { nodeId: alternateSource.id, field: 'signal' },
  { nodeId: 'apply_gain', field: 'signal_a' },
);
const alternateSignal = parseUnitGraphDraft(alternateSource.content).nodes.find(node => node.id === alternateSource.id)?.out.signal;
assert.equal(parseUnitGraphDraft(reconnectedInput).nodes.find(node => node.id === 'apply_gain')?.in.signal_a, alternateSignal);
assert.throws(
  () => reconnectUnitConnection(
    created,
    catalog,
    { nodeId: 'apply_gain', field: 'signal_a' },
    { nodeId: 'apply_gain', field: 'signal' },
    { nodeId: 'apply_gain', field: 'signal_a' },
  ),
  /creates a cycle/,
);
assert.equal(parseUnitGraphDraft(created).nodes.find(node => node.id === 'apply_gain')?.in.signal_a, originalApplyGainInput);

const insertedConnection = insertAtomNodeOnConnection(
  created,
  catalog,
  'amplitude_clip_soft',
  { nodeId: 'apply_gain', field: 'signal_b' },
  { x: 80, y: 40 },
);
const insertedGraph = parseUnitGraphDraft(insertedConnection.content);
const insertedNode = insertedGraph.nodes.find(node => node.id === insertedConnection.id);
assert(insertedNode);
assert.equal(insertedNode.in.signal, createdGraph.nodes.find(node => node.id === 'apply_gain')?.in.signal_b);
assert.equal(insertedGraph.nodes.find(node => node.id === 'apply_gain')?.in.signal_b, insertedNode.out.signal);
assert.deepEqual(insertedNode.ui?.position, { x: 80, y: 40 });
assert.throws(
  () => insertAtomNodeOnConnection(
    created,
    catalog,
    'delay_tap_feedback',
    { nodeId: 'apply_gain', field: 'signal_b' },
  ),
  /no compatible input\/output pair/,
);

const firstClip = addAtomNodeToUnit(created, catalog, 'amplitude_clip_hard');
let routedClips = replaceUnitConnection(
  firstClip.content,
  catalog,
  { nodeId: 'apply_gain', field: 'signal' },
  { nodeId: firstClip.id, field: 'signal' },
);
assert.throws(
  () => replaceUnitConnection(
    routedClips,
    catalog,
    { nodeId: firstClip.id, field: 'signal' },
    { nodeId: 'apply_gain', field: 'signal_a' },
  ),
  /creates a cycle/,
);
const secondClip = addAtomNodeToUnit(routedClips, catalog, 'amplitude_clip_hard');
routedClips = moveUnitConnection(
  secondClip.content,
  catalog,
  { nodeId: firstClip.id, field: 'signal' },
  { nodeId: secondClip.id, field: 'signal' },
);
const movedGraph = parseUnitGraphDraft(routedClips);
assert.equal(movedGraph.nodes.find(node => node.id === firstClip.id)?.in.signal, '');
assert.equal(movedGraph.nodes.find(node => node.id === secondClip.id)?.in.signal, 'output');

const bufferTarget = addAtomNodeToUnit(created, catalog, 'delay_tap_feedback');
assert.throws(
  () => connectUnitNodes(
    bufferTarget.content,
    catalog,
    { nodeId: 'gain_value', field: 'signal' },
    { nodeId: bufferTarget.id, field: 'buffer' },
  ),
  /signal output to buffer input/,
);

console.log('unit v2 graph transformer tests passed');
