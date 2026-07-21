import { readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { performance } from 'node:perf_hooks';
import { dirname, resolve } from 'node:path';

import type { AtomCatalog, AtomCatalogAtom } from '../src/lib/backendSamples.ts';
import {
  addProjectInstance,
  addProjectRoute,
  moveProjectInstance,
  moveProjectRoute,
  parseProjectGraphDraft,
  parseUnitPortNames,
  removeProjectInstance,
  removeProjectRoute,
  validateProjectRoutes,
} from '../src/lib/projectV2Graph.ts';
import {
  addAtomNodeToUnit,
  connectUnitNodes,
  disconnectUnitInput,
  moveUnitConnection,
  parseUnitGraphDraft,
  removeAtomNodeFromUnit,
  replaceAtomNodeInUnit,
  setAtomNodePosition,
} from '../src/lib/unitV2Graph.ts';

type ProjectPortCatalog = Record<string, { inputs: string[]; outputs: string[] }>;

type SizeBucket = 'small' | 'medium' | 'large' | 'extreme';

type ProjectBenchmarkResult = {
  profile: string;
  file: string;
  nodes: number;
  routes: number;
  bucket: SizeBucket;
  parseMs: number;
  unitPortResolutionMs: number;
  addRemoveMs: number;
  moveInstanceMs: number;
  routeMutationMs: number;
  totalMs: number;
  memoryGrowthMb: number;
  valid: boolean;
  invalidExpected: boolean;
  error?: string;
};

type UnitBenchmarkResult = {
  file: string;
  nodes: number;
  parseMs: number;
  addMs: number;
  replaceMs: number;
  moveMs: number;
  connectMs: number;
  removeMs: number;
  totalMs: number;
  memoryGrowthMb: number;
};

type PerfResult = {
  profile: 'perf-benchmark';
  generatedAt: string;
  command: string;
  projects: ProjectBenchmarkResult[];
  units: UnitBenchmarkResult[];
  check?: {
    passed: boolean;
    failures: string[];
    regressions: string[];
  };
};

type ThresholdBucket = {
  maxTotalMs: number;
  maxParseMs: number;
  maxAddRemoveMs: number;
  maxRouteMutationMs: number;
  maxUnitAddMs: number;
  maxUnitTotalMs: number;
};

type Thresholds = {
  buckets: Record<SizeBucket, ThresholdBucket>;
  maxRegressionPercent: number;
  minAbsoluteRegressionMs: number;
};

type Options = {
  requestedProfiles: string[];
  checkOnly: boolean;
  unitLimit: number;
  outputPath?: string;
  thresholdsPath?: string;
  baselinePath?: string;
};

const repoRoot = resolve(process.cwd(), '..');
const fixtureDir = resolve(repoRoot, 'test/fixtures/projects-v2/perf');
const unitRoot = resolve(repoRoot, 'test/fixtures/units-v2');
const catalogPath = resolve(repoRoot, 'test/golden/v2-inspect-atoms.json');
const thresholdsDefaultPath = resolve(process.cwd(), 'scripts', 'perf-thresholds.json');
const PERF_PROFILE_SUFFIXES = ['branching', 'highly_connected', 'linear', 'payload', 'reuse', 'invalid'] as const;
const PERF_PROFILE_BUCKETS = ['small', 'medium', 'large', 'extreme'] as const;

function isPerfFixture(profile: string): boolean {
  if (profile === 'small-invalid') return true;

  const [bucket, suffix] = profile.split('-', 2);
  if (!suffix) return false;
  return PERF_PROFILE_BUCKETS.includes(bucket as any) && PERF_PROFILE_SUFFIXES.includes(suffix as any);
}

function parseOptions(argv: string[]): Options {
  const options: Options = {
    requestedProfiles: [],
    checkOnly: false,
    unitLimit: 8,
  };

  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === '--check') {
      options.checkOnly = true;
    } else if (arg === '--out' || arg === '--output') {
      options.outputPath = argv[index + 1];
      index += 1;
    } else if (arg.startsWith('--out=') || arg.startsWith('--output=')) {
      options.outputPath = arg.includes('=') ? arg.split('=', 2)[1] : undefined;
    } else if (arg === '--baseline' || arg === '--compare') {
      options.baselinePath = argv[index + 1];
      index += 1;
    } else if (arg.startsWith('--baseline=') || arg.startsWith('--compare=')) {
      options.baselinePath = arg.split('=', 2)[1];
    } else if (arg === '--thresholds') {
      options.thresholdsPath = argv[index + 1];
      index += 1;
    } else if (arg.startsWith('--thresholds=')) {
      options.thresholdsPath = arg.split('=', 2)[1];
    } else if (arg.startsWith('--unit-limit=')) {
      const next = Number.parseInt(arg.slice('--unit-limit='.length), 10);
      if (Number.isFinite(next) && next > 0) options.unitLimit = next;
    } else if (arg === '--units') {
      const next = Number.parseInt(argv[index + 1], 10);
      if (Number.isFinite(next) && next > 0) options.unitLimit = next;
      index += 1;
    } else if (arg === '--help' || arg === '-h') {
      console.log(`Usage:
  node --disable-warning=ExperimentalWarning --experimental-strip-types scripts/perf-benchmark.mts [profiles...]

Options:
  --check                Fail command if any performance threshold/regression check fails.
  --out <path>           Write full JSON report to file.
  --baseline <path>      Compare with a previous perf report.
  --thresholds <path>    Load custom thresholds JSON.
  --unit-limit <n>       Number of unit fixtures to include (default: 8).
`);
      process.exit(0);
    } else if (!arg.startsWith('-')) {
      options.requestedProfiles.push(arg);
    }
  }

  return options;
}

function resolveProjectFile(name: string): string {
  return resolve(fixtureDir, name);
}

function listProfiles(filter: string[]): string[] {
  const files = readdirSync(fixtureDir).filter(name => name.endsWith('.project.v2.yaml'));
  const candidates = files.filter(name => {
    const profile = fileToProfile(name);
    return isPerfFixture(profile);
  });

  if (filter.length === 0) return candidates.sort();

  const wanted = new Set(filter);
  const matchesRequested = (profile: string): boolean => {
    const [bucket] = profile.split('-', 2);
    for (const request of wanted) {
      if (request === profile) return true;
      if (request.endsWith('-*') && request.slice(0, -2) === bucket) return true;
      if (request === bucket && profile.startsWith(`${bucket}-`)) return true;
    }
    return false;
  };

  return candidates.filter(name => {
    const profile = fileToProfile(name);
    return matchesRequested(profile);
  }).sort();
}

function fileToProfile(file: string): string {
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

function parseCatalog(): AtomCatalog {
  return JSON.parse(readFileSync(catalogPath, 'utf8')) as AtomCatalog;
}

function parseThresholds(path?: string): Thresholds {
  const source = readFileSync(path ?? thresholdsDefaultPath, 'utf8');
  return JSON.parse(source) as Thresholds;
}

function bucketForNodeCount(nodes: number): SizeBucket {
  if (nodes <= 5) return 'small';
  if (nodes <= 20) return 'medium';
  if (nodes <= 50) return 'large';
  return 'extreme';
}

function memoryMb(): number {
  if (typeof globalThis.gc === 'function') globalThis.gc();
  return Number((process.memoryUsage().heapUsed / 1024 / 1024).toFixed(3));
}

function resolveUnitPorts(
  draft: ReturnType<typeof parseProjectGraphDraft>,
  projectPath: string,
): ProjectPortCatalog {
  const ports: ProjectPortCatalog = {};
  for (const reference of draft.units) {
    const file = resolve(dirname(projectPath), reference.file);
    const content = readFileSync(file, 'utf8');
    const resolved = parseUnitPortNames(content);
    if (!resolved.inputs.length || !resolved.outputs.length) {
      throw new Error(`Unit "${reference.id}" has missing input/output ports.`);
    }
    ports[reference.id] = resolved;
  }
  return ports;
}

function benchmarkProjectMutations(content: string, draft: ReturnType<typeof parseProjectGraphDraft>, ports: ProjectPortCatalog) {
  const mutations = {
    addRemoveMs: 0,
    moveInstanceMs: 0,
    routeMutationMs: 0,
  };

  const working = { content };
  if (draft.units.length > 0) {
    const iterations = Math.max(6, Math.min(120, draft.nodes.length * 2));
    mutations.addRemoveMs = measure(() => {
      let workingContent = working.content;
      for (let index = 0; index < iterations; index += 1) {
        const instanceId = `bench_mutation_${index}`;
        const added = addProjectInstance(workingContent, draft.units[0].id, instanceId);
        workingContent = removeProjectInstance(added.content, added.id);
      }
    });
  }

  if (draft.nodes.length > 1) {
    const iterations = Math.max(5, Math.min(50, draft.nodes));
    mutations.moveInstanceMs = measure(() => {
      let workingContent = working.content;
      for (let index = 0; index < iterations; index += 1) {
        const nextIndex = Math.min(Math.max(0, index % draft.nodes), draft.nodes - 1);
        const source = draft.nodes[index % draft.nodes].id;
        const targetIndex = (nextIndex + 1) % draft.nodes;
        workingContent = moveProjectInstance(workingContent, source, targetIndex);
      }
    });
  }

  if (draft.routes.length > 0) {
    const iterations = Math.max(3, Math.min(40, draft.routes));
    mutations.routeMutationMs = measure(() => {
      let workingContent = working.content;
      const sample = draft.routes[0];
      for (let index = 0; index < iterations; index += 1) {
        const removeIndex = index % draft.routes;
        workingContent = removeProjectRoute(workingContent, removeIndex);
        workingContent = addProjectRoute(workingContent, ports, sample);
        if (draft.routes > 1) {
          const moveFrom = removeIndex;
          const moveTo = (removeIndex + 1) % Math.max(draft.routes, 1);
          workingContent = moveProjectRoute(workingContent, moveFrom, moveTo);
        }
      }
    });
  }

  return mutations;
}

function benchmarkProjectFixture(file: string): ProjectBenchmarkResult {
  const profile = fileToProfile(file);
  const path = resolveProjectFile(file);
  const invalidExpected = /-invalid$/.test(profile);
  const startHeap = memoryMb();

  try {
    const parseMs = measure(() => {
      parseProjectFromFile(path);
    });
    const parsed = parseProjectFromFile(path);
    const unitPortResolutionMs = measure(() => {
      resolveUnitPorts(parsed.draft, path);
    });
    const ports = resolveUnitPorts(parsed.draft, path);
    validateProjectRoutes(parsed.content, ports);
    const mutations = benchmarkProjectMutations(parsed.content, parsed.draft, ports);
    const totalMs = parseMs + unitPortResolutionMs + mutations.addRemoveMs + mutations.moveInstanceMs + mutations.routeMutationMs;

    return {
      profile,
      file: path,
      nodes: parsed.draft.nodes.length,
      routes: parsed.draft.routes.length,
      bucket: bucketForNodeCount(parsed.draft.nodes.length),
      parseMs: Number(parseMs.toFixed(3)),
      unitPortResolutionMs: Number(unitPortResolutionMs.toFixed(3)),
      addRemoveMs: Number(mutations.addRemoveMs.toFixed(3)),
      moveInstanceMs: Number(mutations.moveInstanceMs.toFixed(3)),
      routeMutationMs: Number(mutations.routeMutationMs.toFixed(3)),
      totalMs: Number(totalMs.toFixed(3)),
      memoryGrowthMb: Number((memoryMb() - startHeap).toFixed(3)),
      valid: true,
      invalidExpected,
    };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    if (!invalidExpected) {
      return {
        profile,
        file: path,
        nodes: 0,
        routes: 0,
        bucket: 'small',
        parseMs: 0,
        unitPortResolutionMs: 0,
        addRemoveMs: 0,
        moveInstanceMs: 0,
        routeMutationMs: 0,
        totalMs: 0,
        memoryGrowthMb: Number((memoryMb() - startHeap).toFixed(3)),
        valid: false,
        invalidExpected,
        error: message,
      };
    }

    return {
      profile,
      file: path,
      nodes: 0,
      routes: 0,
      bucket: 'small',
      parseMs: 0,
      unitPortResolutionMs: 0,
      addRemoveMs: 0,
      moveInstanceMs: 0,
      routeMutationMs: 0,
      totalMs: 0,
      memoryGrowthMb: Number((memoryMb() - startHeap).toFixed(3)),
      valid: false,
      invalidExpected,
      error: 'expected-validation-failure',
    };
  }
}

function pickSignalAtoms(catalog: AtomCatalog): [AtomCatalogAtom | null, AtomCatalogAtom | null, AtomCatalogAtom | null] {
  const candidates = catalog.atoms.filter(atom =>
    atom.inputs.some(field => field.type === 'signal') && atom.outputs.some(field => field.type === 'signal'),
  );
  return [candidates[0] ?? null, candidates[1] ?? candidates[0] ?? null, candidates[2] ?? candidates[0] ?? null];
}

function benchmarkUnitFixture(file: string, catalog: AtomCatalog): UnitBenchmarkResult {
  const filePath = resolve(unitRoot, file);
  const startHeap = memoryMb();
  const content = readFileSync(filePath, 'utf8');
  const parsed = parseUnitGraphDraft(content);
  const parseMs = Number(measure(() => {
    parseUnitGraphDraft(content);
  }).toFixed(3));

  const [sourceAtom, targetAtom, replacementAtom] = pickSignalAtoms(catalog);
  if (!sourceAtom || !targetAtom || !replacementAtom) {
    return {
      file,
      nodes: parsed.nodes.length,
      parseMs,
      addMs: 0,
      replaceMs: 0,
      moveMs: 0,
      connectMs: 0,
      removeMs: 0,
      totalMs: parseMs,
      memoryGrowthMb: 0,
    };
  }

  const sourceOut = sourceAtom.outputs.find(field => field.type === 'signal')?.name ?? '';
  const targetIn = targetAtom.inputs.find(field => field.type === 'signal')?.name ?? '';
  const addMs = measure(() => {
    const created = addAtomNodeToUnit(content, catalog, sourceAtom.name);
    removeAtomNodeFromUnit(created.content, created.id);
  });

  const replaceMs = measure(() => {
    const created = addAtomNodeToUnit(content, catalog, sourceAtom.name);
    const replaced = replaceAtomNodeInUnit(created.content, catalog, created.id, replacementAtom.name);
    removeAtomNodeFromUnit(replaced.content, replaced.id);
  });

  const moveMs = measure(() => {
    const created = addAtomNodeToUnit(content, catalog, sourceAtom.name, { x: 0, y: 0 });
    for (let index = 0; index < 40; index += 1) {
      const moving = setAtomNodePosition(created.content, created.id, { x: index, y: index * 0.75 });
      created.content = moving;
    }
  });

  let connectMs = 0;
  if (sourceOut && targetIn) {
    connectMs = measure(() => {
      const first = addAtomNodeToUnit(content, catalog, sourceAtom.name, { x: 10, y: 10 });
      const second = addAtomNodeToUnit(first.content, catalog, targetAtom.name, { x: 20, y: 20 });
      const third = addAtomNodeToUnit(second.content, catalog, targetAtom.name, { x: 30, y: 30 });
      const connected = connectUnitNodes(third.content, catalog, { nodeId: first.id, field: sourceOut }, { nodeId: second.id, field: targetIn });
      const moved = moveUnitConnection(connected, catalog, { nodeId: second.id, field: targetIn }, { nodeId: third.id, field: targetIn });
      const disconnected = disconnectUnitInput(moved, { nodeId: third.id, field: targetIn });
      removeAtomNodeFromUnit(disconnected, second.id);
      removeAtomNodeFromUnit(disconnected, first.id);
      removeAtomNodeFromUnit(disconnected, third.id);
    });
  }

  const removeMs = measure(() => {
    const addedA = addAtomNodeToUnit(content, catalog, sourceAtom.name, { x: 100, y: 100 });
    const addedB = addAtomNodeToUnit(addedA.content, catalog, sourceAtom.name, { x: 120, y: 120 });
    let working = addedB.content;
    working = removeAtomNodeFromUnit(working, addedB.id);
    removeAtomNodeFromUnit(working, addedA.id);
  });

  const totalMs = Number((parseMs + addMs + replaceMs + moveMs + connectMs + removeMs).toFixed(3));

  return {
    file,
    nodes: parsed.nodes.length,
    parseMs,
    addMs: Number(addMs.toFixed(3)),
    replaceMs: Number(replaceMs.toFixed(3)),
    moveMs: Number(moveMs.toFixed(3)),
    connectMs: Number(connectMs.toFixed(3)),
    removeMs: Number(removeMs.toFixed(3)),
    totalMs,
    memoryGrowthMb: Number((memoryMb() - startHeap).toFixed(3)),
  };
}

function benchmarkUnits(catalog: AtomCatalog, limit: number): UnitBenchmarkResult[] {
  const files = readdirSync(unitRoot)
    .filter(item => item.endsWith('.unit.v2.yaml'))
    .filter(item => !item.startsWith('perf_'))
    .sort()
    .slice(0, Math.max(0, limit));

  return files.map(file => benchmarkUnitFixture(file, catalog));
}

function assertThresholds(projects: ProjectBenchmarkResult[], units: UnitBenchmarkResult[], thresholds: Thresholds): {
  failures: string[];
  regressions: string[];
} {
  const failures: string[] = [];
  const regressions: string[] = [];

  for (const row of projects) {
    if (!row.valid) {
      if (!row.invalidExpected) failures.push(`project:${row.profile}: ${row.error ?? 'fixture failed'}`);
      continue;
    }
    if (row.invalidExpected) {
      failures.push(`project:${row.profile}: expected invalid fixture was accepted`);
      continue;
    }
    const bucket = thresholds.buckets[row.bucket];
    if (row.parseMs > bucket.maxParseMs) {
      failures.push(`project:${row.profile}: parseMs ${row.parseMs} > ${bucket.maxParseMs}`);
    }
    if (row.addRemoveMs > bucket.maxAddRemoveMs) {
      failures.push(`project:${row.profile}: add/remove ${row.addRemoveMs} > ${bucket.maxAddRemoveMs}`);
    }
    if (row.routeMutationMs > bucket.maxRouteMutationMs) {
      failures.push(`project:${row.profile}: routeMutation ${row.routeMutationMs} > ${bucket.maxRouteMutationMs}`);
    }
    if (row.totalMs > bucket.maxTotalMs) {
      failures.push(`project:${row.profile}: total ${row.totalMs} > ${bucket.maxTotalMs}`);
    }
  }

  const mediumBucket = thresholds.buckets.medium;
  for (const row of units) {
    if (row.addMs > mediumBucket.maxUnitAddMs) {
      failures.push(`unit:${row.file}: addMs ${row.addMs} > ${mediumBucket.maxUnitAddMs}`);
    }
    if (row.totalMs > mediumBucket.maxUnitTotalMs) {
      failures.push(`unit:${row.file}: totalMs ${row.totalMs} > ${mediumBucket.maxUnitTotalMs}`);
    }
  }

  return { failures, regressions };
}

function compareBaselinePerf(current: PerfResult, baselinePath: string, thresholds: Thresholds): string[] {
  const raw = readFileSync(baselinePath, 'utf8');
  const baseline = JSON.parse(raw) as PerfResult;
  const baselineByProfile = new Map<string, ProjectBenchmarkResult>();
  for (const row of baseline.projects) {
    baselineByProfile.set(row.profile, row);
  }

  const regressions: string[] = [];
  const compareMetric = (label: string, value: number, prior: number) => {
    if (prior <= 0) return;
    const delta = ((value - prior) / prior) * 100;
    const deltaMs = value - prior;
    if (delta > thresholds.maxRegressionPercent && deltaMs >= thresholds.minAbsoluteRegressionMs) {
      regressions.push(`${label}: +${delta.toFixed(1)}% (baseline ${prior}ms -> ${value}ms)`);
    }
  };
  for (const row of current.projects) {
    const prior = baselineByProfile.get(row.profile);
    if (!prior || !prior.valid || !row.valid) continue;
    compareMetric(`project:${row.profile}:totalMs`, row.totalMs, prior.totalMs);
  }

  const baselineUnitsByFile = new Map(baseline.units.map(row => [row.file, row]));
  const unitMetrics: Array<keyof Pick<UnitBenchmarkResult,
    'addMs' | 'replaceMs' | 'moveMs' | 'connectMs' | 'removeMs' | 'totalMs'>> = [
      'addMs',
      'replaceMs',
      'moveMs',
      'connectMs',
      'removeMs',
      'totalMs',
    ];
  for (const row of current.units) {
    const prior = baselineUnitsByFile.get(row.file);
    if (!prior) continue;
    for (const metric of unitMetrics) {
      compareMetric(`unit:${row.file}:${metric}`, row[metric], prior[metric]);
    }
  }

  return regressions;
}

function main() {
  const options = parseOptions(process.argv.slice(2));
  const files = listProfiles(options.requestedProfiles);

  if (files.length === 0) {
    console.error('No performance fixtures found. Run "npm run perf:fixtures" first.');
    process.exit(1);
  }

  const catalog = parseCatalog();
  const projects = files.map(file => benchmarkProjectFixture(file));
  const units = benchmarkUnits(catalog, options.unitLimit);
  const report: PerfResult = {
    profile: 'perf-benchmark',
    generatedAt: new Date().toISOString(),
    command: process.argv.join(' '),
    projects,
    units,
  };

  if (options.checkOnly || options.thresholdsPath || options.baselinePath) {
    const thresholds = parseThresholds(options.thresholdsPath);
    const check = assertThresholds(projects, units, thresholds);
    if (options.baselinePath) {
      check.regressions.push(...compareBaselinePerf(report, options.baselinePath, thresholds));
    }
    report.check = {
      passed: check.failures.length === 0 && check.regressions.length === 0,
      failures: check.failures,
      regressions: check.regressions,
    };
  }

  if (options.outputPath) {
    writeFileSync(options.outputPath, `${JSON.stringify(report, null, 2)}\n`);
  }

  console.log(JSON.stringify(report, null, 2));

  if (options.checkOnly && report.check && !report.check.passed) process.exit(1);
}

main();
