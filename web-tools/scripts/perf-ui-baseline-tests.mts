import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';

type TrendOptions = {
  generatedAt: string;
  runId: string;
  valueMs: number;
  cpuThrottle?: number;
};

const directory = mkdtempSync(path.join(tmpdir(), 'apg-perf-ui-baseline-'));
const script = path.resolve('scripts/perf-ui-baseline.mts');

function trend(options: TrendOptions) {
  return {
    schema: 'apg.unit-editor.browser-perf.v1',
    generatedAt: options.generatedAt,
    profile: 'chromium-typical-laptop',
    browser: 'chromium',
    cpuThrottle: options.cpuThrottle ?? 4,
    memoryLimitMb: 0,
    viewport: '1280x720',
    runner: {
      os: 'linux',
      arch: 'x64',
      cpuModel: 'test cpu',
      cpuCount: 4,
      memoryMb: 8192,
      nodeVersion: process.version,
    },
    source: { runId: options.runId, runAttempt: '1' },
    measurements: [{ name: 'graph-load-ms', title: 'medium graph', valueMs: options.valueMs }],
  };
}

function writeTrend(name: string, value: ReturnType<typeof trend>): string {
  const file = path.join(directory, `${name}.json`);
  writeFileSync(file, JSON.stringify(value));
  return file;
}

function run(args: string[]): void {
  execFileSync(process.execPath, [
    '--disable-warning=ExperimentalWarning',
    '--experimental-strip-types',
    script,
    ...args,
  ], { stdio: 'pipe' });
}

const first = writeTrend('first', trend({ generatedAt: '2026-07-13T00:00:00.000Z', runId: '1', valueMs: 20 }));
const second = writeTrend('second', trend({ generatedAt: '2026-07-14T00:00:00.000Z', runId: '2', valueMs: 40 }));
const baseline = path.join(directory, 'baseline.json');
const chart = path.join(directory, 'trend.html');
run(['--input', first, '--input', second, '--output', baseline, '--chart', chart, '--max-samples', '2']);

let parsed = JSON.parse(readFileSync(baseline, 'utf8'));
assert.equal(parsed.profiles.length, 1);
assert.equal(parsed.profiles[0].samples.length, 2);
assert.equal(parsed.profiles[0].metrics[0].medianMs, 30);
assert.match(readFileSync(chart, 'utf8'), /APG browser performance trends/);

const rerun = writeTrend('rerun', trend({ generatedAt: '2026-07-14T01:00:00.000Z', runId: '2', valueMs: 50 }));
const third = writeTrend('third', trend({ generatedAt: '2026-07-15T00:00:00.000Z', runId: '3', valueMs: 70 }));
run(['--input', rerun, '--input', third, '--previous', baseline, '--output', baseline, '--max-samples', '2']);

parsed = JSON.parse(readFileSync(baseline, 'utf8'));
assert.deepEqual(parsed.profiles[0].samples.map((sample: { source: { runId: string } }) => sample.source.runId), ['2', '3']);
assert.equal(parsed.profiles[0].metrics[0].medianMs, 60);

const incompatible = writeTrend('incompatible', trend({
  generatedAt: '2026-07-15T01:00:00.000Z',
  runId: '4',
  valueMs: 80,
  cpuThrottle: 6,
}));
assert.throws(
  () => run(['--input', incompatible, '--previous', baseline, '--output', baseline]),
  /browser, CPU, or memory metadata changed/,
);

console.log('browser performance baseline tests passed');
