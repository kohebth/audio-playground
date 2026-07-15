import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

type Annotation = { type?: string; description?: string };
type Result = { status?: string; duration?: number; error?: { message?: string } };
type Test = { projectName?: string; annotations?: Annotation[]; results?: Result[] };
type Spec = { title?: string; file?: string; tests?: Test[] };
type Suite = { title?: string; specs?: Spec[]; suites?: Suite[] };
type Report = { suites?: Suite[]; stats?: Record<string, unknown> };

function option(name: string): string {
  const index = process.argv.indexOf(name);
  const value = index >= 0 ? process.argv[index + 1] : undefined;
  if (!value) throw new Error(`Missing ${name} argument.`);
  return value;
}

function collect(suites: Suite[], parents: string[] = []): Array<Record<string, unknown>> {
  const tests: Array<Record<string, unknown>> = [];
  for (const suite of suites) {
    const titles = suite.title ? [...parents, suite.title] : parents;
    for (const spec of suite.specs ?? []) {
      for (const test of spec.tests ?? []) {
        const result = test.results?.at(-1);
        tests.push({
          title: [...titles, spec.title].filter(Boolean).join(' > '),
          file: spec.file ?? '',
          project: test.projectName ?? '',
          status: result?.status ?? 'unknown',
          durationMs: result?.duration ?? 0,
          annotations: test.annotations ?? [],
          error: result?.error?.message ?? '',
        });
      }
    }
    tests.push(...collect(suite.suites ?? [], titles));
  }
  return tests;
}

const input = path.resolve(option('--input'));
const output = path.resolve(option('--output'));
const report = JSON.parse(readFileSync(input, 'utf8')) as Report;
const tests = collect(report.suites ?? []);
const measurements = tests.flatMap(test => {
  const title = String(test.title ?? '');
  return (test.annotations as Annotation[] ?? []).flatMap(annotation => {
    if (!annotation.type?.endsWith('-ms') || !annotation.description) return [];
    const labelled = annotation.description.match(/(?:median=|average=|:)(\d+(?:\.\d+)?)/);
    const direct = annotation.description.match(/^(\d+(?:\.\d+)?)/);
    const valueMs = Number(labelled?.[1] ?? direct?.[1]);
    return Number.isFinite(valueMs) ? [{ name: annotation.type, title, valueMs }] : [];
  });
}).sort((left, right) => right.valueMs - left.valueMs);
const trend = {
  schema: 'apg.unit-editor.browser-perf.v1',
  generatedAt: new Date().toISOString(),
  profile: process.env.APG_PERF_PROFILE ?? 'local',
  browser: process.env.APG_PERF_BROWSER ?? 'chromium',
  cpuThrottle: Number(process.env.APG_CPU_THROTTLE ?? '1'),
  runner: {
    os: process.env.RUNNER_OS ?? process.platform,
    arch: process.env.RUNNER_ARCH ?? process.arch,
  },
  source: {
    commit: process.env.GITHUB_SHA ?? '',
    ref: process.env.GITHUB_REF ?? '',
    runId: process.env.GITHUB_RUN_ID ?? '',
    runAttempt: process.env.GITHUB_RUN_ATTEMPT ?? '',
  },
  summary: report.stats ?? {},
  rankedMeasurements: measurements.slice(0, 3),
  tests,
};

mkdirSync(path.dirname(output), { recursive: true });
writeFileSync(output, `${JSON.stringify(trend, null, 2)}\n`);
console.log(`Wrote ${tests.length} browser performance results to ${output}`);
