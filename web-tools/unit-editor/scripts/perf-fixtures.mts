import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import yaml from 'js-yaml';

import { parseUnitPortNames } from '../src/lib/projectV2Graph.ts';

const repoRoot = resolve(process.cwd(), '..', '..');
const fixtureRoot = resolve(repoRoot, 'test/fixtures/projects-v2', 'perf');
const unitRoot = resolve(repoRoot, 'test/fixtures/units-v2');

const PERF_PORT_COUNT = 24;

const SOURCE_UNITS = [
  'simple_gain',
  'noise_gate',
  'overdrive',
  'tone_stack',
  'tremolo',
  'delay',
  'simple_clip',
  'schroeder_reverb',
];

const PROFILES = [
  { name: 'small', nodes: 5, atoms: 25, routes: 30 },
  { name: 'medium', nodes: 20, atoms: 100, routes: 140 },
  { name: 'large', nodes: 50, atoms: 500, routes: 700 },
  { name: 'extreme', nodes: 100, atoms: 1000, routes: 1500 },
] as const;

type Topology = 'linear' | 'branching' | 'highly_connected' | 'reuse' | 'payload' | 'invalid';

type Profile = (typeof PROFILES)[number];

type UnitTemplate = {
  id: string;
  file: string;
};

type Route = { from: string; to: string };

type ProjectFixture = {
  kind: 'apg.project';
  schema: 'apg.project.v2';
  name: string;
  version: string;
  units: { id: string; file: string }[];
  chain: {
    nodes: { id: string; unit: string; params?: Record<string, string> }[];
    routes: Route[];
  };
  scenes?: { name: string; params: Record<string, string> }[];
  targets: {
    default: string;
    export: string[];
  };
  meta?: {
    perf: {
      profile: string;
      topology: Topology;
      target: {
        units: number;
        atoms: number;
        routes: number;
      };
    };
  };
};

function readString(path: string): string {
  return readFileSync(path, 'utf8');
}

function ensureDir(path: string): void {
  mkdirSync(path, { recursive: true });
}

function writeYaml(path: string, value: unknown): void {
  writeFileSync(path, yaml.dump(value, {
    lineWidth: 120,
    noRefs: true,
    quotingType: '"',
    noCompatMode: true,
  }), 'utf8');
}

function createFanoutUnitFile(name: string, paramCount: number, inputCount = PERF_PORT_COUNT): string {
  const file = `${name}.unit.v2.yaml`;
  const path = resolve(unitRoot, file);

  const ports = {
    inputs: Array.from({ length: inputCount }, (_, index) => ({
      name: `in${index}`,
      kind: 'signal',
      type: 'signal',
      channels: 1,
    })),
    outputs: [{ name: 'out', kind: 'signal', type: 'signal', channels: 1 }],
  };

  const params: Record<string, { type: string; default: number; min?: number; max?: number }> = {};
  for (let index = 1; index <= paramCount; index += 1) {
    params[`p${String(index).padStart(3, '0')}`] = {
      type: 'float',
      default: 0.5,
      min: 0,
      max: 1,
    };
  }

  const unitDoc = {
    kind: 'apg.unit',
    schema: 'apg.unit.v2',
    name,
    version: '2.0.0',
    params,
    ports,
    graph: {
      nodes: [],
    },
    compatibility: {
      desktop_full: true,
      wasm_realtime: true,
      m7_static: true,
      offline_render: true,
    },
    meta: {
      title: `Perf unit ${name}`,
      description: 'Synthetic deterministic fixture unit for performance generation.',
      category: 'test',
    },
  };

  writeYaml(path, unitDoc);
  return file;
}

function buildUnitTemplates(): UnitTemplate[] {
  const templates: UnitTemplate[] = [];

  for (const unitBase of SOURCE_UNITS) {
    const file = `${unitBase}.unit.v2.yaml`;
    const content = readString(resolve(unitRoot, file));
    const ports = parseUnitPortNames(content);
    const input = ports.inputs.includes('input') ? 'input' : ports.inputs[0];
    const output = ports.outputs.includes('output') ? 'output' : ports.outputs[0];
    if (!input || !output) {
      continue;
    }
    templates.push({ id: `${unitBase}_unit`, file: `../units-v2/${file}` });
  }

  if (templates.length < 2) {
    throw new Error('Not enough valid source unit templates found.');
  }

  const fanWidePath = createFanoutUnitFile('perf_fanwide', 3);
  templates.push({ id: 'perf_fanwide', file: `../units-v2/${fanWidePath}` });

  const payloadPath = createFanoutUnitFile('perf_fanwide_payload', 64);
  templates.push({ id: 'perf_fanwide_payload', file: `../units-v2/${payloadPath}` });

  const fanWideManyPortsPath = createFanoutUnitFile('perf_fanwide_24in', 1, PERF_PORT_COUNT);
  templates.push({ id: 'perf_fanwide_24in', file: `../units-v2/${fanWideManyPortsPath}` });

  return templates;
}

function buildNodes(template: UnitTemplate, count: number): { id: string; unit: string }[] {
  return Array.from({ length: count }, (_, index) => ({
    id: `u${String(index + 1).padStart(3, '0')}`,
    unit: template.id,
  }));
}

function buildLinearRoutes(nodes: string[]): Route[] {
  if (nodes.length === 0) return [];
  const routes: Route[] = [{ from: 'system.input', to: `${nodes[0]}.in0` }];

  for (let index = 0; index < nodes.length - 1; index += 1) {
    routes.push({ from: `${nodes[index]}.out`, to: `${nodes[index + 1]}.in0` });
  }

  routes.push({ from: `${nodes[nodes.length - 1]}.out`, to: 'system.output' });
  return routes;
}

function buildDenseRoutes(nodes: string[], inputCount: number, targetRoutes: number): Route[] {
  return buildTopologyRoutes(
    nodes,
    inputCount,
    targetRoutes,
    (targetIndex, slot) => (slot - 1) % targetIndex,
  );
}

function buildReuseRoutes(nodes: string[], inputCount: number, targetRoutes: number): Route[] {
  return buildTopologyRoutes(
    nodes,
    inputCount,
    targetRoutes,
    () => 0,
  );
}

function buildBranchingRoutes(nodes: string[], inputCount: number, targetRoutes: number): Route[] {
  return buildTopologyRoutes(
    nodes,
    inputCount,
    targetRoutes,
    targetIndex => targetIndex - 1,
  );
}

function buildTopologyRoutes(
  nodes: string[],
  inputCount: number,
  targetRoutes: number,
  sourcePicker: (targetIndex: number, slot: number) => number,
): Route[] {
  const routes = buildLinearRoutes(nodes);
  const usedTargets = new Set<string>(routes.map(route => route.to));
  const nextPortByTarget = new Map<string, number>(nodes.map(node => [node, 1]));

  let slot = 1;
  const maxAttempts = Math.max(
    1,
    nodes.length * nodes.length * inputCount * 4,
  );

  let attempts = 0;
  while (routes.length < targetRoutes && attempts < maxAttempts) {
    let addedInRound = false;

    for (let targetIndex = 1; targetIndex < nodes.length && routes.length < targetRoutes; targetIndex += 1) {
      const targetNode = nodes[targetIndex];
      const port = nextPortByTarget.get(targetNode) ?? 1;
      if (port >= inputCount) {
        continue;
      }

      const sourceIndex = sourcePicker(targetIndex, slot);
      if (sourceIndex < 0 || sourceIndex >= targetIndex) {
        continue;
      }

      const candidate: Route = { from: `${nodes[sourceIndex]}.out`, to: `${targetNode}.in${port}` };
      if (!usedTargets.has(candidate.to)) {
        routes.push(candidate);
        usedTargets.add(candidate.to);
        nextPortByTarget.set(targetNode, port + 1);
        attempts = 0;
        addedInRound = true;
      } else {
        nextPortByTarget.set(targetNode, port + 1);
      }
    }

    if (!addedInRound) {
      attempts += 1;
    }
    slot += 1;
  }

  if (routes.length < targetRoutes) {
    throw new Error(`Unable to build routes for target ${targetRoutes}. Produced ${routes.length}.`);
  }

  return routes.slice(0, targetRoutes);
}

function buildInvalidRoutes(nodes: string[]): Route[] {
  const routes = buildLinearRoutes(nodes);
  routes.push({ from: `${nodes[0]}.out`, to: `${nodes[1]}.not_a_port` });
  routes.push({ from: 'missing.output', to: `${nodes[0]}.in0` });
  routes.push({ from: `${nodes[nodes.length - 1]}.out`, to: 'missing.input' });
  routes.push({ from: `${nodes[1]}.out`, to: `u001.in0` });
  return routes;
}

function buildProjectNodes(profile: Profile, template: UnitTemplate, topology: Topology): { nodes: string[]; nodesPayload: { id: string; unit: string; params?: Record<string, string> }[] } {
  const nodesPayload = buildNodes(template, profile.nodes);
  const ids = nodesPayload.map(item => item.id);

  if (topology === 'payload' && nodesPayload.length > 0) {
    nodesPayload[0].unit = 'perf_fanwide_payload';
  }

  return { nodes: ids, nodesPayload };
}

function buildRoutes(nodes: string[], topology: Topology, targetRoutes: number): Route[] {
  switch (topology) {
    case 'linear':
      return buildLinearRoutes(nodes).slice(0, targetRoutes);
    case 'branching':
      return buildBranchingRoutes(nodes, PERF_PORT_COUNT, targetRoutes);
    case 'highly_connected':
      return buildDenseRoutes(nodes, PERF_PORT_COUNT, targetRoutes);
    case 'reuse':
      return buildReuseRoutes(nodes, PERF_PORT_COUNT, targetRoutes);
    case 'payload':
      return buildDenseRoutes(nodes, PERF_PORT_COUNT, targetRoutes);
    case 'invalid':
      return buildInvalidRoutes(nodes);
    default:
      return buildLinearRoutes(nodes);
  }
}

function buildProject(profile: Profile, topology: Topology, templates: UnitTemplate[]): ProjectFixture {
  const template = templates.find(template => template.id === 'perf_fanwide_24in')
    ?? templates[0];

  const projectNodes = buildProjectNodes(profile, template, topology);
  const routes = buildRoutes(projectNodes.nodes, topology, topology === 'invalid' ? profile.routes : profile.routes);
  const routeCount = routes.length;

  const units = new Set(projectNodes.nodesPayload.map(node => node.unit));
  const unitRefs = Array.from(units).map(id => templates.find(template => template.id === id)).filter((item): item is UnitTemplate => Boolean(item));

  if (unitRefs.length === 0) {
    throw new Error(`No unit references resolved for ${profile.name} ${topology}.`);
  }

  const payload: ProjectFixture = {
    kind: 'apg.project',
    schema: 'apg.project.v2',
    name: `perf-${topology}-${profile.name}`,
    version: '2.0.0',
    units: unitRefs.map(unit => ({ id: unit.id, file: unit.file })),
    chain: {
      nodes: projectNodes.nodesPayload,
      routes,
    },
    targets: {
      default: 'desktop_full',
      export: ['wasm_realtime', 'offline_render'],
    },
    meta: {
      perf: {
        profile: profile.name,
        topology,
        target: {
          units: profile.nodes,
          atoms: profile.atoms,
          routes: routeCount,
        },
      },
    },
  };

  if (profile.nodes >= 64) {
    payload.scenes = [
      {
        name: 'Baseline',
        params: Object.fromEntries(
          projectNodes.nodesPayload.slice(0, 3).flatMap(node => [
            [`${node.id}.p001`, '0.25'],
            [`${node.id}.p002`, '0.75'],
          ]),
        ),
      },
    ];
  }

  return payload;
}

function writeOutput(profile: Profile, topology: Topology, payload: ProjectFixture): string {
  ensureDir(fixtureRoot);
  const file = resolve(fixtureRoot, `${profile.name}-${topology}.project.v2.yaml`);
  writeYaml(file, payload);
  return file;
}

function generate(): void {
  const templates = buildUnitTemplates();
  const outputs: Array<{ profile: string; topology: Topology; path: string; nodes: number; routes: number }> = [];
  const topologies: Topology[] = ['linear', 'branching', 'highly_connected', 'reuse'];

  for (const profile of PROFILES) {
    for (const topology of topologies) {
      const payload = buildProject(profile, topology, templates);
      const output = writeOutput(profile, topology, payload);
      outputs.push({
        profile: profile.name,
        topology,
        path: output,
        nodes: payload.chain.nodes.length,
        routes: payload.chain.routes.length,
      });
    }

    const payloadProject = buildProject(profile, 'payload', templates);
    const payloadPath = resolve(fixtureRoot, `${profile.name}-payload.project.v2.yaml`);
    writeYaml(payloadPath, payloadProject);
    outputs.push({
      profile: profile.name,
      topology: 'payload',
      path: payloadPath,
      nodes: payloadProject.chain.nodes.length,
      routes: payloadProject.chain.routes.length,
    });
  }

  const invalidProfile = PROFILES[0];
  const invalid = buildProject(invalidProfile, 'invalid', templates);
  const invalidPath = resolve(fixtureRoot, `${invalidProfile.name}-invalid.project.v2.yaml`);
  writeYaml(invalidPath, invalid);
  outputs.push({
    profile: `${invalidProfile.name}-invalid`,
    topology: 'invalid',
    path: invalidPath,
    nodes: invalid.chain.nodes.length,
    routes: invalid.chain.routes.length,
  });

  console.log('Generated deterministic performance fixtures:');
  for (const item of outputs) {
    console.log(`${item.profile}/${item.topology}\t${item.nodes} nodes\t${item.routes} routes\t${item.path}`);
  }
}

generate();
