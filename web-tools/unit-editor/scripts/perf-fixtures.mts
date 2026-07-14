import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import yaml from 'js-yaml';

import { parseUnitPortNames } from '../src/lib/projectV2Graph.ts';

type UnitTemplate = {
  id: string;
  file: string;
  input: string;
  output: string;
};

type Profile = {
  name: string;
  nodes: number;
};

const repoRoot = resolve(process.cwd(), '..', '..');
const projectDir = resolve(repoRoot, 'test/fixtures/projects-v2', 'perf');
const unitRoot = resolve(repoRoot, 'test/fixtures/units-v2');

const PROFILES: Profile[] = [
  { name: 'small', nodes: 24 },
  { name: 'medium', nodes: 64 },
  { name: 'large', nodes: 160 },
  { name: 'extreme', nodes: 320 },
];

const FIXED_UNITS = [
  'simple_gain',
  'noise_gate',
  'overdrive',
  'tone_stack',
  'tremolo',
  'delay',
  'simple_clip',
  'schroeder_reverb',
];

function readString(path: string): string {
  return readFileSync(path, 'utf8');
}

function buildUnitTemplates(): UnitTemplate[] {
  const templates: UnitTemplate[] = [];
  for (const unitBase of FIXED_UNITS) {
    const file = `${unitBase}.unit.v2.yaml`;
    const path = resolve(unitRoot, file);
    const content = readString(path);
    const ports = parseUnitPortNames(content);
    const input = ports.inputs.includes('input') ? 'input' : ports.inputs[0];
    const output = ports.outputs.includes('output') ? 'output' : ports.outputs[0];
    if (!input || !output) continue;
    templates.push({
      id: `${unitBase}_unit`,
      file: `../units-v2/${file}`,
      input,
      output,
    });
  }

  if (templates.length < 2) {
    throw new Error('Not enough valid templates were found to generate deterministic perf fixtures.');
  }

  return templates;
}

function buildNodes(templates: UnitTemplate[], count: number) {
  return Array.from({ length: count }, (_, index) => {
    const template = templates[index % templates.length];
    const id = `unit_${String(index + 1).padStart(3, '0')}`;
    return {
      id,
      unit: template.id,
      template,
    };
  });
}

function buildRoutes(nodes: ReturnType<typeof buildNodes>) {
  if (nodes.length === 0) return [];
  const first = nodes[0];
  const last = nodes[nodes.length - 1];
  const routes = [{ from: 'system.input', to: `${first.id}.${first.template.input}` }];

  for (let index = 0; index < nodes.length - 1; index += 1) {
    routes.push({
      from: `${nodes[index].id}.${nodes[index].template.output}`,
      to: `${nodes[index + 1].id}.${nodes[index + 1].template.input}`,
    });
  }

  routes.push({ from: `${last.id}.${last.template.output}`, to: 'system.output' });
  return routes;
}

function buildProject(profile: Profile, templates: UnitTemplate[]) {
  const nodes = buildNodes(templates, profile.nodes);
  const routes = buildRoutes(nodes);
  const fixture = {
    kind: 'apg.project',
    schema: 'apg.project.v2',
    name: `perf-${profile.name}-chain`,
    version: '2.0.0',
    units: templates.map(template => ({
      id: template.id,
      file: template.file,
    })),
    chain: {
      nodes: nodes.map(({ id, unit }) => ({ id, unit })),
      routes,
    },
    targets: {
      default: 'desktop_full',
      export: ['wasm_realtime', 'offline_render'],
    },
  };

  return yaml.dump(fixture, {
    lineWidth: 120,
    noRefs: true,
    quotingType: '"',
    noCompatMode: true,
  });
}

function writeFixture(profile: Profile, content: string) {
  mkdirSync(projectDir, { recursive: true });
  const output = resolve(projectDir, `${profile.name}.project.v2.yaml`);
  writeFileSync(output, content, 'utf8');
  return output;
}

function generate() {
  const templates = buildUnitTemplates();
  const outputs = PROFILES.map(profile => {
    const content = buildProject(profile, templates);
    const path = writeFixture(profile, content);
    return { profile: profile.name, path, nodes: profile.nodes, routes: profile.nodes + 1 };
  });

  console.log('Generated deterministic performance fixtures:');
  for (const item of outputs) {
    console.log(`${item.profile}\t${item.nodes} nodes\t${item.routes} routes\t${item.path}`);
  }
}

generate();
