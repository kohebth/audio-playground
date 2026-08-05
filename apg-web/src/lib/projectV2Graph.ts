import yaml from 'js-yaml';

import type {
  Compatibility,
  ProjectInspect,
  ProjectInstance,
  ProjectRoute,
  ProjectScene,
  ProjectUnit,
} from './backendSamples';
import { classifyUserEffectContent, parseUnitGraphDraft } from './unitV2Graph.ts';

export type ProjectUnitRefDraft = { id: string; file: string };
export type GraphPosition = { x: number; y: number };
export type ProjectRoutingRole = 'panner' | 'mixer';
export type ProjectRoutingPath = { port: string; levelParam: string };
export type ProjectRoutingContract = { role: ProjectRoutingRole; paths: ProjectRoutingPath[] };
export type ProjectInstanceDraft = {
  id: string;
  unit: string;
  params: Record<string, string>;
  routing?: { section: string };
  ui?: { position?: GraphPosition };
};
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

export type ProjectUnitPorts = {
  inputs: string[];
  outputs: string[];
  routing?: ProjectRoutingContract;
  userPlaceable?: boolean;
  reason?: string | null;
};
export type ProjectPortCatalog = Record<string, ProjectUnitPorts>;

export type ProjectInstanceClipboard = {
  unit: string;
  params: Record<string, string>;
  routing?: { section: string };
};

export type ProjectRemovalResult = {
  content: string;
  mode: 'bridged' | 'disconnected';
  bridgedRoutes: number;
};

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

function parseRoutingSection(value: unknown): { section: string } | undefined {
  if (!isObject(value) || typeof value.section !== 'string' || value.section.length === 0) return undefined;
  return { section: value.section };
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

function uniqueRoutingSection(existing: Set<string>): string {
  let suffix = 1;
  while (existing.has(`parallel_${suffix}`)) suffix += 1;
  return `parallel_${suffix}`;
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
      routing: parseRoutingSection(node.routing),
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
    routing: node.routing,
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

export function parseUnitPortNames(content: string): ProjectUnitPorts {
  const policy = classifyUserEffectContent(content);
  const routing = parseUnitGraphDraft(content).routing;
  return {
    inputs: policy.audioInputs.map(port => port.name),
    outputs: policy.audioOutputs.map(port => port.name),
    routing: routing ? { role: routing.role, paths: routing.paths.map(path => ({ ...path })) } : undefined,
    userPlaceable: routing ? false : policy.userPlaceable,
    reason: routing ? 'Routing helpers are placed by Add in parallel.' : policy.reason,
  };
}

function isUserPlaceablePorts(ports: ProjectUnitPorts | undefined): ports is ProjectUnitPorts {
  if (!ports) return false;
  return ports.userPlaceable ?? (ports.inputs.length === 1 && ports.outputs.length === 1);
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
  routing?: { section: string },
): { content: string; id: string } {
  const doc = loadDocument(content);
  const draft = parseProjectGraphDraft(content);
  if (!draft.units.some(reference => reference.id === unit)) throw new Error(`Project unit "${unit}" was not found.`);
  if (!/^[a-z][a-z0-9_]*$/.test(requestedId)) throw new Error('Instance id must use lowercase snake_case.');
  if (draft.nodes.some(node => node.id === requestedId)) throw new Error(`Project instance "${requestedId}" already exists.`);
  const chain = ensureChain(doc);
  const node: Record<string, unknown> = { id: requestedId, unit, params: { ...params } };
  if (routing) node.routing = { ...routing };
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
): { content: string; id: string } {
  const draft = parseProjectGraphDraft(content);
  const route = draft.routes[routeIndex];
  if (!route) throw new Error(`Project route ${routeIndex} was not found.`);
  const unitPorts = ports[unit];
  if (!unitPorts || unitPorts.inputs.length !== 1 || unitPorts.outputs.length !== 1) {
    throw new Error(`Project unit "${unit}" must have exactly one input and one output for route insertion.`);
  }
  const added = addProjectInstance(content, unit, requestedId, params);
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

export function insertProjectParallelOnRoute(
  content: string,
  ports: ProjectPortCatalog,
  effectUnit: string,
  effectId: string,
  pannerUnit: string,
  pannerId: string,
  mixerUnit: string,
  mixerId: string,
  routeIndex: number,
  effectParams: Record<string, string> = {},
  pannerParams?: Record<string, string>,
  mixerParams?: Record<string, string>,
): { content: string; effectId: string; pannerId: string; mixerId: string; section: string } {
  const draft = parseProjectGraphDraft(content);
  const route = draft.routes[routeIndex];
  if (!route) throw new Error(`Project route ${routeIndex} was not found.`);
  const effectPorts = ports[effectUnit];
  if (!isUserPlaceablePorts(effectPorts)) {
    throw new Error(`Parallel effect "${effectUnit}" must have exactly one input and one output.`);
  }
  const pannerPorts = ports[pannerUnit];
  const mixerPorts = ports[mixerUnit];
  if (!pannerPorts?.routing || pannerPorts.routing.role !== 'panner'
    || pannerPorts.inputs.length !== 1 || pannerPorts.outputs.length !== 2) {
    throw new Error(`Parallel panner "${pannerUnit}" must expose one input and exactly two routed outputs.`);
  }
  if (!mixerPorts?.routing || mixerPorts.routing.role !== 'mixer'
    || mixerPorts.inputs.length !== 2 || mixerPorts.outputs.length !== 1) {
    throw new Error(`Parallel mixer "${mixerUnit}" must expose exactly two routed inputs and one output.`);
  }
  const pannerPaths = pannerPorts.routing.paths;
  const mixerPaths = mixerPorts.routing.paths;
  if (pannerPaths.length !== 2 || mixerPaths.length !== 2
    || pannerPaths.some((path, index) => (
      path.port !== pannerPorts.outputs[index]
      || path.port !== mixerPaths[index]?.port
      || path.port !== mixerPorts.inputs[index]
    ))) {
    throw new Error('Parallel panner and mixer must expose the same two ordered routing paths.');
  }

  const section = uniqueRoutingSection(new Set(draft.nodes.flatMap(node => node.routing ? [node.routing.section] : [])));
  const resolvedPannerParams = pannerParams ?? Object.fromEntries(pannerPaths.map(path => [path.levelParam, '0.0']));
  const resolvedMixerParams = mixerParams ?? Object.fromEntries(mixerPaths.map(path => [path.levelParam, '-6.0206']));
  const panner = addProjectInstance(content, pannerUnit, pannerId, resolvedPannerParams, { section });
  const effect = addProjectInstance(panner.content, effectUnit, effectId, effectParams);
  const mixer = addProjectInstance(effect.content, mixerUnit, mixerId, resolvedMixerParams, { section });
  const doc = loadDocument(mixer.content);
  const chain = ensureChain(doc);
  const routes = (chain.routes as unknown[]).filter(isObject);
  routes.splice(routeIndex, 1,
    { from: route.from, to: `${pannerId}.${pannerPorts.inputs[0]}` },
    { from: `${pannerId}.${pannerPaths[0].port}`, to: `${mixerId}.${mixerPaths[0].port}` },
    { from: `${pannerId}.${pannerPaths[1].port}`, to: `${effectId}.${effectPorts.inputs[0]}` },
    { from: `${effectId}.${effectPorts.outputs[0]}`, to: `${mixerId}.${mixerPaths[1].port}` },
    { from: `${mixerId}.${mixerPorts.outputs[0]}`, to: route.to });
  chain.routes = routes;
  const next = dumpDocument(doc);
  validateProjectRoutes(next, ports);
  return { content: next, effectId, pannerId, mixerId, section };
}

export function duplicateProjectInstance(content: string, instanceId: string): { content: string; id: string } {
  const draft = parseProjectGraphDraft(content);
  const source = draft.nodes.find(node => node.id === instanceId);
  if (!source) throw new Error(`Project instance "${instanceId}" was not found.`);
  if (source.routing) throw new Error('Routing helpers can only be created through Add in parallel.');
  const id = uniqueId(new Set(draft.nodes.map(node => node.id)), `${source.id}_copy`);
  return addProjectInstance(content, source.unit, id, source.params);
}

export function copyProjectInstance(content: string, instanceId: string): ProjectInstanceClipboard {
  const source = parseProjectGraphDraft(content).nodes.find(node => node.id === instanceId);
  if (!source) throw new Error(`Project instance "${instanceId}" was not found.`);
  return { unit: source.unit, params: { ...source.params }, routing: source.routing };
}

export function pasteProjectInstance(
  content: string,
  clipboard: ProjectInstanceClipboard,
): { content: string; id: string } {
  if (clipboard.routing) throw new Error('Paste would create an unpaired routing helper; use Add in parallel instead.');
  const draft = parseProjectGraphDraft(content);
  const id = uniqueId(new Set(draft.nodes.map(node => node.id)), `${clipboard.unit.replace(/_unit$/, '')}_copy`);
  return addProjectInstance(content, clipboard.unit, id, clipboard.params);
}

export function replaceProjectInstance(
  content: string,
  ports: ProjectPortCatalog,
  instanceId: string,
  nextUnit: string,
  defaults: Record<string, string>,
): string {
  const draft = parseProjectGraphDraft(content);
  const current = draft.nodes.find(node => node.id === instanceId);
  if (!current) throw new Error(`Project instance "${instanceId}" was not found.`);
  if (!draft.units.some(unit => unit.id === nextUnit)) throw new Error(`Project unit "${nextUnit}" was not found.`);
  const currentPorts = ports[current.unit];
  const nextPorts = ports[nextUnit];
  const replacingRouting = Boolean(currentPorts?.routing || nextPorts?.routing || current.routing);
  if (replacingRouting) {
    const currentRouting = currentPorts?.routing;
    const nextRouting = nextPorts?.routing;
    const sameContract = currentRouting && nextRouting
      && currentRouting.role === nextRouting.role
      && currentRouting.paths.length === nextRouting.paths.length
      && currentRouting.paths.every((path, index) => (
        path.port === nextRouting.paths[index]?.port && path.levelParam === nextRouting.paths[index]?.levelParam
      ));
    if (!current.routing || !sameContract) {
      throw new Error('Routing helpers can only be replaced by a helper with the same role and path contract.');
    }
  } else {
    if (!isUserPlaceablePorts(currentPorts)) throw new Error('Only single-input, single-output effects can be replaced in place.');
    if (!isUserPlaceablePorts(nextPorts)) throw new Error('Replacement must have one mono audio input and one mono audio output.');
  }

  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const node = (chain.nodes as unknown[]).filter(isObject).find(item => String(item.id) === instanceId);
  if (!node) throw new Error(`Project instance "${instanceId}" was not found.`);
  node.unit = nextUnit;
  node.params = { ...defaults };
  for (const route of (chain.routes as unknown[]).filter(isObject)) {
    if (String(route.from) === `${instanceId}.${currentPorts!.outputs[0]}`) {
      route.from = `${instanceId}.${nextPorts!.outputs[0]}`;
    }
    if (String(route.to) === `${instanceId}.${currentPorts!.inputs[0]}`) {
      route.to = `${instanceId}.${nextPorts!.inputs[0]}`;
    }
  }
  for (const scene of (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject)) {
    const retained = isObject(scene.params)
      ? Object.entries(scene.params).filter(([path]) => !path.startsWith(`${instanceId}.`))
      : [];
    scene.params = Object.fromEntries([
      ...retained,
      ...Object.entries(defaults).map(([key, value]) => [`${instanceId}.${key}`, value] as const),
    ]);
  }
  const next = dumpDocument(doc);
  validateProjectRoutes(next, ports);
  return next;
}

export function rebindProjectInstanceUnit(
  content: string,
  ports: ProjectPortCatalog,
  instanceId: string,
  nextUnit: string,
): string {
  const draft = parseProjectGraphDraft(content);
  const current = draft.nodes.find(node => node.id === instanceId);
  if (!current) throw new Error(`Project instance "${instanceId}" was not found.`);
  if (current.routing) throw new Error('Routing helpers do not expose editable contracts.');
  if (!draft.units.some(unit => unit.id === nextUnit)) throw new Error(`Project unit "${nextUnit}" was not found.`);
  const currentPorts = ports[current.unit];
  const nextPorts = ports[nextUnit];
  if (!isUserPlaceablePorts(currentPorts) || !isUserPlaceablePorts(nextPorts)) {
    throw new Error('Contracts must keep one mono audio input and one mono audio output.');
  }

  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const node = (chain.nodes as unknown[]).filter(isObject).find(item => String(item.id) === instanceId);
  if (!node) throw new Error(`Project instance "${instanceId}" was not found.`);
  node.unit = nextUnit;
  for (const route of (chain.routes as unknown[]).filter(isObject)) {
    if (String(route.from) === `${instanceId}.${currentPorts.outputs[0]}`) {
      route.from = `${instanceId}.${nextPorts.outputs[0]}`;
    }
    if (String(route.to) === `${instanceId}.${currentPorts.inputs[0]}`) {
      route.to = `${instanceId}.${nextPorts.inputs[0]}`;
    }
  }
  const next = dumpDocument(doc);
  validateProjectRoutes(next, ports);
  return next;
}

export function syncProjectUnitContract(
  content: string,
  ports: ProjectPortCatalog,
  unitId: string,
  previousUnitContent: string,
  nextUnitContent: string,
): string {
  const draft = parseProjectGraphDraft(content);
  if (!draft.units.some(unit => unit.id === unitId)) throw new Error(`Project unit "${unitId}" was not found.`);
  const previousPorts = parseUnitPortNames(previousUnitContent);
  const nextPorts = parseUnitPortNames(nextUnitContent);
  if (!isUserPlaceablePorts(previousPorts) || !isUserPlaceablePorts(nextPorts)) {
    throw new Error('Contracts must keep one mono audio input and one mono audio output.');
  }

  const nextParams = parseUnitGraphDraft(nextUnitContent).params;
  const nextParamNames = new Set(nextParams.map(param => param.name));
  const doc = loadDocument(content);
  const chain = ensureChain(doc);
  const nodes = (chain.nodes as unknown[]).filter(isObject);
  const matchingNodes = nodes.filter(node => String(node.unit) === unitId);
  const matchingIds = new Set(matchingNodes.map(node => String(node.id)));
  for (const node of matchingNodes) {
    if (parseRoutingSection(node.routing)) throw new Error('Routing helpers cannot use editable contracts.');
    const existing = stringMap(node.params);
    node.params = Object.fromEntries(nextParams.map(param => [param.name, existing[param.name] ?? param.default]));
  }

  for (const route of (chain.routes as unknown[]).filter(isObject)) {
    const from = String(route.from ?? '');
    const to = String(route.to ?? '');
    for (const instanceId of matchingIds) {
      if (from === `${instanceId}.${previousPorts.outputs[0]}`) {
        route.from = `${instanceId}.${nextPorts.outputs[0]}`;
      }
      if (to === `${instanceId}.${previousPorts.inputs[0]}`) {
        route.to = `${instanceId}.${nextPorts.inputs[0]}`;
      }
    }
  }

  for (const scene of (Array.isArray(doc.scenes) ? doc.scenes : []).filter(isObject)) {
    const values = stringMap(scene.params);
    const retained = Object.fromEntries(Object.entries(values).filter(([path]) => {
      const separator = path.indexOf('.');
      if (separator < 1) return true;
      const instanceId = path.slice(0, separator);
      return !matchingIds.has(instanceId) || nextParamNames.has(path.slice(separator + 1));
    }));
    for (const node of matchingNodes) {
      const instanceId = String(node.id);
      const instanceParams = stringMap(node.params);
      for (const param of nextParams) {
        const path = `${instanceId}.${param.name}`;
        if (!(path in retained)) retained[path] = instanceParams[param.name] ?? param.default;
      }
    }
    scene.params = retained;
  }

  const next = dumpDocument(doc);
  validateProjectRoutes(next, { ...ports, [unitId]: nextPorts });
  return next;
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

export function removeProjectInstanceWithTopology(
  content: string,
  ports: ProjectPortCatalog,
  instanceId: string,
): ProjectRemovalResult {
  const draft = parseProjectGraphDraft(content);
  const instance = draft.nodes.find(node => node.id === instanceId);
  if (!instance) throw new Error(`Project instance "${instanceId}" was not found.`);
  const instancePorts = ports[instance.unit];
  const canBridge = isUserPlaceablePorts(instancePorts);
  const inputEndpoint = canBridge ? `${instanceId}.${instancePorts.inputs[0]}` : '';
  const outputEndpoint = canBridge ? `${instanceId}.${instancePorts.outputs[0]}` : '';
  const incoming = canBridge ? draft.routes.filter(route => route.to === inputEndpoint) : [];
  const outgoing = canBridge ? draft.routes.filter(route => route.from === outputEndpoint) : [];
  const bridgeSource = incoming.length === 1 ? incoming[0].from : null;

  const doc = loadDocument(removeProjectInstance(content, instanceId));
  const chain = ensureChain(doc);
  let bridgedRoutes = 0;
  if (bridgeSource) {
    const routes = (chain.routes as unknown[]).filter(isObject);
    const existing = new Set(routes.map(route => `${String(route.from)}\u0000${String(route.to)}`));
    for (const route of outgoing) {
      const key = `${bridgeSource}\u0000${route.to}`;
      if (existing.has(key)) continue;
      routes.push({ from: bridgeSource, to: route.to });
      existing.add(key);
      bridgedRoutes += 1;
    }
    chain.routes = routes;
  }
  const next = dumpDocument(doc);
  validateProjectRoutes(next, ports);
  return { content: next, mode: bridgedRoutes > 0 ? 'bridged' : 'disconnected', bridgedRoutes };
}

export function removeEmptyProjectRoutingSection(
  content: string,
  ports: ProjectPortCatalog,
  instanceId: string,
): ProjectRemovalResult {
  const draft = parseProjectGraphDraft(content);
  const selected = draft.nodes.find(node => node.id === instanceId);
  const selectedContract = selected ? ports[selected.unit]?.routing : undefined;
  if (!selected || !selected.routing || !selectedContract) {
    throw new Error(`Routing helper "${instanceId}" was not found.`);
  }

  const sectionNodes = draft.nodes.filter(node => node.routing?.section === selected.routing!.section);
  const panner = sectionNodes.find(node => ports[node.unit]?.routing?.role === 'panner');
  const mixer = sectionNodes.find(node => ports[node.unit]?.routing?.role === 'mixer');
  if (!panner || !mixer || sectionNodes.length !== 2) {
    throw new Error(`Routing section "${selected.routing.section}" is incomplete.`);
  }
  const pannerPorts = ports[panner.unit];
  const mixerPorts = ports[mixer.unit];
  const incoming = draft.routes.find(route => route.to === `${panner.id}.${pannerPorts.inputs[0]}`);
  const outgoing = draft.routes.find(route => route.from === `${mixer.id}.${mixerPorts.outputs[0]}`);
  const directPaths = pannerPorts.routing!.paths.every((path, index) => (
    draft.routes.some(route => (
      route.from === `${panner.id}.${path.port}`
      && route.to === `${mixer.id}.${mixerPorts.routing!.paths[index].port}`
    ))
  ));
  if (!incoming || !outgoing || !directPaths) {
    throw new Error('Remove the effects inside both parallel paths before removing the split/join.');
  }

  const withoutPanner = removeProjectInstance(content, panner.id);
  const withoutHelpers = removeProjectInstance(withoutPanner, mixer.id);
  const next = addProjectRoute(withoutHelpers, ports, { from: incoming.from, to: outgoing.to });
  validateProjectRoutes(next, ports);
  return { content: next, mode: 'bridged', bridgedRoutes: 1 };
}

export function moveProjectInstanceOnRoute(
  content: string,
  ports: ProjectPortCatalog,
  instanceId: string,
  targetRouteIndex: number,
): string {
  const draft = parseProjectGraphDraft(content);
  const instance = draft.nodes.find(node => node.id === instanceId);
  if (!instance) throw new Error(`Project instance "${instanceId}" was not found.`);
  const contract = ports[instance.unit];
  if (instance.routing || contract?.routing) throw new Error('Routing helpers cannot move independently.');
  if (!isUserPlaceablePorts(contract) || contract.inputs.length !== 1 || contract.outputs.length !== 1) {
    throw new Error(`Effect "${instanceId}" must expose exactly one input and one output to move on a rail.`);
  }
  const targetRoute = draft.routes[targetRouteIndex];
  if (!targetRoute) throw new Error(`Project route ${targetRouteIndex} was not found.`);

  const inputEndpoint = `${instanceId}.${contract.inputs[0]}`;
  const outputEndpoint = `${instanceId}.${contract.outputs[0]}`;
  const incomingIndexes = draft.routes.flatMap((route, index) => route.to === inputEndpoint ? [index] : []);
  const outgoingIndexes = draft.routes.flatMap((route, index) => route.from === outputEndpoint ? [index] : []);
  const disconnected = incomingIndexes.length === 0 && outgoingIndexes.length === 0;
  const connected = incomingIndexes.length === 1 && outgoingIndexes.length === 1;
  if (!disconnected && !connected) {
    throw new Error(`Effect "${instanceId}" must be fully connected or fully disconnected before it can move.`);
  }
  if (connected && (targetRouteIndex === incomingIndexes[0] || targetRouteIndex === outgoingIndexes[0])) {
    return content;
  }

  const incidentIndexes = connected
    ? [incomingIndexes[0], outgoingIndexes[0]].sort((left, right) => left - right)
    : [];
  const incomingRoute = connected ? draft.routes[incomingIndexes[0]] : null;
  const outgoingRoute = connected ? draft.routes[outgoingIndexes[0]] : null;
  const nextRoutes: ProjectRouteDraft[] = [];
  draft.routes.forEach((route, index) => {
    if (index === targetRouteIndex) {
      nextRoutes.push(
        { from: targetRoute.from, to: inputEndpoint },
        { from: outputEndpoint, to: targetRoute.to },
      );
      return;
    }
    if (connected && index === incidentIndexes[0]) {
      nextRoutes.push({ from: incomingRoute!.from, to: outgoingRoute!.to });
      return;
    }
    if (connected && index === incidentIndexes[1]) return;
    nextRoutes.push({ ...route });
  });

  const doc = loadDocument(content);
  ensureChain(doc).routes = nextRoutes;
  const next = dumpDocument(doc);
  validateProjectRoutes(next, ports);
  return next;
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
    if (draft.nodes.find(node => node.id === instance)?.routing) {
      throw new Error(`Routing helper "${instance}" is always active and cannot be bypassed by a scene.`);
    }
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
  validateSceneSnapshot(draft, scene.params, scene.bypass);
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
  if (draft.routes.some((item, index) => index !== replacedIndex && item.from === route.from)) {
    throw new Error(`Route source "${route.from}" is already connected. Use Add in parallel to split a path.`);
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
  const nodesById = new Map(draft.nodes.map(node => [node.id, node]));
  const sections = new Map<string, { panner?: ProjectInstanceDraft; mixer?: ProjectInstanceDraft }>();
  const routeBySource = new Map<string, ProjectRouteDraft>();
  const routeByTarget = new Map<string, ProjectRouteDraft>();
  const incidentCount = new Map(draft.nodes.map(node => [node.id, 0]));

  for (const node of draft.nodes) {
    const contract = ports[node.unit];
    if (!contract) throw new Error(`Project unit contract "${node.unit}" was not found.`);
    const routing = contract.routing;
    if (routing) {
      if (!node.routing?.section) throw new Error(`Routing helper "${node.id}" must declare routing.section.`);
      if (routing.paths.length !== 2) throw new Error(`Routing helper "${node.id}" must expose exactly two paths.`);
      const pathPorts = routing.paths.map(path => path.port);
      if (routing.role === 'panner'
        && (contract.inputs.length !== 1 || contract.outputs.length !== 2
          || contract.outputs.some((port, index) => port !== pathPorts[index]))) {
        throw new Error(`Panner "${node.id}" must expose one input and its two ordered path outputs.`);
      }
      if (routing.role === 'mixer'
        && (contract.inputs.length !== 2 || contract.outputs.length !== 1
          || contract.inputs.some((port, index) => port !== pathPorts[index]))) {
        throw new Error(`Mixer "${node.id}" must expose its two ordered path inputs and one output.`);
      }
      const section = sections.get(node.routing.section) ?? {};
      if (section[routing.role]) {
        throw new Error(`Routing section "${node.routing.section}" has more than one ${routing.role}.`);
      }
      section[routing.role] = node;
      sections.set(node.routing.section, section);
    } else {
      if (node.routing) throw new Error(`Effect "${node.id}" cannot declare routing.section.`);
      if (!isUserPlaceablePorts(contract)) {
        throw new Error(`Effect "${node.id}" must expose exactly one mono input and one mono output.`);
      }
    }
  }

  for (const [sectionId, section] of sections) {
    if (!section.panner || !section.mixer) {
      throw new Error(`Routing section "${sectionId}" must contain one panner and one mixer.`);
    }
    const panner = ports[section.panner.unit].routing!;
    const mixer = ports[section.mixer.unit].routing!;
    if (panner.paths.some((path, index) => (
      path.port !== mixer.paths[index]?.port || path.levelParam !== mixer.paths[index]?.levelParam
    ))) {
      throw new Error(`Routing section "${sectionId}" panner and mixer path contracts do not match.`);
    }
  }

  for (const scene of draft.scenes) validateSceneSnapshot(draft, scene.params, scene.bypass);

  for (const route of draft.routes) {
    const source = parseEndpoint(route.from);
    const target = parseEndpoint(route.to);
    if (source.instance === 'system') {
      if (source.port !== 'input') throw new Error('Only system.input can be a route source.');
    } else {
      const node = nodesById.get(source.instance);
      if (!node) throw new Error(`Route source instance "${source.instance}" was not found.`);
      if (!ports[node.unit].outputs.includes(source.port)) throw new Error(`"${route.from}" is not a unit output.`);
      incidentCount.set(node.id, (incidentCount.get(node.id) ?? 0) + 1);
    }
    if (target.instance === 'system') {
      if (target.port !== 'output') throw new Error('Only system.output can be a route target.');
    } else {
      const node = nodesById.get(target.instance);
      if (!node) throw new Error(`Route target instance "${target.instance}" was not found.`);
      if (!ports[node.unit].inputs.includes(target.port)) throw new Error(`"${route.to}" is not a unit input.`);
      incidentCount.set(node.id, (incidentCount.get(node.id) ?? 0) + 1);
    }
    if (routeBySource.has(route.from)) {
      throw new Error(`Route source "${route.from}" is already connected. Use Add in parallel to split a path.`);
    }
    if (routeByTarget.has(route.to)) throw new Error(`Route target "${route.to}" is already connected.`);
    routeBySource.set(route.from, route);
    routeByTarget.set(route.to, route);
  }

  if (!routeBySource.has('system.input')) throw new Error('system.input must have exactly one route.');
  if (!routeByTarget.has('system.output')) throw new Error('system.output must have exactly one route.');

  for (const node of draft.nodes) {
    const contract = ports[node.unit];
    const endpoints = [
      ...contract.inputs.map(port => `${node.id}.${port}`),
      ...contract.outputs.map(port => `${node.id}.${port}`),
    ];
    const connected = endpoints.filter(endpoint => routeBySource.has(endpoint) || routeByTarget.has(endpoint));
    if (contract.routing) {
      if (connected.length !== endpoints.length) {
        throw new Error(`Routing helper "${node.id}" must have every input and output connected.`);
      }
    } else if (connected.length !== 0 && connected.length !== 2) {
      throw new Error(`Effect "${node.id}" must have one connected input and one connected output.`);
    }
  }

  const visitedNodes = new Set<string>();
  const activeSources = new Set<string>();
  const activeSections = new Set<string>();

  const trace = (sourceEndpoint: string, expectedTarget: string): void => {
    if (activeSources.has(sourceEndpoint)) throw new Error('Project routes create a cycle.');
    const route = routeBySource.get(sourceEndpoint);
    if (!route) throw new Error(`Routing path from "${sourceEndpoint}" is incomplete.`);
    if (route.to === expectedTarget) return;
    const target = parseEndpoint(route.to);
    if (target.instance === 'system') {
      throw new Error(`Routing path from "${sourceEndpoint}" reached ${route.to} instead of ${expectedTarget}.`);
    }
    const node = nodesById.get(target.instance)!;
    const contract = ports[node.unit];
    activeSources.add(sourceEndpoint);
    visitedNodes.add(node.id);
    try {
      if (!contract.routing) {
        if (target.port !== contract.inputs[0]) throw new Error(`Routing path enters "${route.to}" through the wrong port.`);
        trace(`${node.id}.${contract.outputs[0]}`, expectedTarget);
        return;
      }
      if (contract.routing.role === 'mixer') {
        throw new Error(`Routing path crossed into mixer "${node.id}" before its matching path endpoint.`);
      }
      if (target.port !== contract.inputs[0]) throw new Error(`Routing path enters panner "${node.id}" through the wrong port.`);
      const sectionId = node.routing!.section;
      if (activeSections.has(sectionId)) throw new Error(`Routing section "${sectionId}" creates a cycle.`);
      const section = sections.get(sectionId)!;
      const mixer = section.mixer!;
      const mixerContract = ports[mixer.unit];
      activeSections.add(sectionId);
      visitedNodes.add(mixer.id);
      try {
        contract.routing.paths.forEach((path, index) => {
          trace(`${node.id}.${path.port}`, `${mixer.id}.${mixerContract.routing!.paths[index].port}`);
        });
      } finally {
        activeSections.delete(sectionId);
      }
      trace(`${mixer.id}.${mixerContract.outputs[0]}`, expectedTarget);
    } finally {
      activeSources.delete(sourceEndpoint);
    }
  };

  trace('system.input', 'system.output');
  for (const node of draft.nodes) {
    if ((incidentCount.get(node.id) ?? 0) > 0 && !visitedNodes.has(node.id)) {
      throw new Error(`Connected effect "${node.id}" is orphaned from the system input/output path.`);
    }
  }
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
  const next = dumpDocument(doc);
  if (draft.nodes.some(node => node.routing)) validateProjectRoutes(next, ports);
  return next;
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
  const next = dumpDocument(doc);
  if (draft.nodes.some(node => node.routing)) validateProjectRoutes(next, ports);
  return next;
}

export function removeProjectRoute(content: string, index: number, ports?: ProjectPortCatalog): string {
  const doc = loadDocument(content);
  const draft = parseProjectGraphDraft(content);
  const chain = ensureChain(doc);
  const routes = (chain.routes as unknown[]).filter(isObject);
  if (!routes[index]) throw new Error(`Project route ${index} was not found.`);
  chain.routes = routes.filter((_, routeIndex) => routeIndex !== index);
  const next = dumpDocument(doc);
  if (draft.nodes.some(node => node.routing)) {
    if (!ports) throw new Error('Routing projects require unit contracts before a route can be removed.');
    validateProjectRoutes(next, ports);
  }
  return next;
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
