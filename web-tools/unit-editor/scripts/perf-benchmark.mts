import { readdirSync, readFileSync } from 'node:fs';
import { performance } from 'node:perf_hooks';
import { resolve } from 'node:path';

import { addProjectInstance, parseProjectGraphDraft, parseUnitPortNames, removeProjectInstance } from '../src/lib/projectV2Graph.ts';

type BenchmarkResult = {
  profile: string;
  file: string;
  nodes: number;
  routes: number;
  parseMs: number;
  unitPortResolutionMs: number;
  mutationMs: number;
  totalMs: number;
};

const repoRoot = resolve(process.cwd(), '..', '..');
const fixtureDir = resolve(repoRoot, 'test/fixtures/projects-v2/perf');
const unitRoot = resolve(repoRoot, 'test/fixtures/units-v2');

function resolveProjectFile(name: string): string {
  return resolve(fixtureDir, name);
}

function listProfiles(filter: string[]): string[] {
  const files = readdirSync(fixtureDir).filter(name => name.endsWith('.project.v2.yaml'));
  if (filter.length === 0) return files.sort();
  const wanted = new Set(filter);
  return files.filter(name => wanted.has(name.replace('.project.v2.yaml', ''))).sort();
}

function fileToProjectProfile(file: string): string {
  return file.replace('.project.v2.yaml', '');
}

function measure(action: () => unknown): number {
  const start = performance.now();
  action();
  return performance.now() - start;
}

function parseProjectFromFile(path: string) {
  const content = readFileSync(path, 'utf8');
  const draft = parseProjectGraphDraft(content);
  return { content, draft };
}

function resolveUnitPorts(draft: ReturnType<typeof parseProjectGraphDraft>) {
  for (const reference of draft.units) {
    const file = resolve(unitRoot, reference.file.replace('../units-v2/', ''));
    const content = readFileSync(file, 'utf8');
    const ports = parseUnitPortNames(content);
    const input = ports.inputs.includes('input') ? 'input' : ports.inputs[0];
    const output = ports.outputs.includes('output') ? 'output' : ports.outputs[0];
    if (!input || !output) throw new Error(`Unit "${reference.id}" has unsupported port shape.`);
  }
}

function benchmarkMutation(base: string, draft: ReturnType<typeof parseProjectGraphDraft>): number {
  if (!draft.units.length) return 0;
  const unitId = draft.units[0].id;
  const start = performance.now();
  let working = base;
  const iterations = Math.max(40, Math.min(240, draft.nodes.length * 2));
  for (let index = 0; index < iterations; index += 1) {
    const instanceId = `bench_mutation_${index}`;
    const added = addProjectInstance(working, unitId, instanceId);
    working = removeProjectInstance(added.content, added.id);
  }
  return performance.now() - start;
}

function benchmarkFixture(file: string): BenchmarkResult {
  const profile = fileToProjectProfile(file);
  const path = resolveProjectFile(file);

  const parseMs = measure(() => {
    parseProjectFromFile(path);
  });
  const { content, draft } = parseProjectFromFile(path);

  const unitPortResolutionMs = measure(() => {
    resolveUnitPorts(draft);
  });

  const mutationMs = measure(() => {
    benchmarkMutation(content, draft);
  });

  const totalMs = parseMs + unitPortResolutionMs + mutationMs;

  return {
    profile,
    file: path,
    nodes: draft.nodes.length,
    routes: draft.routes.length,
    parseMs: Number(parseMs.toFixed(3)),
    unitPortResolutionMs: Number(unitPortResolutionMs.toFixed(3)),
    mutationMs: Number(mutationMs.toFixed(3)),
    totalMs: Number(totalMs.toFixed(3)),
  };
}

function main() {
  const args = process.argv.slice(2).filter(arg => !arg.startsWith('-')).filter(Boolean);
  const files = listProfiles(args);

  if (files.length === 0) {
    console.error('No performance fixtures found. Run "npm run perf:fixtures" first.');
    process.exit(1);
  }

  const results = files.map(file => benchmarkFixture(file));
  console.log(JSON.stringify({ profile: 'perf-benchmark', generatedAt: new Date().toISOString(), results }, null, 2));
}

main();
