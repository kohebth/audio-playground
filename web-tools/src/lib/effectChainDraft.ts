import type {
  EffectChainEditorDraft,
  EffectChainItemDraft,
  EffectChainParallelDraft,
  EffectChainRailDraft,
} from './projectPackage.ts';
import {
  parseProjectGraphDraft,
  validateProjectRoutes,
  writeProjectTopology,
  type ProjectGraphDraft,
  type ProjectPortCatalog,
  type ProjectRouteDraft,
} from './projectV2Graph.ts';

export type EffectChainLocation = { railId: string; index: number };

export type EffectChainDiagnostic = {
  code: 'missing-panner' | 'missing-mixer' | 'path-count' | 'topology';
  sectionId?: string;
  message: string;
};

type BuildContext = {
  draft: ProjectGraphDraft;
  ports: ProjectPortCatalog;
  routeBySource: Map<string, ProjectRouteDraft>;
  nodesById: Map<string, ProjectGraphDraft['nodes'][number]>;
  sections: Map<string, {
    panner?: ProjectGraphDraft['nodes'][number];
    mixer?: ProjectGraphDraft['nodes'][number];
  }>;
  visited: Set<string>;
};

function endpointInstance(endpoint: string): string {
  return endpoint.split('.')[0] ?? '';
}

function routeRail(
  context: BuildContext,
  id: string,
  source: string,
  target: string,
  activeSections: Set<string>,
): EffectChainRailDraft {
  const items: EffectChainItemDraft[] = [];
  let cursor = source;
  let guard = 0;
  while (cursor !== target) {
    if (guard++ > context.draft.nodes.length + context.draft.routes.length + 2) {
      throw new Error(`Effect Chain rail "${id}" contains a cycle.`);
    }
    const route = context.routeBySource.get(cursor);
    if (!route) throw new Error(`Effect Chain rail "${id}" is incomplete after ${cursor}.`);
    if (route.to === target) break;
    const instanceId = endpointInstance(route.to);
    if (instanceId === 'system') {
      throw new Error(`Effect Chain rail "${id}" reaches ${route.to} instead of ${target}.`);
    }
    const node = context.nodesById.get(instanceId);
    if (!node) throw new Error(`Effect Chain route references missing instance "${instanceId}".`);
    const contract = context.ports[node.unit];
    if (!contract) throw new Error(`Unit contract "${node.unit}" is unavailable.`);
    context.visited.add(instanceId);

    if (!contract.routing) {
      if (contract.inputs.length !== 1 || contract.outputs.length !== 1) {
        throw new Error(`Effect "${instanceId}" must expose exactly one input and one output.`);
      }
      if (route.to !== `${instanceId}.${contract.inputs[0]}`) {
        throw new Error(`Effect Chain enters "${instanceId}" through the wrong input.`);
      }
      items.push({ kind: 'effect', instanceId });
      cursor = `${instanceId}.${contract.outputs[0]}`;
      continue;
    }

    if (contract.routing.role !== 'panner' || !node.routing) {
      throw new Error(`Effect Chain reaches mixer "${instanceId}" before its paired panner.`);
    }
    if (activeSections.has(node.routing.section)) {
      throw new Error(`Parallel section "${node.routing.section}" contains a cycle.`);
    }
    const pair = context.sections.get(node.routing.section);
    if (!pair?.mixer) throw new Error(`Parallel section "${node.routing.section}" is missing its mixer.`);
    const mixerContract = context.ports[pair.mixer.unit];
    if (!mixerContract?.routing || mixerContract.routing.role !== 'mixer') {
      throw new Error(`Parallel section "${node.routing.section}" has an invalid mixer.`);
    }
    if (contract.routing.paths.length !== mixerContract.routing.paths.length) {
      throw new Error(`Parallel section "${node.routing.section}" has mismatched path counts.`);
    }
    activeSections.add(node.routing.section);
    context.visited.add(pair.mixer.id);
    const paths = contract.routing.paths.map((path, index) => ({
      id: `${node.routing!.section}:${path.port}`,
      port: path.port,
      levelParam: path.levelParam,
      rail: routeRail(
        context,
        `${node.routing!.section}:${path.port}`,
        `${node.id}.${path.port}`,
        `${pair.mixer!.id}.${mixerContract.routing!.paths[index].port}`,
        activeSections,
      ),
    }));
    activeSections.delete(node.routing.section);
    items.push({
      kind: 'parallel',
      id: `section:${node.routing.section}`,
      section: node.routing.section,
      pannerInstanceId: node.id,
      mixerInstanceId: pair.mixer.id,
      storedPannerInstanceId: node.id,
      storedMixerInstanceId: pair.mixer.id,
      paths,
    });
    cursor = `${pair.mixer.id}.${mixerContract.outputs[0]}`;
  }
  return { id, items };
}

export function createEffectChainDraft(content: string, ports: ProjectPortCatalog): EffectChainEditorDraft {
  validateProjectRoutes(content, ports);
  const draft = parseProjectGraphDraft(content);
  const sections: BuildContext['sections'] = new Map();
  for (const node of draft.nodes) {
    if (!node.routing) continue;
    const role = ports[node.unit]?.routing?.role;
    if (!role) continue;
    const pair = sections.get(node.routing.section) ?? {};
    pair[role] = node;
    sections.set(node.routing.section, pair);
  }
  const context: BuildContext = {
    draft,
    ports,
    routeBySource: new Map(draft.routes.map(route => [route.from, route])),
    nodesById: new Map(draft.nodes.map(node => [node.id, node])),
    sections,
    visited: new Set(),
  };
  const root = routeRail(context, 'root', 'system.input', 'system.output', new Set());
  const connected = new Set(draft.routes.flatMap(route => [endpointInstance(route.from), endpointInstance(route.to)]));
  for (const node of draft.nodes) {
    if (connected.has(node.id) && !context.visited.has(node.id)) {
      throw new Error(`Connected instance "${node.id}" is outside the Effect Chain rail.`);
    }
  }
  return { version: 1, root };
}

function visitRail(
  rail: EffectChainRailDraft,
  visit: (rail: EffectChainRailDraft, item: EffectChainItemDraft, index: number) => void,
): void {
  rail.items.forEach((item, index) => {
    visit(rail, item, index);
    if (item.kind === 'parallel') item.paths.forEach(path => visitRail(path.rail, visit));
  });
}

export function effectChainDiagnostics(draft: EffectChainEditorDraft | null): EffectChainDiagnostic[] {
  if (!draft) return [{ code: 'topology', message: 'Effect Chain layout is unavailable.' }];
  const diagnostics: EffectChainDiagnostic[] = [];
  visitRail(draft.root, (_rail, item) => {
    if (item.kind !== 'parallel') return;
    if (!item.pannerInstanceId) diagnostics.push({
      code: 'missing-panner', sectionId: item.id, message: `Parallel section "${item.section}" needs a panner.`,
    });
    if (!item.mixerInstanceId) diagnostics.push({
      code: 'missing-mixer', sectionId: item.id, message: `Parallel section "${item.section}" needs a mixer.`,
    });
    if (item.paths.length < 2) diagnostics.push({
      code: 'path-count', sectionId: item.id, message: `Parallel section "${item.section}" needs at least two paths.`,
    });
  });
  return diagnostics;
}

function cloneDraft(draft: EffectChainEditorDraft): EffectChainEditorDraft {
  return structuredClone(draft);
}

function findRail(root: EffectChainRailDraft, railId: string): EffectChainRailDraft | null {
  if (root.id === railId) return root;
  for (const item of root.items) {
    if (item.kind !== 'parallel') continue;
    for (const path of item.paths) {
      const found = findRail(path.rail, railId);
      if (found) return found;
    }
  }
  return null;
}

export function findEffectLocation(
  draft: EffectChainEditorDraft,
  instanceId: string,
  offset = 0,
): EffectChainLocation | null {
  let found: EffectChainLocation | null = null;
  visitRail(draft.root, (rail, item, index) => {
    if (!found && item.kind === 'effect' && item.instanceId === instanceId) {
      found = { railId: rail.id, index: index + offset };
    }
  });
  return found;
}

function itemInputEndpoint(
  item: EffectChainItemDraft,
  nodesById: Map<string, ProjectGraphDraft['nodes'][number]>,
  ports: ProjectPortCatalog,
): string {
  const instanceId = item.kind === 'effect' ? item.instanceId : item.pannerInstanceId;
  if (!instanceId) throw new Error(`Parallel section "${item.kind === 'parallel' ? item.section : item.instanceId}" is incomplete.`);
  const node = nodesById.get(instanceId);
  const port = node ? ports[node.unit]?.inputs[0] : null;
  if (!node || !port) throw new Error(`Effect Chain input endpoint for "${instanceId}" is unavailable.`);
  return `${instanceId}.${port}`;
}

function itemOutputEndpoint(
  item: EffectChainItemDraft,
  nodesById: Map<string, ProjectGraphDraft['nodes'][number]>,
  ports: ProjectPortCatalog,
): string {
  const instanceId = item.kind === 'effect' ? item.instanceId : item.mixerInstanceId;
  if (!instanceId) throw new Error(`Parallel section "${item.kind === 'parallel' ? item.section : item.instanceId}" is incomplete.`);
  const node = nodesById.get(instanceId);
  const port = node ? ports[node.unit]?.outputs[0] : null;
  if (!node || !port) throw new Error(`Effect Chain output endpoint for "${instanceId}" is unavailable.`);
  return `${instanceId}.${port}`;
}

function routeAtLocation(
  rail: EffectChainRailDraft,
  source: string,
  target: string,
  location: EffectChainLocation,
  nodesById: Map<string, ProjectGraphDraft['nodes'][number]>,
  ports: ProjectPortCatalog,
): ProjectRouteDraft | null {
  if (rail.id === location.railId) {
    const index = Math.max(0, Math.min(rail.items.length, location.index));
    return {
      from: index === 0 ? source : itemOutputEndpoint(rail.items[index - 1], nodesById, ports),
      to: index === rail.items.length ? target : itemInputEndpoint(rail.items[index], nodesById, ports),
    };
  }
  for (const item of rail.items) {
    if (item.kind !== 'parallel' || !item.pannerInstanceId || !item.mixerInstanceId) continue;
    const panner = nodesById.get(item.pannerInstanceId);
    const mixer = nodesById.get(item.mixerInstanceId);
    const pannerPaths = panner ? ports[panner.unit]?.routing?.paths : null;
    const mixerPaths = mixer ? ports[mixer.unit]?.routing?.paths : null;
    for (let index = 0; index < item.paths.length; index += 1) {
      const found = routeAtLocation(
        item.paths[index].rail,
        `${item.pannerInstanceId}.${pannerPaths?.[index]?.port}`,
        `${item.mixerInstanceId}.${mixerPaths?.[index]?.port}`,
        location,
        nodesById,
        ports,
      );
      if (found) return found;
    }
  }
  return null;
}

export function findEffectChainRouteIndex(
  content: string,
  draft: EffectChainEditorDraft,
  ports: ProjectPortCatalog,
  location: EffectChainLocation,
): number {
  const project = parseProjectGraphDraft(content);
  const route = routeAtLocation(
    draft.root,
    'system.input',
    'system.output',
    location,
    new Map(project.nodes.map(node => [node.id, node])),
    ports,
  );
  if (!route) throw new Error(`Effect Chain rail "${location.railId}" was not found.`);
  const index = project.routes.findIndex(candidate => candidate.from === route.from && candidate.to === route.to);
  if (index < 0) throw new Error(`The Effect Chain slot ${route.from} → ${route.to} is not connected.`);
  return index;
}

function takeEffect(root: EffectChainRailDraft, instanceId: string): EffectChainEffectDraft | null {
  for (let index = 0; index < root.items.length; index += 1) {
    const item = root.items[index];
    if (item.kind === 'effect' && item.instanceId === instanceId) {
      return root.items.splice(index, 1)[0] as EffectChainEffectDraft;
    }
    if (item.kind === 'parallel') {
      for (const path of item.paths) {
        const found = takeEffect(path.rail, instanceId);
        if (found) return found;
      }
    }
  }
  return null;
}

type EffectChainEffectDraft = Extract<EffectChainItemDraft, { kind: 'effect' }>;

export function insertEffectDraft(
  draft: EffectChainEditorDraft,
  instanceId: string,
  location: EffectChainLocation,
): EffectChainEditorDraft {
  const next = cloneDraft(draft);
  const rail = findRail(next.root, location.railId);
  if (!rail) throw new Error(`Effect Chain rail "${location.railId}" was not found.`);
  const index = Math.max(0, Math.min(rail.items.length, location.index));
  rail.items.splice(index, 0, { kind: 'effect', instanceId });
  return next;
}

export function moveEffectDraft(
  draft: EffectChainEditorDraft,
  instanceId: string,
  location: EffectChainLocation,
): EffectChainEditorDraft {
  const next = cloneDraft(draft);
  const previous = findEffectLocation(next, instanceId);
  const item = takeEffect(next.root, instanceId);
  if (!item) throw new Error(`Effect "${instanceId}" was not found on a rail.`);
  const rail = findRail(next.root, location.railId);
  if (!rail) throw new Error(`Effect Chain rail "${location.railId}" was not found.`);
  const requestedIndex = previous?.railId === location.railId && previous.index < location.index
    ? location.index - 1
    : location.index;
  rail.items.splice(Math.max(0, Math.min(rail.items.length, requestedIndex)), 0, item);
  return next;
}

export function removeEffectDraft(
  draft: EffectChainEditorDraft,
  instanceId: string,
): EffectChainEditorDraft {
  const next = cloneDraft(draft);
  if (!takeEffect(next.root, instanceId)) throw new Error(`Effect "${instanceId}" was not found on a rail.`);
  return next;
}

export function setParallelEndpointDraft(
  draft: EffectChainEditorDraft,
  sectionId: string,
  role: 'panner' | 'mixer',
  connected: boolean,
): EffectChainEditorDraft {
  const next = cloneDraft(draft);
  let found = false;
  visitRail(next.root, (_rail, item) => {
    if (item.kind !== 'parallel' || item.id !== sectionId) return;
    found = true;
    if (role === 'panner') item.pannerInstanceId = connected ? item.storedPannerInstanceId : null;
    else item.mixerInstanceId = connected ? item.storedMixerInstanceId : null;
  });
  if (!found) throw new Error(`Parallel section "${sectionId}" was not found.`);
  return next;
}

function buildRailRoutes(
  rail: EffectChainRailDraft,
  source: string,
  target: string,
  nodesById: Map<string, ProjectGraphDraft['nodes'][number]>,
  ports: ProjectPortCatalog,
  routes: ProjectRouteDraft[],
): void {
  let cursor = source;
  for (const item of rail.items) {
    if (item.kind === 'effect') {
      const node = nodesById.get(item.instanceId);
      const contract = node ? ports[node.unit] : null;
      if (!node || !contract || contract.inputs.length !== 1 || contract.outputs.length !== 1 || contract.routing) {
        throw new Error(`Effect "${item.instanceId}" is unavailable or no longer one-input/one-output.`);
      }
      routes.push({ from: cursor, to: `${node.id}.${contract.inputs[0]}` });
      cursor = `${node.id}.${contract.outputs[0]}`;
      continue;
    }
    if (!item.pannerInstanceId || !item.mixerInstanceId) {
      throw new Error(`Parallel section "${item.section}" is incomplete.`);
    }
    const panner = nodesById.get(item.pannerInstanceId);
    const mixer = nodesById.get(item.mixerInstanceId);
    const pannerPorts = panner ? ports[panner.unit] : null;
    const mixerPorts = mixer ? ports[mixer.unit] : null;
    if (!panner || !mixer || pannerPorts?.routing?.role !== 'panner' || mixerPorts?.routing?.role !== 'mixer') {
      throw new Error(`Parallel section "${item.section}" has unavailable helpers.`);
    }
    if (item.paths.length !== pannerPorts.routing.paths.length
      || item.paths.length !== mixerPorts.routing.paths.length) {
      throw new Error(`Parallel section "${item.section}" path count does not match its helpers.`);
    }
    routes.push({ from: cursor, to: `${panner.id}.${pannerPorts.inputs[0]}` });
    item.paths.forEach((path, index) => buildRailRoutes(
      path.rail,
      `${panner.id}.${pannerPorts.routing!.paths[index].port}`,
      `${mixer.id}.${mixerPorts.routing!.paths[index].port}`,
      nodesById,
      ports,
      routes,
    ));
    cursor = `${mixer.id}.${mixerPorts.outputs[0]}`;
  }
  routes.push({ from: cursor, to: target });
}

export function serializeEffectChainDraft(
  content: string,
  draft: EffectChainEditorDraft,
  ports: ProjectPortCatalog,
  removedInstanceIds: readonly string[] = [],
): string {
  const project = parseProjectGraphDraft(content);
  const nodesById = new Map(project.nodes.map(node => [node.id, node]));
  const retainedEffects = new Set<string>();
  visitRail(draft.root, (_rail, item) => {
    if (item.kind === 'effect') retainedEffects.add(item.instanceId);
  });
  const automaticallyRemoved = project.nodes
    .filter(node => !ports[node.unit]?.routing && !retainedEffects.has(node.id))
    .map(node => node.id);
  const routes: ProjectRouteDraft[] = [];
  buildRailRoutes(draft.root, 'system.input', 'system.output', nodesById, ports, routes);
  const next = writeProjectTopology(content, routes, [...new Set([...removedInstanceIds, ...automaticallyRemoved])]);
  const nextPorts = Object.fromEntries(Object.entries(ports).filter(([unitId]) => (
    parseProjectGraphDraft(next).units.some(unit => unit.id === unitId)
  )));
  validateProjectRoutes(next, nextPorts);
  return next;
}

export function effectChainRailIds(draft: EffectChainEditorDraft): string[] {
  const ids: string[] = [];
  const collect = (rail: EffectChainRailDraft) => {
    ids.push(rail.id);
    rail.items.forEach(item => {
      if (item.kind === 'parallel') item.paths.forEach(path => collect(path.rail));
    });
  };
  collect(draft.root);
  return ids;
}

export function findParallelDraft(
  draft: EffectChainEditorDraft,
  sectionId: string,
): EffectChainParallelDraft | null {
  let found: EffectChainParallelDraft | null = null;
  visitRail(draft.root, (_rail, item) => {
    if (item.kind === 'parallel' && item.id === sectionId) found = item;
  });
  return found;
}
