import yaml from 'js-yaml';

import type {
  Compatibility,
  ProjectInspect,
  ProjectInstance,
  ProjectRoute,
  ProjectScene,
  ProjectUnit,
} from './backendSamples';

export type ProjectUnitRefDraft = { id: string; file: string };
export type GraphPosition = { x: number; y: number };
export type ProjectInstanceDraft = { id: string; unit: string; params: Record<string, string>; ui?: { position?: GraphPosition } };
export type ProjectRouteDraft = { from: string; to: string };
export type ProjectSceneDraft = {
  name: string;
  params: Record<string, string>;
  bypass: Record<string, boolean>;
};

export type ProjectGraphDraft = {
  name: string;
  version: string;
  units: ProjectUnitRefDraft[];
  nodes: ProjectInstanceDraft[];
  routes: ProjectRouteDraft[];
  scenes: ProjectSceneDraft[];
  targets: { default: string; export: string[] };
};

export type ProjectPortCatalog = Record<string, { inputs: string[]; outputs: string[] }>;

type ProjectDocument = Record<string, unknown>;

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function loadDocument(content: string): ProjectDocument {
  const doc = yaml.load(content);
  if (!isObject(doc)) throw new Error('Project YAML must be a mapping.');
  return doc;
}

function dumpDocument(doc: ProjectDocument): string {
  return yaml.dump(doc, { lineWidth: 120, noRefs: true, quotingType: '"' });
}

function stringMap(value: unknown): Record<string, string> {
  if (!isObject(value)) return {};
  return Object.fromEntries(Object.entries(value).map(([key, raw]) => [key, String(raw)]));
}

function booleanMap(value: unknown): Record<string, boolean> {
  if (!isObject(value)) return {};
  return Object.fromEntries(Object.entries(value).flatMap(([key, raw]) => (
    typeof raw === 'boolean' ? [[key, raw]] : []
  )));
}

function parsePosition(value: unknown): GraphPosition | undefined {
  if (!isObject(value)) return undefined;
  const x = Number(value.x);
  const y = Number(value.y);
  return Number.isFinite(x) && Number.isFinite(y) ? { x, y } : undefined;
}

function parseEndpoint(endpoint: string): { instance: string; port: string } {
  const separator = endpoint.indexOf('.');
  if (separator < 1 || separator === endpoint.length - 1) throw new Error(`Invalid project endpoint "${endpoint}".`);
  return { instance: endpoint.slice(0, separator), port: endpoint.slice(separator + 1) };
}

function ensureChain(doc: ProjectDocument): Record<string, unknown> {
  let chain: Record<string, unknown>;
  if (isObject(doc.chain)) {
    chain = doc.chain;
  } else {
    chain = {};
    doc.chain = chain;
  }
  if (!Array.isArray(chain.nodes)) chain.nodes = [];
  if (!Array.isArray(chain.routes)) chain.routes = [];
  return chain;
}

function uniqueId(existing: Set<string>, base: string): string {
  let id = base;
  let suffix = 1;
  while (existing.has(id)) id = `${base}_${++suffix}`;
  return id;
}

export function parseProjectGraphDraft(content: string): ProjectGraphDraft {
  const doc = loadDocument(content);
  const chain = isObject(doc.chain) ? doc.chain : {};
  const targets = isObject(doc.targets) ? doc.targets : {};
  return {
    name: String(doc.name ?? 'unnamed_project'),
    version: String(doc.version ?? '1.0.0'),
    units: (Array.isArray(doc.units) ? doc.units : []).filter(isObject).map(unit => ({
      id: String(unit.id ?? ''),
      file: String(unit.file ?? ''),
    })),
    nodes: (Array.isArray(chain.nodes) ? chain.nodes : []).filter(isObject).map(node => ({
      id: String(node.id ?? ''),
      unit: String(node.unit ?? ''),
      params: stringMap(node.params),
      ui: isObject(node.ui) ? { position: parsePosition(node.ui.position) } : undefined,
    })),
    routes: (Array.isArray(chain.routes) ? chain.routes : []).filter(isObject).map(route => ({
      from: String(route.from ?? ''),
      to: String(route.to ?? ''),
    })),
    scenes: (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject).map(scene => ({
      name: String(scene.name ?? ''),
      params: stringMap(scene.params),
      bypass: booleanMap(scene.bypass),
    })),
    targets: {
      default: String(targets.default ?? 'desktop_full'),
      export: (Array.isArray(targets.export) ? targets.export : []).map(String),
    },
  };
}

export function projectDraftToInspect(
  draft: ProjectGraphDraft,
  baseline: ProjectInspect,
  file = baseline.file,
): ProjectInspect {
  const baselineUnits = new Map(baseline.units.map(unit => [unit.id, unit]));
  const units: ProjectUnit[] = draft.units.map(reference => baselineUnits.get(reference.id) ?? {
    ...reference,
    name: reference.id,
    compatibility: {} as Compatibility,
  });
  const nodes: ProjectInstance[] = draft.nodes.map(node => ({
    id: node.id,
    unit: node.unit,
    params: Object.entries(node.params).map(([key, value]) => ({ key, value })),
  }));
  const routes: ProjectRoute[] = draft.routes.map(route => ({ ...route }));
  const scenes: ProjectScene[] = draft.scenes.map(scene => ({
    name: scene.name,
    params: Object.entries(scene.params).map(([key, value]) => ({ key, value })),
    bypass: { ...scene.bypass },
  }));
  return {
    ...baseline,
    file,
    name: draft.name,
    version: draft.version,
    units,
    nodes,
    routes,
    scenes,
    targets: draft.targets,
  };
}

export function parseUnitPortNames(content: string): { inputs: string[]; outputs: string[] } {
  const doc = yaml.load(content);
  if (!isObject(doc) || !isObject(doc.ports)) throw new Error('Unit ports must be a mapping.');
  const names = (value: unknown) => (Array.isArray(value) ? value : []).filter(isObject).map(port => String(port.name ?? ''));
  return { inputs: names(doc.ports.inputs), outputs: names(doc.ports.outputs) };
}

export function addProjectUnitReference(content: string, id: string, file: string): string {
  if (!/^[a-z][a-z0-9_]*$/.test(id)) throw new Error('Unit reference id must use lowercase snake_case.');
  if (!file || file.startsWith('/') || file.includes('\\') || file.includes(':')) {
    throw new Error('Unit reference file must be a confined relative path.');
  }
  const doc = loadDocument(content);
  const units = (Array.isArray(doc.units) ? doc.units : []).filter(isObject);
  if (units.some(unit => String(unit.id) === id)) throw new Error(`Project unit "${id}" already exists.`);
  if (units.some(unit => String(unit.file) === file)) throw new Error(`Project unit file "${file}" is already referenced.`);
  units.push({ id, file });
  doc.units = units;
  return dumpDocument(doc);
}

export function addProjectInstance(
  content: string,
  unit: string,
  requestedId: string,
  params: Record<string, string> = {},
  position?: GraphPosition,
): { content: string; id: string } {
  const doc = loadDocument(content);
  const draft = parseProjectGraphDraft(content);
  if (!draft.units.some(reference => reference.id === unit)) throw new Error(`Project unit "${unit}" was not found.`);
  if (!/^[a-z][a-z0-9_]*$/.test(requestedId)) throw new Error('Instance id must use lowercase snake_case.');
  if (draft.nodes.some(node => node.id === requestedId)) throw new Error(`Project instance "${requestedId}" already exists.`);
  const chain = ensureChain(doc);
  const node: Record<string, unknown> = { id: requestedId, unit, params: { ...params } };
  if (position) node.ui = { position };
  (chain.nodes as unknown[]).push(node);
  return { content: dumpDocument(doc), id: requestedId };
}

export function insertProjectInstanceOnRoute(
  content: string,
  ports: ProjectPortCatalog,
  unit: string,
  requestedId: string,
  routeIndex: number,
  params: Record<string, string> = {},
  position?: GraphPosition,
): { content: string; id: string } {
  const draft = parseProjectGraphDraft(content);
  const route = draft.routes[routeIndex];
  if (!route) throw new Error(`Project route ${routeIndex} was not found.`);
  const unitPorts = ports[unit];
  if (!unitPorts || unitPorts.inputs.length !== 1 || unitPorts.outputs.length !== 1) {
    throw new Error(`Project unit "${unit}" must have exactly one input and one output for route insertion.`);
  }
  const added = addProjectInstance(content, unit, requestedId, params, position);
  const doc = loadDocument(added.content);
  const chain = ensureChain(doc);
  const routes = (chain.routes as unknown[]).filter(isObject);
  routes.splice(routeIndex, 1,
    { from: route.from, to: `${added.id}.${unitPorts.inputs[0]}` },
    { from: `${added.id}.${unitPorts.outputs[0]}`, to: route.to });
  chain.routes = routes;
  const next = dumpDocument(doc);
  validateProjectRoutes(next, ports);
  return { content: next, id: added.id };
}

export function duplicateProjectInstance(content: string, instanceId: string): { content: string; id: string } {
  const draft = parseProjectGraphDraft(content);
  const source = draft.nodes.find(node => node.id === instanceId);
  if (!source) throw new Error(`Project instance "${instanceId}" was not found.`);
  const id = uniqueId(new Set(draft.nodes.map(node => node.id)), `${source.id}_copy`);
  return addProjectInstance(content, source.unit, id, source.params);
}

export function renameProjectInstance(content: string, instanceId: string, nextId: string): string {
  if (!/^[a-z][a-z0-9_]*$/.test(nextId)) throw new Error('Instance id must use lowercase snake_case.');
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const nodes = (chain.nodes as unknown[]).filter(isObject);
  const node = nodes.find(item => String(item.id) === instanceId);
  if (!node) throw new Error(`Project instance "${instanceId}" was not found.`);
  if (nodes.some(item => String(item.id) === nextId)) throw new Error(`Project instance "${nextId}" already exists.`);
  node.id = nextId;
  for (const route of (chain.routes as unknown[]).filter(isObject)) {
    for (const key of ['from', 'to']) {
      const endpoint = String(route[key] ?? '');
      if (endpoint.startsWith(`${instanceId}.`)) route[key] = `${nextId}${endpoint.slice(instanceId.length)}`;
    }
  }
  for (const scene of (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject)) {
    if (isObject(scene.params)) {
      scene.params = Object.fromEntries(Object.entries(scene.params).map(([path, value]) => [
        path.startsWith(`${instanceId}.`) ? `${nextId}${path.slice(instanceId.length)}` : path,
        value,
      ]));
    }
    if (isObject(scene.bypass) && instanceId in scene.bypass) {
      scene.bypass = Object.fromEntries(Object.entries(scene.bypass).map(([id, value]) => [
        id === instanceId ? nextId : id,
        value,
      ]));
    }
  }
  return dumpDocument(doc);
}

export function removeProjectInstance(content: string, instanceId: string): string {
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const nodes = (chain.nodes as unknown[]).filter(isObject);
  if (!nodes.some(node => String(node.id) === instanceId)) throw new Error(`Project instance "${instanceId}" was not found.`);
  chain.nodes = nodes.filter(node => String(node.id) !== instanceId);
  chain.routes = (chain.routes as unknown[]).filter(isObject).filter(route =>
    !String(route.from ?? '').startsWith(`${instanceId}.`) && !String(route.to ?? '').startsWith(`${instanceId}.`));
  for (const scene of (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject)) {
    if (isObject(scene.params)) {
      scene.params = Object.fromEntries(
        Object.entries(scene.params).filter(([path]) => !path.startsWith(`${instanceId}.`)),
      );
    }
    if (isObject(scene.bypass)) {
      scene.bypass = Object.fromEntries(Object.entries(scene.bypass).filter(([id]) => id !== instanceId));
    }
  }
  return dumpDocument(doc);
}

export function moveProjectInstance(content: string, instanceId: string, nextIndex: number): string {
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const nodes = (chain.nodes as unknown[]).filter(isObject);
  const index = nodes.findIndex(node => String(node.id) === instanceId);
  if (index < 0) throw new Error(`Project instance "${instanceId}" was not found.`);
  const bounded = Math.max(0, Math.min(nodes.length - 1, nextIndex));
  const [node] = nodes.splice(index, 1);
  nodes.splice(bounded, 0, node);
  chain.nodes = nodes;
  return dumpDocument(doc);
}

export function setProjectInstancePosition(content: string, instanceId: string, position: GraphPosition): string {
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const nodes = (chain.nodes as unknown[]).filter(isObject);
  const node = nodes.find(item => String(item.id) === instanceId);
  if (!node) throw new Error(`Project instance "${instanceId}" was not found.`);
  node.ui = { ...(isObject(node.ui) ? node.ui : {}), position };
  chain.nodes = nodes;
  return dumpDocument(doc);
}

function validateSceneSnapshot(
  draft: ProjectGraphDraft,
  params: Record<string, string>,
  bypass: Record<string, boolean>,
): void {
  const instances = new Set(draft.nodes.map(node => node.id));
  for (const path of Object.keys(params)) {
    const { instance } = parseEndpoint(path);
    if (!instances.has(instance)) throw new Error(`Scene parameter "${path}" references a missing instance.`);
  }
  for (const instance of Object.keys(bypass)) {
    if (!instances.has(instance)) throw new Error(`Scene bypass references missing instance "${instance}".`);
  }
}

export function upsertProjectScene(
  content: string,
  name: string,
  params: Record<string, string>,
  bypass: Record<string, boolean>,
): string {
  const sceneName = name.trim();
  if (!sceneName) throw new Error('Scene name is required.');
  const draft = parseProjectGraphDraft(content);
  validateSceneSnapshot(draft, params, bypass);
  const doc = loadDocument(content);
  const scenes = (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject);
  const scene = scenes.find(item => String(item.name) === sceneName);
  const snapshot = { name: sceneName, params: { ...params }, bypass: { ...bypass } };
  if (scene) Object.assign(scene, snapshot);
  else scenes.push(snapshot);
  doc.scenes = scenes;
  return dumpDocument(doc);
}

export function renameProjectScene(content: string, name: string, nextName: string): string {
  const sceneName = nextName.trim();
  if (!sceneName) throw new Error('Scene name is required.');
  const doc = loadDocument(content);
  const scenes = (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject);
  const scene = scenes.find(item => String(item.name) === name);
  if (!scene) throw new Error(`Scene "${name}" was not found.`);
  if (scenes.some(item => item !== scene && String(item.name) === sceneName)) {
    throw new Error(`Scene "${sceneName}" already exists.`);
  }
  scene.name = sceneName;
  doc.scenes = scenes;
  return dumpDocument(doc);
}

export function removeProjectScene(content: string, name: string): string {
  const doc = loadDocument(content);
  const scenes = (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject);
  if (!scenes.some(scene => String(scene.name) === name)) throw new Error(`Scene "${name}" was not found.`);
  doc.scenes = scenes.filter(scene => String(scene.name) !== name);
  return dumpDocument(doc);
}

export function applyProjectScene(
  content: string,
  name: string,
): { content: string; bypass: Record<string, boolean> } {
  const draft = parseProjectGraphDraft(content);
  const scene = draft.scenes.find(item => item.name === name);
  if (!scene) throw new Error(`Scene "${name}" was not found.`);
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const nodes = (chain.nodes as unknown[]).filter(isObject);
  for (const [path, value] of Object.entries(scene.params)) {
    const { instance, port: param } = parseEndpoint(path);
    const node = nodes.find(item => String(item.id) === instance);
    if (!node) throw new Error(`Scene parameter "${path}" references a missing instance.`);
    node.params = { ...(isObject(node.params) ? node.params : {}), [param]: value };
  }
  chain.nodes = nodes;
  return { content: dumpDocument(doc), bypass: { ...scene.bypass } };
}

function validateRoute(
  draft: ProjectGraphDraft,
  ports: ProjectPortCatalog,
  route: ProjectRouteDraft,
  replacedIndex?: number,
): void {
  const source = parseEndpoint(route.from);
  const target = parseEndpoint(route.to);
  if (source.instance === 'system') {
    if (source.port !== 'input') throw new Error('Only system.input can be a route source.');
  } else {
    const node = draft.nodes.find(item => item.id === source.instance);
    if (!node) throw new Error(`Route source instance "${source.instance}" was not found.`);
    if (!ports[node.unit]?.outputs.includes(source.port)) throw new Error(`"${route.from}" is not a unit output.`);
  }
  if (target.instance === 'system') {
    if (target.port !== 'output') throw new Error('Only system.output can be a route target.');
  } else {
    const node = draft.nodes.find(item => item.id === target.instance);
    if (!node) throw new Error(`Route target instance "${target.instance}" was not found.`);
    if (!ports[node.unit]?.inputs.includes(target.port)) throw new Error(`"${route.to}" is not a unit input.`);
  }
  if (draft.routes.some((item, index) => index !== replacedIndex && item.to === route.to)) {
    throw new Error(`Route target "${route.to}" is already connected.`);
  }
  const adjacency = new Map<string, Set<string>>();
  const candidates = draft.routes.filter((_, index) => index !== replacedIndex).concat(route);
  for (const item of candidates) {
    const from = parseEndpoint(item.from).instance;
    const to = parseEndpoint(item.to).instance;
    if (from === 'system' || to === 'system') continue;
    const targets = adjacency.get(from) ?? new Set<string>();
    targets.add(to);
    adjacency.set(from, targets);
  }
  for (const origin of draft.nodes.map(node => node.id)) {
    const pending = [...(adjacency.get(origin) ?? [])];
    const visited = new Set<string>();
    while (pending.length > 0) {
      const current = pending.pop();
      if (!current) continue;
      if (current === origin) throw new Error(`Route ${route.from} -> ${route.to} creates a cycle.`);
      if (visited.has(current)) continue;
      visited.add(current);
      for (const next of adjacency.get(current) ?? []) pending.push(next);
    }
  }
}

export function validateProjectRoutes(content: string, ports: ProjectPortCatalog): void {
  const draft = parseProjectGraphDraft(content);
  draft.routes.forEach((route, index) => validateRoute(draft, ports, route, index));
}

export function addProjectRoute(
  content: string,
  ports: ProjectPortCatalog,
  route: ProjectRouteDraft,
): string {
  const doc = loadDocument(content);
  const draft = parseProjectGraphDraft(content);
  validateRoute(draft, ports, route);
  (ensureChain(doc).routes as unknown[]).push({ ...route });
  return dumpDocument(doc);
}

export function replaceProjectRoute(
  content: string,
  ports: ProjectPortCatalog,
  index: number,
  route: ProjectRouteDraft,
): string {
  const doc = loadDocument(content);
  const draft = parseProjectGraphDraft(content);
  if (!draft.routes[index]) throw new Error(`Project route ${index} was not found.`);
  validateRoute(draft, ports, route, index);
  (ensureChain(doc).routes as unknown[])[index] = { ...route };
  return dumpDocument(doc);
}

export function removeProjectRoute(content: string, index: number): string {
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const routes = (chain.routes as unknown[]).filter(isObject);
  if (!routes[index]) throw new Error(`Project route ${index} was not found.`);
  chain.routes = routes.filter((_, routeIndex) => routeIndex !== index);
  return dumpDocument(doc);
}

export function moveProjectRoute(content: string, index: number, nextIndex: number): string {
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const routes = (chain.routes as unknown[]).filter(isObject);
  if (!routes[index]) throw new Error(`Project route ${index} was not found.`);
  const bounded = Math.max(0, Math.min(routes.length - 1, nextIndex));
  const [route] = routes.splice(index, 1);
  routes.splice(bounded, 0, route);
  chain.routes = routes;
  return dumpDocument(doc);
}
