import yaml from 'js-yaml';
import type { AtomCatalog, AtomCatalogAtom } from './backendSamples';

export type UnitParamDraft = {
  name: string;
  type: string;
  default: string;
  min?: string;
  max?: string;
  ui?: {
    label?: string;
    unit?: string;
    display_precision?: string;
  };
};

export type UnitGraphNode = {
  id: string;
  atom: string;
  in: Record<string, string>;
  out: Record<string, string>;
  config: Record<string, string>;
  ui?: {
    position?: GraphPosition;
  };
};

export type UnitGraphDraft = {
  name: string;
  params: UnitParamDraft[];
  signals: string[];
  nodes: UnitGraphNode[];
};

export type UnitConnectionEndpoint = {
  nodeId: string;
  field: string;
};

export type GraphPosition = {
  x: number;
  y: number;
};

type UnitDocument = Record<string, unknown> & {
  params?: Record<string, unknown>;
  graph?: {
    signals?: unknown[];
    nodes?: unknown[];
  };
  ports?: {
    inputs?: unknown[];
    outputs?: unknown[];
  };
};

export type CreateUnitOptions = {
  name: string;
  title?: string;
  category?: string;
};

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function loadDocument(content: string): UnitDocument {
  const doc = yaml.load(content);
  if (!isObject(doc)) throw new Error('YAML must be a mapping.');
  return doc as UnitDocument;
}

function dumpDocument(doc: Record<string, unknown>): string {
  return yaml.dump(doc, { lineWidth: 120, noRefs: true, quotingType: '"' });
}

function stringMap(value: unknown): Record<string, string> {
  if (!isObject(value)) return {};
  return Object.fromEntries(Object.entries(value).map(([key, raw]) => [key, String(raw)]));
}

function scalarString(value: unknown): string | undefined {
  if (value === undefined || value === null || isObject(value) || Array.isArray(value)) return undefined;
  return String(value);
}

function parseParam(name: string, value: unknown): UnitParamDraft {
  const raw = isObject(value) ? value : {};
  const ui = isObject(raw.ui) ? raw.ui : {};
  return {
    name,
    type: String(raw.type ?? 'float'),
    default: String(raw.default ?? '0'),
    min: scalarString(raw.min),
    max: scalarString(raw.max),
    ui: {
      label: scalarString(ui.label),
      unit: scalarString(ui.unit),
      display_precision: scalarString(ui.display_precision),
    },
  };
}

function parseNode(value: unknown, index: number): UnitGraphNode {
  const raw = isObject(value) ? value : {};
  const ui = isObject(raw.ui) ? raw.ui : {};
  const position = isObject(ui.position) ? ui.position : {};
  const x = Number(position.x);
  const y = Number(position.y);
  return {
    id: String(raw.id ?? `node_${index + 1}`),
    atom: String(raw.atom ?? 'unknown'),
    in: stringMap(raw.in),
    out: stringMap(raw.out),
    config: stringMap(raw.config),
    ui: Number.isFinite(x) && Number.isFinite(y) ? { position: { x, y } } : undefined,
  };
}

function parseGraphFromDocument(doc: UnitDocument): UnitGraphDraft {
  const params = isObject(doc.params)
    ? Object.entries(doc.params).map(([name, value]) => parseParam(name, value))
    : [];
  const graph = isObject(doc.graph) ? doc.graph : {};
  const signals = Array.isArray(graph.signals)
    ? graph.signals.filter((signal): signal is string => typeof signal === 'string')
    : [];
  const nodes = Array.isArray(graph.nodes) ? graph.nodes.map(parseNode) : [];

  return {
    name: String(doc.name ?? 'unnamed_unit'),
    params,
    signals,
    nodes,
  };
}

function ensureGraph(doc: UnitDocument): NonNullable<UnitDocument['graph']> {
  if (!isObject(doc.graph)) doc.graph = {};
  if (!Array.isArray(doc.graph.signals)) doc.graph.signals = [];
  if (!Array.isArray(doc.graph.nodes)) doc.graph.nodes = [];
  return doc.graph;
}

function mapToYaml(value: Record<string, string>): Record<string, unknown> | undefined {
  if (Object.keys(value).length === 0) return undefined;
  return Object.fromEntries(Object.entries(value).map(([key, raw]) => {
    const numeric = Number(raw);
    return [key, raw.trim() !== '' && Number.isFinite(numeric) ? numeric : raw];
  }));
}

function nodeToYaml(node: UnitGraphNode): Record<string, unknown> {
  const raw: Record<string, unknown> = { id: node.id, atom: node.atom };
  const inputs = mapToYaml(node.in);
  const outputs = mapToYaml(node.out);
  const config = mapToYaml(node.config);
  if (inputs) raw.in = inputs;
  if (outputs) raw.out = outputs;
  if (config) raw.config = config;
  if (node.ui?.position) raw.ui = { position: node.ui.position };
  return raw;
}

function uniqueName(existing: Set<string>, base: string): string {
  let index = 1;
  let candidate = base;
  while (existing.has(candidate)) {
    index += 1;
    candidate = `${base}_${index}`;
  }
  existing.add(candidate);
  return candidate;
}

function atomDefaultValue(type: string): string {
  return type === 'int' || type === 'uint' || type === 'bool' ? '0' : '0.0';
}

function catalogNode(catalog: AtomCatalog, node: UnitGraphNode): AtomCatalogAtom {
  const atom = catalog.atoms.find(item => item.name === node.atom);
  if (!atom) throw new Error(`Atom "${node.atom}" is not available in the catalog.`);
  return atom;
}

function compatibleFields(source: AtomCatalogAtom['outputs'][number], target: AtomCatalogAtom['inputs'][number]): boolean {
  return source.type === target.type &&
    (source.buffer_samples === undefined || target.buffer_samples === undefined || source.buffer_samples === target.buffer_samples);
}

function wouldCreateCycle(nodes: UnitGraphNode[], sourceId: string, targetId: string, replacedTarget?: UnitConnectionEndpoint): boolean {
  if (sourceId === targetId) return true;
  const producerBySignal = new Map<string, string>();
  for (const node of nodes) {
    for (const signal of Object.values(node.out)) if (signal) producerBySignal.set(signal, node.id);
  }
  const adjacency = new Map<string, Set<string>>();
  for (const node of nodes) {
    for (const [field, signal] of Object.entries(node.in)) {
      if (replacedTarget?.nodeId === node.id && replacedTarget.field === field) continue;
      const producer = producerBySignal.get(signal);
      if (!producer) continue;
      const targets = adjacency.get(producer) ?? new Set<string>();
      targets.add(node.id);
      adjacency.set(producer, targets);
    }
  }
  const pending = [targetId];
  const visited = new Set<string>();
  while (pending.length > 0) {
    const current = pending.pop();
    if (!current || visited.has(current)) continue;
    if (current === sourceId) return true;
    visited.add(current);
    for (const next of adjacency.get(current) ?? []) pending.push(next);
  }
  return false;
}

function connectUnitNodesInternal(
  content: string,
  catalog: AtomCatalog,
  source: UnitConnectionEndpoint,
  target: UnitConnectionEndpoint,
  replace: boolean,
): string {
  const doc = loadDocument(content);
  const draft = parseGraphFromDocument(doc);
  const sourceNode = draft.nodes.find(node => node.id === source.nodeId);
  const targetNode = draft.nodes.find(node => node.id === target.nodeId);
  if (!sourceNode) throw new Error(`Source atom node "${source.nodeId}" was not found.`);
  if (!targetNode) throw new Error(`Target atom node "${target.nodeId}" was not found.`);
  const sourceField = catalogNode(catalog, sourceNode).outputs.find(field => field.name === source.field);
  const targetField = catalogNode(catalog, targetNode).inputs.find(field => field.name === target.field);
  if (!sourceField) throw new Error(`"${source.nodeId}.${source.field}" is not an atom output.`);
  if (!targetField) throw new Error(`"${target.nodeId}.${target.field}" is not an atom input.`);
  if (!compatibleFields(sourceField, targetField)) {
    throw new Error(`Cannot connect ${sourceField.type} output to ${targetField.type} input.`);
  }
  const signal = sourceNode.out[source.field];
  if (!signal) throw new Error(`Source output "${source.nodeId}.${source.field}" has no signal.`);
  if (!replace && targetNode.in[target.field]) {
    throw new Error(`Target input "${target.nodeId}.${target.field}" is already connected.`);
  }
  if (wouldCreateCycle(draft.nodes, source.nodeId, target.nodeId, target)) {
    throw new Error(`Connection ${source.nodeId}.${source.field} -> ${target.nodeId}.${target.field} creates a cycle.`);
  }
  targetNode.in[target.field] = signal;
  return serializeUnitGraphNodeUpdate(content, targetNode);
}

export function connectUnitNodes(
  content: string,
  catalog: AtomCatalog,
  source: UnitConnectionEndpoint,
  target: UnitConnectionEndpoint,
): string {
  return connectUnitNodesInternal(content, catalog, source, target, false);
}

export function replaceUnitConnection(
  content: string,
  catalog: AtomCatalog,
  source: UnitConnectionEndpoint,
  target: UnitConnectionEndpoint,
): string {
  return connectUnitNodesInternal(content, catalog, source, target, true);
}

export function disconnectUnitInput(content: string, target: UnitConnectionEndpoint): string {
  const draft = parseUnitGraphDraft(content);
  const node = draft.nodes.find(item => item.id === target.nodeId);
  if (!node) throw new Error(`Target atom node "${target.nodeId}" was not found.`);
  if (!(target.field in node.in)) throw new Error(`"${target.nodeId}.${target.field}" is not a bound atom input.`);
  node.in[target.field] = '';
  return serializeUnitGraphNodeUpdate(content, node);
}

export function moveUnitConnection(
  content: string,
  catalog: AtomCatalog,
  from: UnitConnectionEndpoint,
  to: UnitConnectionEndpoint,
): string {
  const draft = parseUnitGraphDraft(content);
  const oldTarget = draft.nodes.find(node => node.id === from.nodeId);
  if (!oldTarget) throw new Error(`Target atom node "${from.nodeId}" was not found.`);
  const signal = oldTarget.in[from.field];
  if (!signal) throw new Error(`Target input "${from.nodeId}.${from.field}" is not connected.`);
  const source = draft.nodes.flatMap(node => Object.entries(node.out).map(([field, value]) => ({ nodeId: node.id, field, value })))
    .find(output => output.value === signal);
  if (!source) throw new Error(`Connection source for signal "${signal}" was not found.`);
  const disconnected = disconnectUnitInput(content, from);
  return connectUnitNodes(disconnected, catalog, source, to);
}

export function reconnectUnitConnection(
  content: string,
  catalog: AtomCatalog,
  from: UnitConnectionEndpoint,
  source: UnitConnectionEndpoint,
  target: UnitConnectionEndpoint,
): string {
  return connectUnitNodes(disconnectUnitInput(content, from), catalog, source, target);
}

function portSignals(value: unknown): string[] {
  if (!isObject(value)) return [];
  if (Array.isArray(value.signals)) return value.signals.filter((signal): signal is string => typeof signal === 'string');
  const name = scalarString(value.name);
  return name ? [name] : [];
}

export function createUnitV2({ name, title, category }: CreateUnitOptions): string {
  const normalized = name.trim();
  if (!/^[a-z][a-z0-9_]*$/.test(normalized)) {
    throw new Error('Unit name must use lowercase snake_case and start with a letter.');
  }
  const doc: UnitDocument = {
    kind: 'apg.unit',
    schema: 'apg.unit.v2',
    name: normalized,
    version: '1.0.0',
    meta: {
      title: title?.trim() || normalized.replace(/_/g, ' '),
      category: category?.trim() || 'custom',
      description: '',
    },
    params: {
      gain: {
        type: 'float',
        default: 1,
        min: 0,
        max: 4,
        ui: { label: 'Gain', control: 'knob', unit: 'x', scale: 'linear', display_precision: 2 },
      },
    },
    ports: {
      inputs: [{ name: 'input', type: 'audio', channels: 1 }],
      outputs: [{ name: 'output', type: 'audio', channels: 1 }],
    },
    graph: {
      signals: ['input', 'gain_value', 'output'],
      nodes: [
        { id: 'gain_value', atom: 'generation_dc', out: { signal: 'gain_value' }, config: { value: '${params.gain}' } },
        {
          id: 'apply_gain',
          atom: 'amplitude_multiply',
          in: { signal_a: 'input', signal_b: 'gain_value' },
          out: { signal: 'output' },
        },
      ],
    },
    compatibility: { desktop_full: true, wasm_realtime: true, m7_static: true, offline_render: true },
  };
  return dumpDocument(doc);
}

export function parseUnitGraphDraft(content: string): UnitGraphDraft {
  return parseGraphFromDocument(loadDocument(content));
}

export function serializeUnitGraphNodeUpdate(content: string, node: UnitGraphNode, originalId = node.id): string {
  const doc = loadDocument(content);
  const graph = ensureGraph(doc);
  const nodes = graph.nodes?.filter(isObject) ?? [];
  const index = nodes.findIndex(item => String(item.id) === originalId);
  if (index < 0) throw new Error(`Atom node "${originalId}" was not found.`);
  const duplicate = nodes.some((item, itemIndex) => itemIndex !== index && String(item.id) === node.id);
  if (duplicate) throw new Error(`Atom node id "${node.id}" is already used.`);
  nodes[index] = nodeToYaml(node);
  graph.nodes = nodes;
  return dumpDocument(doc);
}

export function addAtomNodeToUnit(
  content: string,
  catalog: AtomCatalog,
  atomName: string,
  position?: GraphPosition,
): { content: string; id: string } {
  const atom = catalog.atoms.find(item => item.name === atomName);
  if (!atom) throw new Error(`Atom "${atomName}" is not available in the catalog.`);

  const doc = loadDocument(content);
  const graph = ensureGraph(doc);
  const draft = parseGraphFromDocument(doc);
  const existingNodeIds = new Set(draft.nodes.map(node => node.id));
  const existingSignals = new Set(draft.signals);
  const id = uniqueName(existingNodeIds, atom.name.replace(/[^a-z0-9_]+/gi, '_').toLowerCase());
  const outputs = Object.fromEntries(atom.outputs.map(field => {
    const signal = uniqueName(existingSignals, `${id}_${field.name}`);
    return [field.name, signal];
  }));
  const node: UnitGraphNode = {
    id,
    atom: atom.name,
    in: Object.fromEntries(atom.inputs.map(field => [field.name, ''])),
    out: outputs,
    config: Object.fromEntries(atom.config.map(field => [field.name, atomDefaultValue(field.type)])),
    ui: position ? { position } : undefined,
  };

  graph.signals = Array.from(existingSignals);
  graph.nodes = [...(graph.nodes?.filter(isObject) ?? []), nodeToYaml(node)];
  return { content: dumpDocument(doc), id };
}

export function removeAtomNodeFromUnit(content: string, nodeId: string): string {
  const doc = loadDocument(content);
  const graph = ensureGraph(doc);
  const draft = parseGraphFromDocument(doc);
  const node = draft.nodes.find(item => item.id === nodeId);
  if (!node) throw new Error(`Atom node "${nodeId}" was not found.`);

  const outputs = new Set(Object.values(node.out).filter(Boolean));
  const publicOutputs = new Set((doc.ports?.outputs ?? []).flatMap(portSignals));
  const consumers = draft.nodes
    .filter(item => item.id !== nodeId)
    .filter(item => Object.values(item.in).some(signal => outputs.has(signal)))
    .map(item => item.id);
  if (consumers.length > 0) {
    throw new Error(`Cannot remove "${nodeId}" while ${consumers.join(', ')} consumes its output.`);
  }
  const exposed = [...outputs].filter(signal => publicOutputs.has(signal));
  if (exposed.length > 0) {
    throw new Error(`Cannot remove "${nodeId}" while unit output ${exposed.join(', ')} references its output.`);
  }

  graph.nodes = (graph.nodes?.filter(isObject) ?? []).filter(item => String(item.id) !== nodeId);
  const remainingReferences = new Set(
    parseGraphFromDocument(doc).nodes.flatMap(item => [...Object.values(item.in), ...Object.values(item.out)]).filter(Boolean),
  );
  graph.signals = (graph.signals ?? []).filter(signal => typeof signal !== 'string' || !outputs.has(signal) || remainingReferences.has(signal));
  return dumpDocument(doc);
}

export function pasteAtomNodeIntoUnit(content: string, source: UnitGraphNode): { content: string; id: string } {
  const doc = loadDocument(content);
  const graph = ensureGraph(doc);
  const draft = parseGraphFromDocument(doc);
  const existingNodeIds = new Set(draft.nodes.map(node => node.id));
  const existingSignals = new Set(draft.signals);
  const id = uniqueName(existingNodeIds, `${source.id}_copy`);
  const out = Object.fromEntries(Object.entries(source.out).map(([key, signal]) => {
    if (!signal) return [key, ''];
    return [key, uniqueName(existingSignals, `${id}_${key}`)];
  }));
  const node: UnitGraphNode = { ...source, id, out };

  graph.signals = Array.from(existingSignals);
  graph.nodes = [...(graph.nodes?.filter(isObject) ?? []), nodeToYaml(node)];
  return { content: dumpDocument(doc), id };
}

export function updateProjectInstanceParam(content: string, instanceId: string, key: string, value: string): string {
  const doc = loadDocument(content);
  const chain = isObject(doc.chain) ? doc.chain : {};
  const nodes = Array.isArray(chain.nodes) ? chain.nodes : [];
  const node = nodes.find((item): item is Record<string, unknown> => isObject(item) && String(item.id) === instanceId);
  if (!node) throw new Error(`Project node "${instanceId}" was not found.`);
  if (!isObject(node.params)) node.params = {};

  const numeric = Number(value);
  (node.params as Record<string, unknown>)[key] = value.trim() !== '' && Number.isFinite(numeric) ? numeric : value;
  return dumpDocument(doc);
}

export function findAtom(catalog: AtomCatalog, name: string): AtomCatalogAtom | null {
  return catalog.atoms.find(atom => atom.name === name) ?? null;
}
