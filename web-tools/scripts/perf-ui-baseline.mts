import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';

type Measurement = { name: string; title: string; valueMs: number };
type Runner = {
  os: string;
  arch: string;
  cpuModel?: string;
  cpuCount?: number;
  memoryMb?: number;
  nodeVersion?: string;
};
type Source = { commit?: string; ref?: string; runId?: string; runAttempt?: string };
type Trend = {
  schema: string;
  generatedAt: string;
  profile: string;
  browser: string;
  cpuThrottle: number;
  memoryLimitMb: number;
  viewport?: string;
  runner: Runner;
  source?: Source;
  measurements?: Measurement[];
};
type Sample = {
  generatedAt: string;
  viewport: string;
  runner: Runner;
  source: Source;
  measurements: Measurement[];
};
type MetricSummary = Measurement & {
  sampleCount: number;
  medianMs: number;
  minMs: number;
  maxMs: number;
  latestMs: number;
};
type ProfileBaseline = {
  profile: string;
  browser: string;
  cpuThrottle: number;
  memoryLimitMb: number;
  samples: Sample[];
  metrics: MetricSummary[];
};
type Baseline = {
  schema: 'apg.unit-editor.browser-baseline.v1';
  generatedAt: string;
  maxSamplesPerProfile: number;
  profiles: ProfileBaseline[];
};

type Options = {
  inputs: string[];
  previous?: string;
  output: string;
  chart?: string;
  maxSamples: number;
};

function argumentValue(argv: string[], index: number, name: string): string {
  const value = argv[index + 1];
  if (!value) throw new Error(`Missing value for ${name}.`);
  return value;
}

function parseOptions(argv: string[]): Options {
  const options: Options = { inputs: [], output: '', maxSamples: 12 };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === '--input') {
      options.inputs.push(argumentValue(argv, index, arg));
      index += 1;
    } else if (arg === '--previous') {
      options.previous = argumentValue(argv, index, arg);
      index += 1;
    } else if (arg === '--output') {
      options.output = argumentValue(argv, index, arg);
      index += 1;
    } else if (arg === '--chart') {
      options.chart = argumentValue(argv, index, arg);
      index += 1;
    } else if (arg === '--max-samples') {
      options.maxSamples = Number.parseInt(argumentValue(argv, index, arg), 10);
      index += 1;
    }
  }
  if (options.inputs.length === 0) throw new Error('At least one --input report is required.');
  if (!options.output) throw new Error('Missing --output argument.');
  if (!Number.isFinite(options.maxSamples) || options.maxSamples < 1) {
    throw new Error('--max-samples must be a positive integer.');
  }
  return options;
}

function readJson<T>(file: string): T {
  return JSON.parse(readFileSync(path.resolve(file), 'utf8')) as T;
}

function emptyBaseline(maxSamples: number): Baseline {
  return {
    schema: 'apg.unit-editor.browser-baseline.v1',
    generatedAt: new Date(0).toISOString(),
    maxSamplesPerProfile: maxSamples,
    profiles: [],
  };
}

function validateTrend(trend: Trend, file: string): void {
  if (trend.schema !== 'apg.unit-editor.browser-perf.v1') throw new Error(`${file}: unsupported trend schema.`);
  if (!trend.profile || !trend.browser || !trend.generatedAt) throw new Error(`${file}: incomplete profile metadata.`);
  if (!Array.isArray(trend.measurements)) throw new Error(`${file}: measurements are missing.`);
  if (trend.measurements.some(item => !item.name || !item.title || !Number.isFinite(item.valueMs))) {
    throw new Error(`${file}: invalid measurement.`);
  }
}

function median(values: number[]): number {
  const sorted = [...values].sort((left, right) => left - right);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 0 ? (sorted[middle - 1] + sorted[middle]) / 2 : sorted[middle];
}

function metricKey(metric: Pick<Measurement, 'name' | 'title'>): string {
  return `${metric.name}\u0000${metric.title}`;
}

function summarize(samples: Sample[]): MetricSummary[] {
  const byMetric = new Map<string, Measurement[]>();
  for (const sample of samples) {
    for (const measurement of sample.measurements) {
      const key = metricKey(measurement);
      byMetric.set(key, [...(byMetric.get(key) ?? []), measurement]);
    }
  }
  return [...byMetric.values()].map(values => {
    const durations = values.map(value => value.valueMs);
    const latest = values.at(-1)!;
    return {
      name: latest.name,
      title: latest.title,
      valueMs: median(durations),
      sampleCount: durations.length,
      medianMs: median(durations),
      minMs: Math.min(...durations),
      maxMs: Math.max(...durations),
      latestMs: latest.valueMs,
    };
  }).sort((left, right) => right.medianMs - left.medianMs);
}

function sampleIdentity(sample: Sample): string {
  const runId = sample.source.runId;
  return runId ? `${runId}:${sample.source.runAttempt ?? ''}` : sample.generatedAt;
}

function mergeTrend(baseline: Baseline, trend: Trend, maxSamples: number): void {
  let profile = baseline.profiles.find(item => item.profile === trend.profile);
  if (!profile) {
    profile = {
      profile: trend.profile,
      browser: trend.browser,
      cpuThrottle: trend.cpuThrottle,
      memoryLimitMb: trend.memoryLimitMb,
      samples: [],
      metrics: [],
    };
    baseline.profiles.push(profile);
  }
  if (profile.browser !== trend.browser
    || profile.cpuThrottle !== trend.cpuThrottle
    || profile.memoryLimitMb !== trend.memoryLimitMb) {
    throw new Error(`${trend.profile}: browser, CPU, or memory metadata changed; use a new profile name.`);
  }
  const sample: Sample = {
    generatedAt: trend.generatedAt,
    viewport: trend.viewport ?? 'unknown',
    runner: trend.runner,
    source: trend.source ?? {},
    measurements: trend.measurements ?? [],
  };
  const identity = sampleIdentity(sample);
  profile.samples = [...profile.samples.filter(item => sampleIdentity(item) !== identity), sample]
    .sort((left, right) => left.generatedAt.localeCompare(right.generatedAt))
    .slice(-maxSamples);
  profile.metrics = summarize(profile.samples);
}

function escapeHtml(value: string): string {
  return value.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
}

function chartPoints(values: number[], width: number, height: number): string {
  const max = Math.max(...values, 1);
  const min = Math.min(...values, 0);
  const range = Math.max(max - min, 1);
  return values.map((value, index) => {
    const x = values.length === 1 ? width / 2 : (index / (values.length - 1)) * width;
    const y = height - ((value - min) / range) * height;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(' ');
}

function renderChart(baseline: Baseline): string {
  const profiles = baseline.profiles.map(profile => {
    const metricCharts = profile.metrics.slice(0, 12).map(metric => {
      const values = profile.samples.flatMap(sample => {
        const match = sample.measurements.find(item => metricKey(item) === metricKey(metric));
        return match ? [match.valueMs] : [];
      });
      return `<article><header><strong>${escapeHtml(metric.name)}</strong><span>median ${metric.medianMs.toFixed(2)} ms · latest ${metric.latestMs.toFixed(2)} ms</span></header><svg viewBox="0 0 720 120" role="img" aria-label="${escapeHtml(metric.name)} trend"><line x1="0" y1="119" x2="720" y2="119"/><polyline points="${chartPoints(values, 720, 108)}"/></svg><small>${escapeHtml(metric.title)}</small></article>`;
    }).join('');
    const runner = profile.samples.at(-1)?.runner;
    return `<section><h2>${escapeHtml(profile.profile)}</h2><p>${escapeHtml(profile.browser)} · ${profile.cpuThrottle}x CPU · ${profile.memoryLimitMb || 'unrestricted'} MB heap · ${escapeHtml(runner?.cpuModel ?? 'unknown CPU')} · ${profile.samples.length} samples</p><div>${metricCharts || '<p>No timing measurements.</p>'}</div></section>`;
  }).join('');
  return `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>APG browser performance trends</title><style>body{margin:0;padding:32px;background:#0b1020;color:#e5e7eb;font:14px system-ui,sans-serif}main{max-width:1100px;margin:auto}h1,h2{letter-spacing:0}section{margin:28px 0;border-top:1px solid #26324a}section>div{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:12px}article{padding:14px;border:1px solid #26324a;border-radius:6px;background:#11182a}header{display:flex;justify-content:space-between;gap:12px}header span,small,p{color:#9ca3af}svg{width:100%;height:120px;margin:10px 0;background:#0b1020}line{stroke:#334155}polyline{fill:none;stroke:#38bdf8;stroke-width:3;vector-effect:non-scaling-stroke}</style></head><body><main><h1>APG browser performance trends</h1><p>Generated ${escapeHtml(baseline.generatedAt)}. Rolling median over at most ${baseline.maxSamplesPerProfile} samples per profile.</p>${profiles}</main></body></html>`;
}

const options = parseOptions(process.argv.slice(2));
const previousPath = options.previous ? path.resolve(options.previous) : undefined;
const baseline = previousPath && existsSync(previousPath)
  ? readJson<Baseline>(previousPath)
  : emptyBaseline(options.maxSamples);
if (baseline.schema !== 'apg.unit-editor.browser-baseline.v1') throw new Error('Unsupported baseline schema.');
baseline.maxSamplesPerProfile = options.maxSamples;
for (const input of options.inputs) {
  const trend = readJson<Trend>(input);
  validateTrend(trend, input);
  mergeTrend(baseline, trend, options.maxSamples);
}
baseline.generatedAt = new Date().toISOString();
baseline.profiles.sort((left, right) => left.profile.localeCompare(right.profile));
const output = path.resolve(options.output);
mkdirSync(path.dirname(output), { recursive: true });
writeFileSync(output, `${JSON.stringify(baseline, null, 2)}\n`);
if (options.chart) {
  const chart = path.resolve(options.chart);
  mkdirSync(path.dirname(chart), { recursive: true });
  writeFileSync(chart, renderChart(baseline));
}
console.log(`Updated ${baseline.profiles.length} browser baseline profiles in ${output}`);
