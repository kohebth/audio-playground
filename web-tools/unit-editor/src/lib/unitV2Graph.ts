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
};

export type UnitGraphDraft = {
  name: string;
  params: UnitParamDraft[];
  signals: string[];
  nodes: UnitGraphNode[];
};

type UnitDocument = Record<string, unknown> & {
  params?: Record<string, unknown>;
  graph?: {
    signals?: unknown[];
    nodes?: unknown[];
  };
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
  return {
    id: String(raw.id ?? `node_${index + 1}`),
    atom: String(raw.atom ?? 'unknown'),
    in: stringMap(raw.in),
    out: stringMap(raw.out),
    config: stringMap(raw.config),
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

export function addAtomNodeToUnit(content: string, catalog: AtomCatalog, atomName: string): { content: string; id: string } {
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
  const consumers = draft.nodes
    .filter(item => item.id !== nodeId)
    .filter(item => Object.values(item.in).some(signal => outputs.has(signal)))
    .map(item => item.id);
  if (consumers.length > 0) {
    throw new Error(`Cannot remove "${nodeId}" while ${consumers.join(', ')} consumes its output.`);
  }

  graph.nodes = (graph.nodes?.filter(isObject) ?? []).filter(item => String(item.id) !== nodeId);
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
