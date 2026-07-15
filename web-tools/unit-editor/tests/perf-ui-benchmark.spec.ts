import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { expect, type CDPSession, type Page, test } from '@playwright/test';
import { dump as serializeYaml, load as parseYaml } from 'js-yaml';

type PerfSample = {
  name: string;
  durationMs: number;
  startedAt: number;
  endedAt: number;
};

type AutosaveWrite = {
  at: number;
  bytes: number;
  signature: string;
};

type RuntimeSnapshot = {
  phase: string;
  activeRevision: number;
  preparedRevision: number;
  meter: {
    frames: number;
    valid: boolean;
    underruns: number;
    callbackDeadlineMisses: number;
    maxCallbackMs: number;
  };
  resources: {
    workerActive: boolean;
    workletActive: boolean;
    pendingControlRequests: number;
    pendingProcessorRequests: number;
    contextState: string;
    workletStarts: number;
    workletStops: number;
    preparedImageBytes: number;
    streamTracks: number;
    inputNodeActive: boolean;
    fileSourceActive: boolean;
    meterTimerActive: boolean;
    latencyTimerActive: boolean;
    audioTracePollingActive: boolean;
  };
};

type PerfThresholdMap = {
  maxMs: Record<string, number>;
};

const thresholds: PerfThresholdMap = JSON.parse(
  readFileSync(path.resolve(process.cwd(), 'scripts', 'perf-ui-thresholds.json'), 'utf8'),
);

test.beforeEach(async ({ browserName, page }, testInfo) => {
  const requestedRate = Number(process.env.APG_CPU_THROTTLE ?? '1');
  const throttleRate = Number.isFinite(requestedRate) && requestedRate >= 1 ? requestedRate : 1;
  if (throttleRate > 1) {
    test.skip(browserName !== 'chromium', 'CPU throttling uses Chromium DevTools Protocol.');
    const session = await page.context().newCDPSession(page);
    await session.send('Emulation.setCPUThrottlingRate', { rate: throttleRate });
    await session.detach();
  }
  testInfo.annotations.push({
    type: 'performance-profile',
    description: JSON.stringify({
      browser: browserName,
      cpuThrottle: throttleRate,
      memoryLimitMb: Number(process.env.APG_MEMORY_LIMIT_MB ?? '0'),
      profile: process.env.APG_PERF_PROFILE ?? 'local',
      viewport: page.viewportSize() ? `${page.viewportSize()!.width}x${page.viewportSize()!.height}` : 'native',
    }),
  });
});

type PerfFixture = {
  chain?: {
    nodes?: Array<{ file?: string }>;
    routes?: unknown[];
  };
  meta?: {
    perf?: {
      profile?: string;
      topology?: string;
      target?: {
        units?: number;
        atoms?: number;
        routes?: number;
      };
    };
  };
};

type PerfFixturePayload = {
  schema: string;
  version: number;
  entryProject: string;
  files: {
    path: string;
    role: 'project' | 'unit';
    content: string;
  }[];
};

type PerfUnitFixture = {
  graph?: {
    signals?: string[];
    nodes?: Array<Record<string, unknown>>;
  };
};

function getThreshold(name: string): number | null {
  return thresholds.maxMs[name] ?? null;
}

function clearPerfSpans(page: Page) {
  return page.evaluate(() => {
    const trace = (window as {
      __apgPerfTrace?: {
        samples?: unknown[];
        renderSamples?: unknown[];
        componentRenders?: Record<string, number>;
        counters?: Record<string, number>;
      };
    }).__apgPerfTrace;
    if (!trace) return;
    trace.samples = [];
    trace.renderSamples = [];
    trace.componentRenders = {};
    trace.counters = {};
  });
}

async function waitForSpanCount(page: Page, name: string, minimum: number) {
  await expect
    .poll(async () => {
      const samples = await getSpans(page, name);
      return samples.length;
    }, {
      message: `waiting for ${minimum} span(s) named ${name}`,
      timeout: 5000,
    })
    .toBeGreaterThanOrEqual(minimum);
}

async function getSpans(page: Page, name: string): Promise<PerfSample[]> {
  return page.evaluate((targetName: string): PerfSample[] => {
    const trace = (window as { __apgPerfTrace?: { samples?: PerfSample[] } }).__apgPerfTrace;
    if (!trace?.samples) return [];
    return trace.samples.filter(sample => sample.name === targetName);
  }, name);
}

async function runAndAssertBudget(page: Page, name: string, minimumSamples = 1, thresholdName = name) {
  const budgetMs = getThreshold(thresholdName);
  expect(budgetMs).toBeTruthy();
  await waitForSpanCount(page, name, minimumSamples);
  const samples = await getSpans(page, name);
  const latest = samples.slice(-minimumSamples);
  const maxMs = Math.max(...latest.map(sample => sample.durationMs));
  expect(maxMs).toBeLessThanOrEqual(budgetMs ?? Number.POSITIVE_INFINITY);
  return maxMs;
}

function assertDurationBudget(name: string, durationMs: number) {
  const budgetMs = getThreshold(name);
  expect(budgetMs).toBeTruthy();
  expect(durationMs).toBeLessThanOrEqual(budgetMs ?? Number.POSITIVE_INFINITY);
}

function resolveRepoRoot(): string {
  return path.resolve(process.cwd(), '..', '..');
}

function normalizePosixPath(value: string): string {
  return value.split(path.sep).join('/');
}

function readPerfFixtureMeta(filePath: string): { nodes: number; routes: number; atoms: number } {
  const absolutePath = path.resolve(resolveRepoRoot(), filePath);
  const parsed = parseYaml(readFileSync(absolutePath, 'utf8')) as PerfFixture;
  return {
    nodes: Array.isArray(parsed.chain?.nodes) ? parsed.chain.nodes.length : 0,
    routes: Array.isArray(parsed.chain?.routes) ? parsed.chain.routes.length : 0,
    atoms: parsed.meta?.perf?.target?.atoms ?? 0,
  };
}

function resolveFixtureUnitPath(projectAbsolute: string, unitFile: string): string | null {
  const candidates = [
    path.resolve(path.dirname(projectAbsolute), unitFile),
    path.resolve(resolveRepoRoot(), 'test', 'fixtures', unitFile.replace(/^\.\.\//, '')),
    path.resolve(resolveRepoRoot(), unitFile.replace(/^\.\.\//, '')),
  ];

  for (const candidate of candidates) {
    if (existsSync(candidate)) {
      return candidate;
    }
  }

  return null;
}

function buildPerfWorkspacePayload(profilePath: string): string {
  const repoRoot = resolveRepoRoot();
  const projectAbsolute = path.resolve(repoRoot, profilePath);
  const projectContent = readFileSync(projectAbsolute, 'utf8');
  const parsed = parseYaml(projectContent) as PerfFixture & { units?: Array<{ file?: string }> };
  const unitFiles = new Map<string, string>();

  for (const unit of parsed.units ?? []) {
    const unitFile = unit?.file;
    if (!unitFile) continue;
    const absolute = resolveFixtureUnitPath(projectAbsolute, unitFile);
    if (!absolute) {
      throw new Error(`Unable to resolve unit file "${unitFile}" for "${profilePath}".`);
    }
    if (absolute.startsWith(repoRoot)) {
      unitFiles.set(absolute, readFileSync(absolute, 'utf8'));
    }
  }

  const files: PerfFixturePayload['files'] = [
    {
      path: normalizePosixPath(path.relative(repoRoot, projectAbsolute)),
      role: 'project',
      content: projectContent,
    },
  ];

  for (const [absolutePath, content] of unitFiles) {
    files.push({
      path: normalizePosixPath(path.relative(repoRoot, absolutePath)),
      role: 'unit',
      content,
    });
  }

  return JSON.stringify({
    schema: 'apg.ui.workspace.v2',
    version: 2,
    entryProject: normalizePosixPath(path.relative(repoRoot, projectAbsolute)),
    files,
  } as PerfFixturePayload);
}

function buildAtomRetentionPayloads(profilePath: string, addedAtoms: number): { baseline: string; stressed: string } {
  const payload = JSON.parse(buildPerfWorkspacePayload(profilePath)) as PerfFixturePayload;
  const baseline = JSON.stringify(payload);
  const unitFile = payload.files.find(file => file.role === 'unit');
  if (!unitFile) throw new Error(`Retention fixture "${profilePath}" has no unit file.`);
  const unit = parseYaml(unitFile.content) as PerfUnitFixture;
  if (!Array.isArray(unit.graph?.signals) || !Array.isArray(unit.graph.nodes)) {
    throw new Error(`Retention fixture "${profilePath}" has no mutable graph.`);
  }
  for (let index = 0; index < addedAtoms; index += 1) {
    const suffix = String(index + 1).padStart(3, '0');
    const signal = `retained_signal_${suffix}`;
    unit.graph.signals.push(signal);
    unit.graph.nodes.push({
      id: `retained_atom_${suffix}`,
      atom: 'amplitude_clip_hard',
      in: { signal: '' },
      out: { signal },
      config: { threshold: 1 },
      ui: { position: { x: (index % 10) * 260, y: 1000 + Math.floor(index / 10) * 180 } },
    });
  }
  unitFile.content = serializeYaml(unit, { lineWidth: 120, noRefs: true });
  return { baseline, stressed: JSON.stringify(payload) };
}

async function importWorkspacePayload(page: Page, payload: string, expectedNodes: number): Promise<void> {
  await page.getByTestId('topbar-import-input').setInputFiles({
    name: 'perf-workspace.json',
    mimeType: 'application/json',
    buffer: Buffer.from(payload),
  });
  await expect.poll(() => countProjectNodes(page), { timeout: 20_000 }).toBe(expectedNodes);
}

async function importPerfWorkspaceFixture(page: Page, profilePath: string, expectedNodes: number): Promise<number> {
  const payload = buildPerfWorkspacePayload(profilePath);
  const startedAt = await page.evaluate(() => performance.now());
  await importWorkspacePayload(page, payload, expectedNodes);
  return page.evaluate(start => performance.now() - start, startedAt);
}

async function collectHeapBytes(page: Page): Promise<number> {
  const session = await page.context().newCDPSession(page);
  await session.send('HeapProfiler.collectGarbage');
  const usage = await session.send('Runtime.getHeapUsage');
  await session.detach();
  return usage.usedSize;
}

async function getComponentRenders(page: Page, component: string): Promise<Record<string, number>> {
  return page.evaluate(name => {
    const trace = (window as {
      __apgPerfTrace?: { componentRenders?: Record<string, number> };
    }).__apgPerfTrace;
    return Object.fromEntries(
      Object.entries(trace?.componentRenders ?? {}).filter(([key]) => key.startsWith(`${name}:`)),
    );
  }, component);
}

async function getPerfCounters(page: Page): Promise<Record<string, number>> {
  return page.evaluate(() => {
    const trace = (window as { __apgPerfTrace?: { counters?: Record<string, number> } }).__apgPerfTrace;
    return { ...(trace?.counters ?? {}) };
  });
}

async function getRuntimeSnapshot(page: Page): Promise<RuntimeSnapshot | null> {
  return page.evaluate(() => {
    const trace = (window as { __apgPerfTrace?: { runtime?: RuntimeSnapshot } }).__apgPerfTrace;
    return trace?.runtime ?? null;
  });
}

async function openContractFixture(page: Page, fixturePath: string, expectedAtoms: number): Promise<number> {
  await importPerfWorkspaceFixture(page, fixturePath, 1);
  return openLoadedContract(page, expectedAtoms);
}

async function openLoadedContract(page: Page, expectedAtoms: number): Promise<number> {
  const startedAt = await page.evaluate(() => performance.now());
  await page.getByTestId('project-node-atom_stress').dblclick();
  const canvas = page.getByTestId('contract-canvas');
  await expect(canvas).toBeVisible();
  await expect(canvas).toHaveAttribute('data-atom-count', String(expectedAtoms));
  await expect.poll(() => page.locator('.contract-node').count(), { timeout: 30_000 }).toBeGreaterThan(0);
  return page.evaluate(start => performance.now() - start, startedAt);
}

function median(values: number[]): number {
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.floor(sorted.length / 2)];
}

async function startInteractionProbe(page: Page): Promise<void> {
  await page.evaluate(() => {
    const probe = {
      active: true,
      frameIntervals: [] as number[],
      lastFrameAt: performance.now(),
      longTasks: [] as number[],
      observer: null as PerformanceObserver | null,
    };
    const frame = (now: number) => {
      if (!probe.active) return;
      probe.frameIntervals.push(now - probe.lastFrameAt);
      probe.lastFrameAt = now;
      requestAnimationFrame(frame);
    };
    if (typeof PerformanceObserver !== 'undefined') {
      probe.observer = new PerformanceObserver(list => {
        probe.longTasks.push(...list.getEntries().map(entry => entry.duration));
      });
      try {
        probe.observer.observe({ type: 'longtask', buffered: false });
      } catch {
        probe.observer = null;
      }
    }
    (window as typeof window & { __apgInteractionProbe?: typeof probe }).__apgInteractionProbe = probe;
    requestAnimationFrame(frame);
  });
}

async function stopInteractionProbe(page: Page): Promise<{ frameIntervals: number[]; longTasks: number[] }> {
  await page.waitForTimeout(250);
  return page.evaluate(() => {
    const host = window as typeof window & {
      __apgInteractionProbe?: {
        active: boolean;
        frameIntervals: number[];
        longTasks: number[];
        observer: PerformanceObserver | null;
      };
    };
    const probe = host.__apgInteractionProbe;
    if (!probe) return { frameIntervals: [], longTasks: [] };
    probe.active = false;
    probe.observer?.disconnect();
    return { frameIntervals: probe.frameIntervals, longTasks: probe.longTasks };
  });
}

type DevtoolsTraceEvent = { name?: string; cat?: string; ph?: string; dur?: number };

async function startDevtoolsTrace(page: Page): Promise<{ session: CDPSession; events: DevtoolsTraceEvent[] }> {
  const session = await page.context().newCDPSession(page);
  const events: DevtoolsTraceEvent[] = [];
  session.on('Tracing.dataCollected', payload => events.push(...(payload.value as DevtoolsTraceEvent[])));
  await session.send('Tracing.start', {
    categories: [
      'devtools.timeline',
      'blink.user_timing',
      'disabled-by-default-devtools.timeline',
      'disabled-by-default-devtools.screenshot',
    ].join(','),
    options: 'sampling-frequency=10000',
    transferMode: 'ReportEvents',
  });
  return { session, events };
}

async function stopDevtoolsTrace(trace: { session: CDPSession; events: DevtoolsTraceEvent[] }) {
  const complete = new Promise<void>(resolve => trace.session.once('Tracing.tracingComplete', () => resolve()));
  await trace.session.send('Tracing.end');
  await complete;
  await trace.session.detach();
  const duration = (names: Set<string>) => trace.events.reduce(
    (total, event) => total + (event.ph === 'X' && event.name && names.has(event.name) ? (event.dur ?? 0) / 1000 : 0),
    0,
  );
  return {
    events: trace.events,
    styleMs: duration(new Set(['RecalculateStyles', 'UpdateLayoutTree'])),
    layoutMs: duration(new Set(['Layout'])),
    paintMs: duration(new Set(['Paint', 'CompositeLayers'])),
    userTimingEvents: trace.events.filter(event => event.cat?.includes('blink.user_timing')).length,
  };
}

async function getVisibleContractAtom(page: Page): Promise<{ id: string; x: number; y: number } | null> {
  return page.evaluate(() => {
    const canvas = document.querySelector<HTMLElement>('[data-testid="contract-canvas"]');
    if (!canvas) return null;
    const canvasRect = canvas.getBoundingClientRect();
    for (const node of document.querySelectorAll<HTMLElement>('.react-flow__node[data-id^="contract-atom_"]')) {
      const rect = node.getBoundingClientRect();
      if (rect.left >= canvasRect.left + 40 && rect.right <= canvasRect.right - 40
        && rect.top >= canvasRect.top + 40 && rect.bottom <= canvasRect.bottom - 40) {
        return {
          id: (node.dataset.id ?? '').replace(/^contract-/, ''),
          x: rect.left + rect.width / 2,
          y: rect.top + rect.height / 2,
        };
      }
    }
    return null;
  });
}

async function dispatchContractDrag(
  page: Page,
  type: 'dragover' | 'drop',
  atomName: string | null,
): Promise<void> {
  await page.getByTestId('contract-canvas').evaluate((canvas, payload) => {
    const dataTransfer = new DataTransfer();
    if (payload.atomName) {
      dataTransfer.setData('application/x-apg-atom', payload.atomName);
    } else {
      dataTransfer.setData('text/plain', 'unsupported-payload');
    }
    const bounds = canvas.getBoundingClientRect();
    canvas.dispatchEvent(new DragEvent(payload.type, {
      bubbles: true,
      cancelable: true,
      clientX: bounds.left + bounds.width * 0.55,
      clientY: bounds.top + bounds.height * 0.55,
      dataTransfer,
    }));
  }, { atomName, type });
}

async function dispatchContractEdgeDrop(page: Page, edgeId: string, atomName: string): Promise<void> {
  await page.getByTestId(`rf__edge-${edgeId}`).evaluate((edge, atom) => {
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('application/x-apg-atom', atom);
    const bounds = edge.getBoundingClientRect();
    edge.dispatchEvent(new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      clientX: bounds.left + bounds.width / 2,
      clientY: bounds.top + bounds.height / 2,
      dataTransfer,
    }));
  }, atomName);
}

async function dispatchProjectEdgeDrop(page: Page, edgeId: string, unitId: string): Promise<void> {
  await page.getByTestId(`rf__edge-${edgeId}`).evaluate((edge, unit) => {
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('application/x-apg-unit', unit);
    const bounds = edge.getBoundingClientRect();
    edge.dispatchEvent(new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      clientX: bounds.left + bounds.width / 2,
      clientY: bounds.top + bounds.height / 2,
      dataTransfer,
    }));
  }, unitId);
}

async function launchWorkspace(page: Page) {
  await page.goto('/', { waitUntil: 'domcontentloaded' });
  const launch = page.getByTestId('launch-workspace');
  await expect(launch).toBeVisible({ timeout: 180_000 });
  await launch.click();

  await expect(page.getByTestId('project-canvas')).toBeVisible();
  await expect(page.getByTestId('project-instance-item-gate1')).toBeVisible();
}

async function countProjectNodes(page: Page) {
  return page.locator('.project-node:not(.project-node--system)[data-testid^="project-node-"]').count();
}

async function waitForWorkspaceQuiescence(page: Page) {
  await expect.poll(async () => (await getSpans(page, 'workspace.autosave.persist')).length, {
    timeout: 12_000,
  }).toBeGreaterThanOrEqual(1);
  await expect(page.locator('.transport-state')).not.toHaveText(/validating|preparing/, { timeout: 12_000 });
}

async function installAutosaveProbe(page: Page): Promise<void> {
  await page.evaluate(() => {
    const original = Storage.prototype.setItem;
    const host = window as typeof window & { __apgAutosaveWrites?: AutosaveWrite[] };
    host.__apgAutosaveWrites = [];
    Storage.prototype.setItem = function setItem(key, value) {
      if (key === 'apg.unit-editor.workspace.v2') {
        let hash = 2166136261;
        for (let index = 0; index < value.length; index += 1) {
          hash ^= value.charCodeAt(index);
          hash = Math.imul(hash, 16777619);
        }
        host.__apgAutosaveWrites?.push({
          at: performance.now(),
          bytes: new Blob([value]).size,
          signature: `${value.length}:${hash >>> 0}`,
        });
      }
      return original.call(this, key, value);
    };
  });
}

async function resetAutosaveProbe(page: Page): Promise<void> {
  await page.evaluate(() => {
    (window as typeof window & { __apgAutosaveWrites?: AutosaveWrite[] }).__apgAutosaveWrites = [];
  });
}

async function readAutosaveProbe(page: Page): Promise<AutosaveWrite[]> {
  return page.evaluate(() =>
    [...((window as typeof window & { __apgAutosaveWrites?: AutosaveWrite[] }).__apgAutosaveWrites ?? [])]);
}

function assertUniqueAutosaves(writes: AutosaveWrite[]): void {
  expect(writes.length).toBeGreaterThan(0);
  expect(new Set(writes.map(write => write.signature)).size).toBe(writes.length);
}

async function assertAutosaveBudget(page: Page): Promise<number> {
  const samples = await getSpans(page, 'workspace.autosave.persist');
  expect(samples.length).toBeGreaterThan(0);
  const maxMs = Math.max(...samples.map(sample => sample.durationMs));
  assertDurationBudget('workspace.autosave.persist', maxMs);
  return maxMs;
}

test.describe('UI performance checkpoints', () => {
  test.beforeEach(async ({ page }) => {
    await launchWorkspace(page);
  });

  test('@pr-medium drag-and-drop add on project graph', async ({ page }, testInfo) => {
    await waitForWorkspaceQuiescence(page);
    await clearPerfSpans(page);

    const before = await countProjectNodes(page);
    await page.getByTestId('project-unit-item-overdrive_unit').dragTo(page.getByTestId('project-canvas'), {
      targetPosition: { x: 280, y: 220 },
    });

    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBeGreaterThan(before);

    const dragStartMs = await runAndAssertBudget(page, 'ui.dragStart.projectUnit');
    const dragOverMs = await runAndAssertBudget(page, 'ui.dragOver.projectNode');
    const dropMs = await runAndAssertBudget(page, 'ui.drop.projectNode');
    const addMs = await runAndAssertBudget(page, 'graph.add.projectNode');
    expect((await getPerfCounters(page))['state.workspace.dispatches']).toBe(1);
    expect(Object.keys(await getComponentRenders(page, 'ProjectNode'))).toEqual(['ProjectNode:overdrive']);
    testInfo.annotations.push({ type: 'drag-start-ms', description: dragStartMs.toFixed(2) });
    testInfo.annotations.push({ type: 'drag-over-ms', description: dragOverMs.toFixed(2) });
    testInfo.annotations.push({ type: 'drop-commit-ms', description: dropMs.toFixed(2) });
    testInfo.annotations.push({ type: 'graph-add-ms', description: addMs.toFixed(2) });
  });

  test('inspector switching and parameter edit', async ({ page }) => {
    await page.getByTestId('project-node-drive1').click();
    await runAndAssertBudget(page, 'ui.select.projectNode');

    await clearPerfSpans(page);
    await page.getByTestId('inspector-tab-atom').click();
    await runAndAssertBudget(page, 'ui.change.inspectorView');

    await clearPerfSpans(page);
    const knob = page.getByTestId('param-knob-drive1-drive');
    const box = await knob.boundingBox();
    expect(box).not.toBeNull();

    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 - 40, { steps: 6 });
    await page.mouse.up();

    await runAndAssertBudget(page, 'param.update');
  });

  test('undo and redo are bounded', async ({ page }) => {
    await clearPerfSpans(page);
    await page.getByTestId('project-instance-unit').selectOption('tone_stack_unit');
    await page.getByTestId('project-instance-id').fill('perf_undo_unit');
    await page.getByTestId('project-instance-add').click();

    const withUnit = await countProjectNodes(page);
    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBeGreaterThan(0);

    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBeLessThan(withUnit);
    await runAndAssertBudget(page, 'ui.undo');

    await clearPerfSpans(page);
    await page.getByTestId('topbar-redo').click();
    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBe(withUnit);
    await runAndAssertBudget(page, 'ui.redo');
  });

  test('route create and delete latency', async ({ page }) => {
    await clearPerfSpans(page);
    await page.getByTestId('project-instance-unit').selectOption('tone_stack_unit');
    await page.getByTestId('project-instance-id').fill('perf_route_unit');
    await page.getByTestId('project-instance-add').click();

    const routeBefore = page.getByTestId(/^route-item-/);
    const beforeCount = await routeBefore.count();

    await page.locator('[data-testid="project-route-source"]').selectOption('trem1.output');
    await page.locator('[data-testid="project-route-target"]').selectOption('perf_route_unit.input');

    await clearPerfSpans(page);
    await page.getByTestId('project-route-add').click();
    await expect.poll(() => page.getByTestId(/^route-item-/).count(), { timeout: 5000 }).toBeGreaterThan(beforeCount);
    await runAndAssertBudget(page, 'graph.create.route');
    expect((await getPerfCounters(page))['state.workspace.dispatches']).toBe(1);
    expect(await getComponentRenders(page, 'ProjectNode')).toEqual({});

    await page.getByTestId('inspector-tab-atom').click();
    const latestRoute = page.getByTestId(/^route-item-/).last();
    await latestRoute.click();

    await clearPerfSpans(page);
    await page.getByTestId('inspector-route-disconnect').click();
    await expect.poll(() => page.getByTestId(/^route-item-/).count(), { timeout: 5000 }).toBe(beforeCount);
    await runAndAssertBudget(page, 'graph.delete.route');
    expect((await getPerfCounters(page))['state.workspace.dispatches']).toBe(1);
    expect(await getComponentRenders(page, 'ProjectNode')).toEqual({});
  });

  test('contract atom mutation latency', async ({ page }) => {
    await page.getByTestId('project-instance-unit').selectOption('tone_stack_unit');
    await page.getByTestId('project-instance-id').fill('perf_contract_unit');
    await page.getByTestId('project-instance-add').click();

    await page.getByTestId('project-node-perf_contract_unit').dblclick();
    await expect(page.getByTestId('contract-canvas')).toBeVisible();

    const list = page.locator('[data-testid^="contract-atom-item-"]');
    const before = await list.count();

    await clearPerfSpans(page);
    const atomAddSelect = page.locator('[data-testid="contract-atom-to-add"]');
    await expect(atomAddSelect).toBeVisible({ timeout: 10_000 });
    const firstAtom = await atomAddSelect.evaluate((select: HTMLSelectElement) => {
      const option = select.options.item(0);
      if (!option) return '';
      return option.value;
    });
    expect(firstAtom).toBeTruthy();
    await atomAddSelect.selectOption(firstAtom);
    await page.getByTestId('contract-atom-add').click();
    await expect.poll(() => list.count(), { timeout: 5000 }).toBeGreaterThan(before);
    await runAndAssertBudget(page, 'contract.add.atom');

    const added = list.last();
    await clearPerfSpans(page);
    await added.click();
    await page.getByTestId('contract-atom-remove').click();
    await expect.poll(() => list.count(), { timeout: 5000 }).toBe(before);
    await runAndAssertBudget(page, 'contract.remove.atom');
  });
});

test.describe('Scalability checkpoints', () => {
  test.beforeEach(async ({ page }) => {
    await launchWorkspace(page);
  });

  const fixtures = [
    { profile: 'small-linear', bucket: 'small', path: 'test/fixtures/projects-v2/perf/small-linear.project.v2.yaml' },
    { profile: 'medium-linear', bucket: 'medium', path: 'test/fixtures/projects-v2/perf/medium-linear.project.v2.yaml' },
    { profile: 'large-linear', bucket: 'large', path: 'test/fixtures/projects-v2/perf/large-linear.project.v2.yaml' },
    { profile: 'extreme-linear', bucket: 'extreme', path: 'test/fixtures/projects-v2/perf/extreme-linear.project.v2.yaml' },
  ];

  for (const fixture of fixtures) {
    test(`${fixture.bucket === 'medium' ? '@pr-medium @browser-matrix ' : ''}graph load and synchronize ${fixture.profile}`, async ({ page }, testInfo) => {
      const meta = readPerfFixtureMeta(fixture.path);
      await clearPerfSpans(page);
      const loadMs = await importPerfWorkspaceFixture(page, fixture.path, meta.nodes);
      assertDurationBudget(`graph.load.${fixture.bucket}`, loadMs);
      await waitForSpanCount(page, 'graph.sync.project', 1);
      testInfo.annotations.push({ type: 'graph-load-ms', description: `${fixture.profile}:${loadMs.toFixed(2)}` });
    });
  }

  test('rapid scale add/remove keeps graph mutation bounded', async ({ page }) => {
    const profile = 'test/fixtures/projects-v2/perf/large-linear.project.v2.yaml';
    const meta = readPerfFixtureMeta(profile);
    const baselineNodes = meta.nodes;

    await importPerfWorkspaceFixture(page, profile, baselineNodes);
    const before = await countProjectNodes(page);
    expect(before).toBeGreaterThan(0);
    expect(before).toBe(baselineNodes);

    await page.getByTestId('project-instance-unit').selectOption('perf_fanwide_24in');
    for (let index = 0; index < 12; index += 1) {
      await page.getByTestId('project-instance-id').fill(`perf_scale_${index}`);
      await page.getByTestId('project-instance-add').click();
      await expect(page.getByTestId(`project-instance-item-perf_scale_${index}`)).toBeVisible();
      await runAndAssertBudget(page, 'graph.add.projectNode', 1);
    }

    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBe(before + 12);

    for (let index = 0; index < 12; index += 1) {
      await page.getByTestId(`project-instance-item-perf_scale_${index}`).click();
      await page.getByTestId('inspector-instance-remove').click();
      await runAndAssertBudget(page, 'graph.remove.projectNode', 1);
      await runAndAssertBudget(page, 'graph.sync.project', 1);
    }

    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBe(before);
  });

  test('native unit drop stays bounded when the referenced unit contains 500 atoms', async ({ page }, testInfo) => {
    const profile = 'test/fixtures/projects-v2/perf/large-atoms.project.v2.yaml';
    await importPerfWorkspaceFixture(page, profile, 1);
    await clearPerfSpans(page);

    await page.getByTestId('project-unit-item-perf_atoms_500').dragTo(page.getByTestId('project-canvas'), {
      targetPosition: { x: 420, y: 260 },
    });
    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBe(2);
    expect(await page.locator('.contract-node').count()).toBe(0);

    const addMs = await runAndAssertBudget(page, 'graph.add.projectNode', 1);
    await runAndAssertBudget(page, 'ui.dragStart.projectUnit', 1);
    await runAndAssertBudget(page, 'ui.drop.projectNode', 1);
    await page.getByTestId('project-node-perf_atoms_500').dblclick();
    await expect(page.getByTestId('contract-canvas')).toHaveAttribute('data-atom-count', '500');
    testInfo.annotations.push({ type: 'large-unit-drop-ms', description: addMs.toFixed(2) });
  });

  test('autosave persists below budget during rapid edits', async ({ page }, testInfo) => {
    await page.getByTestId('project-node-drive1').click();
    await page.getByTestId('inspector-tab-atom').click();
    const knob = page.getByTestId('param-knob-drive1-drive');
    const box = await knob.boundingBox();
    expect(box).not.toBeNull();

    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    await clearPerfSpans(page);
    const before = await countProjectNodes(page);
    await expect(before).toBeGreaterThan(0);

    for (let index = 0; index < 24; index += 1) {
      await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
      await page.mouse.down();
      const dy = (index % 2 === 0 ? -1 : 1) * ((index % 6) + 1) * 2;
      await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 + dy);
      await page.mouse.up();
    }

    await expect.poll(async () => (await getSpans(page, 'workspace.autosave.persist')).length, {
      timeout: 12_000,
    }).toBeGreaterThanOrEqual(1);

    await runAndAssertBudget(page, 'param.update', 24);
    const autosaveMs = await runAndAssertBudget(page, 'workspace.autosave.persist', 1);
    testInfo.annotations.push({ type: 'autosave-ms', description: autosaveMs.toFixed(2) });
  });

  test('autosave handles slow and failing storage without losing the last good snapshot', async ({ page }, testInfo) => {
    const pageErrors: string[] = [];
    page.on('pageerror', error => pageErrors.push(error.message));
    await waitForWorkspaceQuiescence(page);
    await page.getByTestId('project-node-drive1').click();
    await page.getByTestId('inspector-tab-atom').click();
    const knob = page.getByTestId('param-knob-drive1-drive');
    const box = await knob.boundingBox();
    expect(box).not.toBeNull();

    await page.evaluate(() => {
      const original = Storage.prototype.setItem;
      const host = window as typeof window & { __apgStorageWrites?: number };
      host.__apgStorageWrites = 0;
      Storage.prototype.setItem = function setItem(key, value) {
        if (key === 'apg.unit-editor.workspace.v2') {
          host.__apgStorageWrites = (host.__apgStorageWrites ?? 0) + 1;
          const startedAt = performance.now();
          while (performance.now() - startedAt < 6) {
            // Intentional synchronous storage latency for the browser budget gate.
          }
        }
        return original.call(this, key, value);
      };
    });

    await clearPerfSpans(page);
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 - 12);
    await page.mouse.up();
    const slowAutosaveMs = await runAndAssertBudget(page, 'workspace.autosave.persist', 1);
    expect(await page.evaluate(() => (window as typeof window & { __apgStorageWrites?: number }).__apgStorageWrites)).toBe(1);
    const lastGoodSnapshot = await page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'));
    expect(lastGoodSnapshot).not.toBeNull();

    await page.evaluate(() => {
      const previous = Storage.prototype.setItem;
      Storage.prototype.setItem = function setItem(key, value) {
        if (key === 'apg.unit-editor.workspace.v2') {
          throw new DOMException('Injected quota failure', 'QuotaExceededError');
        }
        return previous.call(this, key, value);
      };
    });
    await clearPerfSpans(page);
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 + 12);
    await page.mouse.up();

    await expect(page.getByTestId('workspace-save-status')).toHaveText('Save failed', { timeout: 5000 });
    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    expect(await page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).toBe(lastGoodSnapshot);
    await page.keyboard.press('Control+s');
    await expect(page.getByTestId('workspace-save-status')).toHaveText('Save failed');
    expect(pageErrors).toEqual([]);
    testInfo.annotations.push({ type: 'slow-autosave-ms', description: slowAutosaveMs.toFixed(2) });
  });

  test('thirty-second continuous parameter editing coalesces to one autosave', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The continuous autosave gate runs in scheduled performance CI.');
    const fixture = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    await openContractFixture(page, fixture, readPerfFixtureMeta(fixture).atoms);
    await page.getByTestId('contract-atom-item-atom_0001').click();
    const threshold = page.getByLabel('atom_0001 config threshold');
    await expect(threshold).toBeVisible();
    await waitForWorkspaceQuiescence(page);
    await installAutosaveProbe(page);
    await page.clock.install();
    await clearPerfSpans(page);

    for (let index = 0; index < 120; index += 1) {
      await threshold.fill((0.5 + index / 1000).toFixed(3));
      await page.clock.fastForward(250);
    }
    await page.clock.fastForward(400);

    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    const editSamples = await getSpans(page, 'contract.edit.atom');
    expect(editSamples.length).toBeGreaterThanOrEqual(60);
    assertDurationBudget('contract.edit.atom', Math.max(...editSamples.map(sample => sample.durationMs)));
    const writes = await readAutosaveProbe(page);
    expect(writes).toHaveLength(1);
    const autosaveMs = await assertAutosaveBudget(page);
    testInfo.annotations.push({ type: 'parameter-burst-autosave-ms', description: autosaveMs.toFixed(2) });
    testInfo.annotations.push({ type: 'parameter-burst-autosave-writes', description: String(writes.length) });
    testInfo.annotations.push({ type: 'parameter-burst-edits', description: '120 over 30 seconds' });
  });

  test('drag and routing bursts avoid redundant autosaves', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The drag and routing autosave gate runs in scheduled performance CI.');
    await waitForWorkspaceQuiescence(page);
    await installAutosaveProbe(page);
    await clearPerfSpans(page);

    const drive = page.getByTestId('project-node-drive1');
    const driveBox = await drive.boundingBox();
    expect(driveBox).not.toBeNull();
    const dragStart = { x: driveBox!.x + 20, y: driveBox!.y + 20 };
    await page.mouse.move(dragStart.x, dragStart.y);
    await page.mouse.down();
    await page.mouse.move(dragStart.x + 120, dragStart.y + 60, { steps: 80 });
    await page.mouse.up();
    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    await runAndAssertBudget(page, 'graph.move.projectNode', 1);
    expect(await readAutosaveProbe(page)).toHaveLength(1);

    await clearPerfSpans(page);
    await page.getByTestId('project-instance-unit').selectOption('tone_stack_unit');
    await page.getByTestId('project-instance-id').fill('autosave_route_unit');
    await page.getByTestId('project-instance-add').click();
    await waitForWorkspaceQuiescence(page);
    await page.getByTestId('inspector-tab-atom').click();
    await page.getByTestId('project-route-source').selectOption('trem1.output');
    await page.getByTestId('project-route-target').selectOption('autosave_route_unit.input');
    await resetAutosaveProbe(page);
    await clearPerfSpans(page);
    const baselineRoutes = await page.getByTestId(/^route-item-/).count();

    for (let index = 0; index < 20; index += 1) {
      await page.getByTestId('project-route-add').click();
      await expect(page.getByTestId(/^route-item-/)).toHaveCount(baselineRoutes + 1);
      await page.getByTestId(/^route-item-/).last().click();
      await page.getByTestId('inspector-route-disconnect').click();
      await expect(page.getByTestId(/^route-item-/)).toHaveCount(baselineRoutes);
    }
    await page.waitForTimeout(450);
    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    const writes = await readAutosaveProbe(page);
    expect(new Set(writes.map(write => write.signature)).size).toBe(writes.length);
    expect(writes.length).toBeLessThanOrEqual(40);
    const autosaveMs = await assertAutosaveBudget(page);
    testInfo.annotations.push({ type: 'routing-burst-autosave-ms', description: autosaveMs.toFixed(2) });
    testInfo.annotations.push({ type: 'routing-burst-autosave-writes', description: String(writes.length) });
  });

  test('one hundred atom additions and a large payload keep autosave bounded', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The structural autosave gate runs in scheduled performance CI.');
    test.setTimeout(300_000);
    const smallFixture = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    await openContractFixture(page, smallFixture, readPerfFixtureMeta(smallFixture).atoms);
    await waitForWorkspaceQuiescence(page);
    await installAutosaveProbe(page);
    await clearPerfSpans(page);
    const atomList = page.locator('[data-testid^="contract-atom-item-"]');
    const initialAtoms = await atomList.count();
    const add = page.getByTestId('contract-atom-add');

    for (let index = 0; index < 100; index += 1) {
      await add.click();
      await expect(atomList).toHaveCount(initialAtoms + index + 1);
    }
    await page.waitForTimeout(450);
    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    const addSamples = await getSpans(page, 'contract.add.atom');
    expect(addSamples.length).toBeGreaterThanOrEqual(60);
    assertDurationBudget('contract.add.atom', Math.max(...addSamples.map(sample => sample.durationMs)));
    let writes = await readAutosaveProbe(page);
    assertUniqueAutosaves(writes);
    expect(writes.length).toBeLessThanOrEqual(100);
    const atomBurstWrites = writes.length;
    const atomBurstAutosaveMs = await assertAutosaveBudget(page);

    const largeFixture = 'test/fixtures/projects-v2/perf/large-atoms.project.v2.yaml';
    await resetAutosaveProbe(page);
    await clearPerfSpans(page);
    await importPerfWorkspaceFixture(page, largeFixture, 1);
    await page.waitForTimeout(450);
    await waitForSpanCount(page, 'workspace.autosave.persist', 1);
    writes = await readAutosaveProbe(page);
    assertUniqueAutosaves(writes);
    expect(Math.max(...writes.map(write => write.bytes))).toBeGreaterThan(100_000);
    const largePayloadAutosaveMs = await assertAutosaveBudget(page);

    testInfo.annotations.push({ type: 'atom-burst-autosave-ms', description: atomBurstAutosaveMs.toFixed(2) });
    testInfo.annotations.push({ type: 'atom-burst-autosave-writes', description: String(atomBurstWrites) });
    testInfo.annotations.push({ type: 'large-payload-autosave-ms', description: largePayloadAutosaveMs.toFixed(2) });
    testInfo.annotations.push({ type: 'large-payload-bytes', description: String(Math.max(...writes.map(write => write.bytes))) });
  });

  test('one-hour autosave session keeps writes and residual heap bounded', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The one-hour emulation runs in scheduled performance CI.');
    const fixture = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);
    await page.getByTestId('contract-atom-item-atom_0001').click();
    const threshold = page.getByLabel('atom_0001 config threshold');
    await expect(threshold).toBeVisible();
    await waitForWorkspaceQuiescence(page);
    await page.clock.install();
    await page.evaluate(() => {
      const original = Storage.prototype.setItem;
      const host = window as typeof window & { __apgLongSessionWrites?: number };
      host.__apgLongSessionWrites = 0;
      Storage.prototype.setItem = function setItem(key, value) {
        if (key === 'apg.unit-editor.workspace.v2') {
          host.__apgLongSessionWrites = (host.__apgLongSessionWrites ?? 0) + 1;
        }
        return original.call(this, key, value);
      };
    });

    const runHalfHour = async (offset: number) => {
      for (let index = 0; index < 60; index += 1) {
        await threshold.fill((0.5 + (offset + index) / 1000).toFixed(3));
        await page.clock.fastForward(30_000);
      }
    };

    await runHalfHour(0);
    const halfHourHeap = await collectHeapBytes(page);
    await runHalfHour(60);
    const oneHourHeap = await collectHeapBytes(page);
    const writes = await page.evaluate(() =>
      (window as typeof window & { __apgLongSessionWrites?: number }).__apgLongSessionWrites ?? 0);
    const growth = (oneHourHeap - halfHourHeap) / halfHourHeap;

    expect(writes).toBe(120);
    expect(growth).toBeLessThanOrEqual(0.1);
    await expect(page.getByTestId('workspace-save-status')).not.toHaveText('Save failed');
    testInfo.annotations.push({ type: 'long-session-writes', description: String(writes) });
    testInfo.annotations.push({
      type: 'long-session-heap-growth',
      description: `${(growth * 100).toFixed(2)}% (${halfHourHeap} -> ${oneHourHeap})`,
    });
  });

  test('memory growth stays bounded after repeated add/remove', async ({ page }, testInfo) => {
    const profile = 'test/fixtures/projects-v2/perf/extreme-linear.project.v2.yaml';
    const meta = readPerfFixtureMeta(profile);
    await importPerfWorkspaceFixture(page, profile, meta.nodes);

    await page.getByTestId('project-instance-unit').selectOption('perf_fanwide_24in');
    const addAndRemove = async (prefix: string, count: number) => {
      for (let index = 0; index < count; index += 1) {
        const id = `${prefix}_${index}`;
        await page.getByTestId('project-instance-id').fill(id);
        await page.getByTestId('project-instance-add').click();
        await page.getByTestId(`project-instance-item-${id}`).click();
        await page.getByTestId('inspector-instance-remove').click();
      }
    };

    const historyWindowIterations = 25;
    await clearPerfSpans(page);
    await addAndRemove('perf_warm', historyWindowIterations);
    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBe(meta.nodes);
    await waitForWorkspaceQuiescence(page);
    const baseHeap = await collectHeapBytes(page);

    await clearPerfSpans(page);
    await addAndRemove('perf_mem', historyWindowIterations);
    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBe(meta.nodes);
    await waitForWorkspaceQuiescence(page);
    const endHeap = await collectHeapBytes(page);
    const growth = (endHeap - baseHeap) / baseHeap;
    testInfo.annotations.push({
      type: 'heap-growth',
      description: `${(growth * 100).toFixed(2)}% (${baseHeap} -> ${endHeap})`,
    });
    expect(growth).toBeLessThanOrEqual(0.1);
  });

  test('removed atoms are not retained across twenty 100-atom cycles', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The 20x100 atom retention gate runs in scheduled performance CI.');
    test.setTimeout(300_000);
    const profile = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    const payloads = buildAtomRetentionPayloads(profile, 100);
    const cycle = async () => {
      await importWorkspacePayload(page, payloads.stressed, 1);
      await openLoadedContract(page, 125);
      await importWorkspacePayload(page, payloads.baseline, 1);
      await openLoadedContract(page, 25);
    };

    for (let index = 0; index < 25; index += 1) await cycle();
    await waitForWorkspaceQuiescence(page);
    await clearPerfSpans(page);
    const saturatedHeap = await collectHeapBytes(page);

    for (let index = 0; index < 20; index += 1) await cycle();
    await waitForWorkspaceQuiescence(page);
    await clearPerfSpans(page);
    const finalHeap = await collectHeapBytes(page);
    const growth = (finalHeap - saturatedHeap) / saturatedHeap;

    await expect(page.getByTestId('contract-canvas')).toHaveAttribute('data-atom-count', '25');
    expect(growth).toBeLessThanOrEqual(0.1);
    testInfo.annotations.push({ type: 'atom-retention-cycles', description: '20x100' });
    testInfo.annotations.push({
      type: 'atom-retention-heap-growth',
      description: `${(growth * 100).toFixed(2)}% (${saturatedHeap} -> ${finalHeap})`,
    });
  });

  test('inspector, edge, replacement, reload, and navigation lifecycles stay bounded', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The complete UI lifecycle memory gate runs in scheduled performance CI.');
    test.setTimeout(300_000);
    const defaultPayload = buildPerfWorkspacePayload('test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml');
    const mediumPayload = buildPerfWorkspacePayload('test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml');

    const exerciseWindow = async (suffix: string) => {
      await importWorkspacePayload(page, defaultPayload, 7);
      await page.getByTestId('project-instance-unit').selectOption('tone_stack_unit');
      await page.getByTestId('project-instance-id').fill(`lifecycle_route_${suffix}`);
      await page.getByTestId('project-instance-add').click();
      await page.getByTestId('inspector-tab-atom').click();
      await page.getByTestId('project-route-source').selectOption('trem1.output');
      await page.getByTestId('project-route-target').selectOption(`lifecycle_route_${suffix}.input`);
      const baselineRoutes = await page.getByTestId(/^route-item-/).count();

      for (let index = 0; index < 10; index += 1) {
        await page.getByTestId('project-route-add').click();
        await expect(page.getByTestId(/^route-item-/)).toHaveCount(baselineRoutes + 1);
        await page.getByTestId(/^route-item-/).last().click();
        await page.getByTestId('inspector-route-disconnect').click();
        await expect(page.getByTestId(/^route-item-/)).toHaveCount(baselineRoutes);
      }

      await page.getByTestId('project-node-drive1').dblclick();
      await expect(page.getByTestId('contract-canvas')).toBeVisible();
      await page.getByTestId('contract-atom-item-clip_drive').click();
      const type = page.getByTestId('contract-atom-type');
      for (let index = 0; index < 10; index += 1) {
        const nextType = index % 2 === 0 ? 'amplitude_clip_hard' : 'amplitude_clip_soft';
        await page.getByTestId('contract-atom-replace-open').click();
        await page.getByTestId('contract-atom-replace-type').selectOption(nextType);
        await page.getByTestId('contract-atom-replace-preserve').check();
        await page.getByTestId('contract-atom-replace-confirm').click();
        await expect(type).toHaveText(new RegExp(nextType));
      }

      const panel = page.getByTestId('contract-selected-atom-panel');
      const inspectorCycles = await panel.evaluate((details: HTMLDetailsElement) => {
        const summary = details.querySelector('summary');
        if (!summary) throw new Error('Selected atom summary is missing.');
        for (let index = 0; index < 500; index += 1) {
          summary.click();
          summary.click();
        }
        return 500;
      });
      expect(inspectorCycles).toBe(500);
      await expect(panel).toHaveJSProperty('open', true);

      for (let index = 0; index < 10; index += 1) {
        await page.getByRole('button', { name: 'Project graph' }).click();
        await expect(page.getByTestId('project-canvas')).toBeVisible();
        await page.getByTestId('project-node-drive1').dblclick();
        await expect(page.getByTestId('contract-canvas')).toBeVisible();
      }

      for (let index = 0; index < 5; index += 1) {
        await importWorkspacePayload(page, mediumPayload, 1);
        await importWorkspacePayload(page, defaultPayload, 7);
      }
      await page.waitForTimeout(450);
    };

    await exerciseWindow('warm');
    const baselineHeap = await collectHeapBytes(page);
    await exerciseWindow('measure');
    const finalHeap = await collectHeapBytes(page);
    const growth = (finalHeap - baselineHeap) / baselineHeap;

    expect(growth).toBeLessThanOrEqual(0.1);
    testInfo.annotations.push({ type: 'lifecycle-inspector-cycles', description: '1000' });
    testInfo.annotations.push({ type: 'lifecycle-route-cycles', description: '20' });
    testInfo.annotations.push({ type: 'lifecycle-replacement-cycles', description: '20' });
    testInfo.annotations.push({ type: 'lifecycle-reload-cycles', description: '20' });
    testInfo.annotations.push({ type: 'lifecycle-navigation-cycles', description: '20' });
    testInfo.annotations.push({
      type: 'lifecycle-heap-growth',
      description: `${(growth * 100).toFixed(2)}% (${baselineHeap} -> ${finalHeap})`,
    });
  });
});

test.describe('Contract graph atom scalability', () => {
  test.beforeEach(async ({ page }) => {
    await launchWorkspace(page);
  });

  const fixtures = [
    { profile: 'small-atoms', bucket: 'small', path: 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml' },
    { profile: 'medium-atoms', bucket: 'medium', path: 'test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml' },
    { profile: 'medium-atoms-branching', bucket: 'medium', path: 'test/fixtures/projects-v2/perf/medium-atoms-branching.project.v2.yaml' },
    { profile: 'medium-atoms-dense', bucket: 'medium', path: 'test/fixtures/projects-v2/perf/medium-atoms-dense.project.v2.yaml' },
    { profile: 'medium-atoms-payload', bucket: 'medium', path: 'test/fixtures/projects-v2/perf/medium-atoms-payload.project.v2.yaml' },
    { profile: 'large-atoms', bucket: 'large', path: 'test/fixtures/projects-v2/perf/large-atoms.project.v2.yaml' },
  ];

  for (const fixture of fixtures) {
    test(`opens and renders ${fixture.profile}`, async ({ page }, testInfo) => {
      const meta = readPerfFixtureMeta(fixture.path);
      const samples = [await openContractFixture(page, fixture.path, meta.atoms)];
      for (let trial = 1; trial < 3; trial += 1) {
        await page.getByRole('button', { name: 'Project graph' }).click();
        await expect(page.getByTestId('project-node-atom_stress')).toBeVisible();
        samples.push(await openLoadedContract(page, meta.atoms));
      }
      const loadMs = median(samples);
      assertDurationBudget(`contract.load.${fixture.bucket}`, loadMs);
      testInfo.annotations.push({
        type: 'contract-load-ms',
        description: `${fixture.profile}:median=${loadMs.toFixed(2)} samples=${samples.map(value => value.toFixed(2)).join(',')}`,
      });
    });
  }

  test('invalid atom binding is visible without breaking contract inspection', async ({ page }) => {
    const fixture = 'test/fixtures/projects-v2/perf/small-atoms-invalid.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    const pageErrors: string[] = [];
    page.on('pageerror', error => pageErrors.push(error.message));

    await importPerfWorkspaceFixture(page, fixture, 1);
    await expect(page.locator('.transport-state')).toHaveText('error', { timeout: 12_000 });
    await expect(page.locator('.transport-state')).toHaveAttribute('title', /missing_signal/);
    await openLoadedContract(page, meta.atoms);
    await expect(page.getByTestId('contract-canvas')).toHaveAttribute('data-atom-count', String(meta.atoms));
    expect(pageErrors).toEqual([]);
  });

  test('opens the 1,000-atom failure boundary in scheduled runs', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The 1,000-atom boundary runs in scheduled performance CI.');
    const fixture = 'test/fixtures/projects-v2/perf/extreme-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    const samples = [await openContractFixture(page, fixture, meta.atoms)];
    for (let trial = 1; trial < 3; trial += 1) {
      await page.getByRole('button', { name: 'Project graph' }).click();
      await expect(page.getByTestId('project-node-atom_stress')).toBeVisible();
      samples.push(await openLoadedContract(page, meta.atoms));
    }
    const loadMs = median(samples);
    assertDurationBudget('contract.load.extreme', loadMs);
    testInfo.annotations.push({
      type: 'contract-load-ms',
      description: `extreme-atoms:median=${loadMs.toFixed(2)} samples=${samples.map(value => value.toFixed(2)).join(',')}`,
    });
  });

  test('selecting one atom does not rerender unrelated contract nodes', async ({ page }, testInfo) => {
    const fixture = 'test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    await clearPerfSpans(page);
    const visibleAtom = await getVisibleContractAtom(page);
    expect(visibleAtom).not.toBeNull();
    const selected = page.getByTestId(`contract-node-${visibleAtom!.id}`);
    await selected.click();
    await expect(selected).toHaveClass(/contract-node--selected/);
    await expect.poll(async () => Object.keys(await getComponentRenders(page, 'ContractNode')).length).toBeGreaterThan(0);

    const renders = await getComponentRenders(page, 'ContractNode');
    const renderedNodes = Object.keys(renders);
    testInfo.annotations.push({ type: 'contract-node-renders', description: renderedNodes.join(',') });
    expect(renderedNodes).toEqual([`ContractNode:${visibleAtom!.id}`]);
  });

  test('contract drop rejects invalid data, cancels cleanly, and survives rapid transformed drops', async ({ page }) => {
    const fixture = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    const canvas = page.getByTestId('contract-canvas');
    await dispatchContractDrag(page, 'dragover', null);
    await expect(canvas).toHaveClass(/flow-shell--drop-reject/);
    await dispatchContractDrag(page, 'drop', null);
    await expect(canvas).toHaveClass(/flow-shell--drop-idle/);
    await expect(canvas).toHaveAttribute('data-atom-count', String(meta.atoms));

    await dispatchContractDrag(page, 'dragover', 'amplitude_clip_soft');
    await expect(canvas).toHaveClass(/flow-shell--drop-valid/);
    await page.keyboard.press('Escape');
    await expect(canvas).toHaveClass(/flow-shell--drop-idle/);
    await expect(canvas).toHaveAttribute('data-atom-count', String(meta.atoms));

    const pane = page.locator('.react-flow__pane');
    const paneBox = await pane.boundingBox();
    expect(paneBox).not.toBeNull();
    await page.mouse.move(paneBox!.x + 30, paneBox!.y + 30);
    await page.mouse.down();
    await page.mouse.move(paneBox!.x + 110, paneBox!.y + 70, { steps: 5 });
    await page.mouse.up();
    await page.locator('.react-flow__controls-zoomin').click();

    for (let index = 1; index <= 3; index += 1) {
      await clearPerfSpans(page);
      await dispatchContractDrag(page, 'drop', 'amplitude_clip_soft');
      await expect(canvas).toHaveAttribute('data-atom-count', String(meta.atoms + index));
      await runAndAssertBudget(page, 'contract.add.atom', 1);
    }

    const filter = page.getByTestId('atom-palette-filter');
    await filter.fill('clip_soft');
    const filteredAtom = page.getByTestId('atom-palette-item-amplitude_clip_soft');
    await expect(filteredAtom).toBeVisible();
    await expect(page.locator('[data-testid^="atom-palette-item-"]')).toHaveCount(1);

    await clearPerfSpans(page);
    await filteredAtom.dragTo(canvas, { targetPosition: { x: 420, y: 260 } });
    await expect(canvas).toHaveAttribute('data-atom-count', String(meta.atoms + 4));
    await runAndAssertBudget(page, 'ui.dragStart.atomPalette', 1, 'ui.dragStart.projectUnit');
    await runAndAssertBudget(page, 'ui.drop.contractAtom', 1, 'ui.drop.projectNode');
    await runAndAssertBudget(page, 'contract.add.atom', 1);
  });

  test('dropping an atom on an edge splits the connection in one undoable transaction', async ({ page }, testInfo) => {
    const fixture = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    const oldEdgeId = 'contract-edge-contract-atom_0001-contract-atom_0002-signal';
    const firstEdgeId = 'contract-edge-contract-atom_0001-contract-amplitude_clip_soft-signal';
    const secondEdgeId = 'contract-edge-contract-amplitude_clip_soft-contract-atom_0002-signal';
    await expect(page.getByTestId(`rf__edge-${oldEdgeId}`)).toBeVisible();
    await clearPerfSpans(page);
    await dispatchContractEdgeDrop(page, oldEdgeId, 'amplitude_clip_soft');

    await expect(page.getByTestId('contract-canvas')).toHaveAttribute('data-atom-count', String(meta.atoms + 1));
    await expect(page.getByTestId('contract-node-amplitude_clip_soft')).toBeVisible();
    await expect(page.getByTestId(`rf__edge-${oldEdgeId}`)).toHaveCount(0);
    await expect(page.getByTestId(`rf__edge-${firstEdgeId}`)).toBeVisible();
    await expect(page.getByTestId(`rf__edge-${secondEdgeId}`)).toBeVisible();
    const insertMs = await runAndAssertBudget(page, 'contract.insert.atom', 1);
    expect((await getPerfCounters(page))['state.workspace.dispatches']).toBe(1);
    const renderedNodes = Object.keys(await getComponentRenders(page, 'ContractNode')).sort();
    expect(renderedNodes).toEqual(['ContractNode:amplitude_clip_soft', 'ContractNode:atom_0002']);

    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await expect(page.getByTestId('contract-canvas')).toHaveAttribute('data-atom-count', String(meta.atoms));
    await expect(page.getByTestId(`rf__edge-${oldEdgeId}`)).toBeVisible();
    await expect(page.getByTestId('contract-node-amplitude_clip_soft')).toHaveCount(0);
    await runAndAssertBudget(page, 'ui.undo', 1);
    testInfo.annotations.push({ type: 'edge-insert-ms', description: insertMs.toFixed(2) });
    testInfo.annotations.push({ type: 'edge-insert-node-renders', description: renderedNodes.join(',') });
  });

  test('reconnect is atomic, undoable, and only rerenders affected nodes and edge', async ({ page }, testInfo) => {
    const fixture = 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    await dispatchContractDrag(page, 'drop', 'amplitude_clip_soft');
    await expect(page.getByTestId('contract-canvas')).toHaveAttribute('data-atom-count', String(meta.atoms + 1));
    const addedNode = page.locator('.contract-node--selected');
    await expect(addedNode).toBeVisible();
    const addedNodeTestId = await addedNode.getAttribute('data-testid');
    expect(addedNodeTestId).toMatch(/^contract-node-/);
    const addedNodeId = addedNodeTestId!.replace(/^contract-node-/, '');

    const oldEdgeId = 'contract-edge-contract-atom_0002-contract-atom_0003-signal';
    const nextEdgeId = `contract-edge-contract-atom_0002-contract-${addedNodeId}-signal`;
    const oldEdge = page.getByTestId(`rf__edge-${oldEdgeId}`);
    await expect(oldEdge).toBeVisible();
    await oldEdge.dispatchEvent('click');
    const targetUpdater = oldEdge.locator('.react-flow__edgeupdater-target');
    await expect(targetUpdater).toBeVisible();
    const targetUpdaterBox = await targetUpdater.boundingBox();
    const replacementTarget = page.getByTestId(`contract-node-${addedNodeId}`).locator('.contract-node__handle--in');
    const replacementTargetBox = await replacementTarget.boundingBox();
    expect(targetUpdaterBox).not.toBeNull();
    expect(replacementTargetBox).not.toBeNull();

    await clearPerfSpans(page);
    await page.mouse.move(
      targetUpdaterBox!.x + targetUpdaterBox!.width / 2,
      targetUpdaterBox!.y + targetUpdaterBox!.height / 2,
    );
    await page.mouse.down();
    await page.mouse.move(
      replacementTargetBox!.x + replacementTargetBox!.width / 2,
      replacementTargetBox!.y + replacementTargetBox!.height / 2,
      { steps: 8 },
    );
    await page.mouse.up();

    await waitForSpanCount(page, 'contract.reconnect.atom', 1);
    await expect(page.getByTestId(`rf__edge-${nextEdgeId}`)).toBeVisible();
    await expect(oldEdge).toHaveCount(0);
    const reconnectMs = await runAndAssertBudget(page, 'contract.reconnect.atom', 1);
    expect((await getPerfCounters(page))['state.workspace.dispatches']).toBe(1);
    const renderedNodes = Object.keys(await getComponentRenders(page, 'ContractNode')).sort();
    const renderedEdges = Object.keys(await getComponentRenders(page, 'ContractEdge'));
    expect(renderedNodes).toEqual([`ContractNode:${addedNodeId}`, 'ContractNode:atom_0003'].sort());
    expect(renderedEdges).toContain(`ContractEdge:${nextEdgeId}`);
    expect(renderedEdges.every(key => key === `ContractEdge:${oldEdgeId}` || key === `ContractEdge:${nextEdgeId}`)).toBe(true);

    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await expect(page.getByTestId(`rf__edge-${oldEdgeId}`)).toBeVisible();
    await expect(page.getByTestId(`rf__edge-${nextEdgeId}`)).toHaveCount(0);
    await runAndAssertBudget(page, 'ui.undo', 1);
    testInfo.annotations.push({ type: 'reconnect-ms', description: reconnectMs.toFixed(2) });
    testInfo.annotations.push({ type: 'reconnect-node-renders', description: renderedNodes.join(',') });
    testInfo.annotations.push({ type: 'reconnect-edge-renders', description: renderedEdges.join(',') });
  });

  test('editing config in a 500-atom graph stays local', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The 500-atom interaction gate runs in scheduled performance CI.');
    const fixture = 'test/fixtures/projects-v2/perf/large-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    const visibleAtom = await getVisibleContractAtom(page);
    expect(visibleAtom).not.toBeNull();
    await page.getByTestId(`contract-atom-item-${visibleAtom!.id}`).click();
    const config = page.getByLabel(`${visibleAtom!.id} config threshold`);
    await expect(config).toBeVisible();

    await clearPerfSpans(page);
    await config.fill('0.8');
    await waitForSpanCount(page, 'contract.edit.atom', 1);
    await expect(config).toHaveValue('0.8');
    await expect.poll(async () => Object.keys(await getComponentRenders(page, 'ContractNode')).length).toBeGreaterThan(0);

    const editMs = await runAndAssertBudget(page, 'contract.edit.atom', 1);
    const renderedNodes = Object.keys(await getComponentRenders(page, 'ContractNode'));
    testInfo.annotations.push({ type: 'contract-config-ms', description: editMs.toFixed(2) });
    testInfo.annotations.push({ type: 'contract-node-renders', description: renderedNodes.join(',') });
    expect(renderedNodes).toEqual([`ContractNode:${visibleAtom!.id}`]);
  });

  test('500-atom pointer drag, pan, and zoom remain responsive', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The 500-atom interaction gate runs in scheduled performance CI.');
    const fixture = 'test/fixtures/projects-v2/perf/large-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    const visibleNode = await getVisibleContractAtom(page);
    expect(visibleNode).not.toBeNull();

    const devtoolsTrace = await startDevtoolsTrace(page);
    await clearPerfSpans(page);
    await page.mouse.move(visibleNode!.x, visibleNode!.y);
    await page.mouse.down();
    await page.mouse.move(visibleNode!.x + 32, visibleNode!.y + 24, { steps: 6 });
    await page.mouse.up();
    await waitForSpanCount(page, 'contract.move.atom', 1);
    const moveMs = await runAndAssertBudget(page, 'contract.move.atom', 1);
    const movedAtomId = visibleNode!.id;
    const dragRenders = Object.keys(await getComponentRenders(page, 'ContractNode'));
    expect(dragRenders).toEqual([`ContractNode:${movedAtomId}`]);

    const pane = page.locator('.react-flow__pane');
    const paneBox = await pane.boundingBox();
    expect(paneBox).not.toBeNull();
    await clearPerfSpans(page);
    await startInteractionProbe(page);
    await page.mouse.move(paneBox!.x + 24, paneBox!.y + 24);
    await page.mouse.down();
    await page.mouse.move(paneBox!.x + 124, paneBox!.y + 84, { steps: 20 });
    await page.mouse.up();
    await page.mouse.move(paneBox!.x + paneBox!.width / 2, paneBox!.y + paneBox!.height / 2);
    await page.mouse.wheel(0, -360);
    await page.mouse.wheel(0, 360);
    const probe = await stopInteractionProbe(page);
    const trace = await stopDevtoolsTrace(devtoolsTrace);
    writeFileSync(testInfo.outputPath('devtools-trace.json'), JSON.stringify({ traceEvents: trace.events }));

    expect(probe.frameIntervals.length).toBeGreaterThan(5);
    const averageFrameMs = probe.frameIntervals.reduce((sum, value) => sum + value, 0) / probe.frameIntervals.length;
    assertDurationBudget('interaction.frame.average', averageFrameMs);
    expect(probe.longTasks.filter(duration => duration > 50).length).toBeLessThanOrEqual(1);
    const viewportRenders = Object.keys(await getComponentRenders(page, 'ContractNode'));
    expect(viewportRenders.length).toBeLessThan(meta.atoms * 0.25);
    testInfo.annotations.push({ type: 'contract-move-ms', description: moveMs.toFixed(2) });
    testInfo.annotations.push({
      type: 'interaction-frames',
      description: `average=${averageFrameMs.toFixed(2)}ms frames=${probe.frameIntervals.length}`,
    });
    testInfo.annotations.push({ type: 'interaction-long-tasks', description: probe.longTasks.join(',') || 'none' });
    testInfo.annotations.push({ type: 'viewport-node-renders', description: String(viewportRenders.length) });
    testInfo.annotations.push({
      type: 'devtools-render-costs',
      description: `style=${trace.styleMs.toFixed(2)}ms layout=${trace.layoutMs.toFixed(2)}ms paint=${trace.paintMs.toFixed(2)}ms`,
    });
    testInfo.annotations.push({ type: 'devtools-user-timing-events', description: String(trace.userTimingEvents) });
  });

  test('@pr-medium @browser-matrix explicit replacement is controlled and undoable in a medium graph', async ({ page }, testInfo) => {
    const fixture = 'test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    const visibleAtom = await getVisibleContractAtom(page);
    expect(visibleAtom).not.toBeNull();
    await page.getByTestId(`contract-node-${visibleAtom!.id}`).click();
    const type = page.getByTestId('contract-atom-type');
    await expect(type).toHaveText(/amplitude_clip_hard/);
    await expect(type.locator('input, select')).toHaveCount(0);

    await page.getByTestId('contract-atom-replace-open').click();
    await page.getByTestId('contract-atom-replace-type').selectOption('amplitude_clip_soft');
    await page.getByTestId('contract-atom-replace-preserve').check();
    await clearPerfSpans(page);
    await page.getByTestId('contract-atom-replace-confirm').click();
    await waitForSpanCount(page, 'contract.replace.atom', 1);
    await expect(type).toHaveText(/amplitude_clip_soft/);

    const replaceMs = await runAndAssertBudget(page, 'contract.replace.atom', 1);
    const replacementRenders = Object.keys(await getComponentRenders(page, 'ContractNode'));
    expect(replacementRenders).toEqual([`ContractNode:${visibleAtom!.id}`]);

    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await expect(type).toHaveText(/amplitude_clip_hard/);
    const undoMs = await runAndAssertBudget(page, 'ui.undo', 1);

    await clearPerfSpans(page);
    await page.getByTestId('topbar-redo').click();
    await expect(type).toHaveText(/amplitude_clip_soft/);
    const redoMs = await runAndAssertBudget(page, 'ui.redo', 1);

    const threshold = page.getByLabel(`${visibleAtom!.id} config threshold`);
    await clearPerfSpans(page);
    await threshold.fill('0.6');
    await waitForSpanCount(page, 'contract.edit.atom', 1);
    const editRenders = Object.keys(await getComponentRenders(page, 'ContractNode'));
    expect(editRenders).toEqual([`ContractNode:${visibleAtom!.id}`]);
    testInfo.annotations.push({ type: 'replacement-ms', description: replaceMs.toFixed(2) });
    testInfo.annotations.push({ type: 'replacement-undo-ms', description: undoMs.toFixed(2) });
    testInfo.annotations.push({ type: 'replacement-redo-ms', description: redoMs.toFixed(2) });
  });

  test('inspector cycling and atom rename stay isolated', async ({ page }, testInfo) => {
    const fixture = 'test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml';
    const meta = readPerfFixtureMeta(fixture);
    await openContractFixture(page, fixture, meta.atoms);

    const visibleAtom = await getVisibleContractAtom(page);
    expect(visibleAtom).not.toBeNull();
    await page.getByTestId(`contract-node-${visibleAtom!.id}`).click();
    const panel = page.getByTestId('contract-selected-atom-panel');
    await expect(panel).toBeVisible();

    await clearPerfSpans(page);
    const toggleAverageMs = await panel.evaluate((details: HTMLDetailsElement) => {
      const summary = details.querySelector('summary');
      if (!summary) throw new Error('Selected atom summary is missing.');
      const startedAt = performance.now();
      for (let index = 0; index < 100; index += 1) {
        summary.click();
        summary.click();
      }
      return (performance.now() - startedAt) / 200;
    });
    assertDurationBudget('ui.inspector.toggle', toggleAverageMs);
    await expect(panel).toHaveJSProperty('open', true);
    expect(Object.keys(await getComponentRenders(page, 'ContractNode'))).toHaveLength(0);

    const renamedId = 'renamed_atom';
    await clearPerfSpans(page);
    await page.getByTestId('contract-atom-id').fill(renamedId);
    await waitForSpanCount(page, 'contract.edit.atom', 1);
    await expect(page.getByTestId(`contract-node-${renamedId}`)).toBeVisible();
    await expect(page.getByTestId(`contract-node-${visibleAtom!.id}`)).toHaveCount(0);
    const renameMs = await runAndAssertBudget(page, 'contract.edit.atom', 1);
    expect(Object.keys(await getComponentRenders(page, 'ContractNode'))).toEqual([`ContractNode:${renamedId}`]);

    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await expect(page.getByTestId(`contract-node-${visibleAtom!.id}`)).toBeVisible();
    await runAndAssertBudget(page, 'ui.undo', 1);
    await page.getByTestId('topbar-redo').click();
    await expect(page.getByTestId(`contract-node-${renamedId}`)).toBeVisible();
    await runAndAssertBudget(page, 'ui.redo', 1);
    testInfo.annotations.push({ type: 'inspector-toggle-average-ms', description: toggleAverageMs.toFixed(3) });
    testInfo.annotations.push({ type: 'atom-rename-ms', description: renameMs.toFixed(2) });
  });

  test('raw YAML metadata edit does not rerender contract nodes or edges', async ({ page }) => {
    await page.getByTestId('project-node-drive1').dblclick();
    await expect(page.getByTestId('contract-canvas')).toBeVisible();
    const diagnostics = page.locator('details.developer-diagnostics');
    await diagnostics.locator(':scope > summary').click();
    const editor = page.getByLabel(/^Workspace file .*overdrive\.unit\.v2\.yaml$/);
    const original = await editor.inputValue();
    const updated = original.replace(
      'description: Boosts mono guitar input, soft clips it, shapes tone, and applies output level.',
      'description: Boosts mono guitar input, soft clips it, shapes tone, and applies output level efficiently.',
    );
    expect(updated).not.toBe(original);

    await clearPerfSpans(page);
    await editor.fill(updated);
    await waitForSpanCount(page, 'workspace.update.raw', 1);
    await page.waitForTimeout(400);

    expect((await getPerfCounters(page))['state.workspace.dispatches']).toBe(1);
    expect(await getComponentRenders(page, 'ContractNode')).toEqual({});
    expect(await getComponentRenders(page, 'ContractEdge')).toEqual({});
  });

  test('editing one parameter does not rerender unrelated project nodes', async ({ page }, testInfo) => {
    await page.getByTestId('project-node-drive1').click();
    await page.getByTestId('inspector-tab-atom').click();
    const knob = page.getByTestId('param-knob-drive1-drive');
    const box = await knob.boundingBox();
    expect(box).not.toBeNull();

    await clearPerfSpans(page);
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 - 6);
    await page.mouse.up();
    await waitForSpanCount(page, 'param.update', 1);
    await expect.poll(async () => Object.keys(await getComponentRenders(page, 'ProjectNode')).length).toBeGreaterThan(0);

    const renders = await getComponentRenders(page, 'ProjectNode');
    const renderedNodes = Object.keys(renders);
    testInfo.annotations.push({ type: 'project-node-renders', description: renderedNodes.join(',') });
    expect(renderedNodes).toEqual(['ProjectNode:drive1']);
  });
});

test.describe('Live WASM runtime performance', () => {
  test.beforeEach(async ({ page }) => {
    await launchWorkspace(page);
  });

  test('rapid parameter controls keep live audio healthy and release input handles on stop', async ({ page }, testInfo) => {
    const pageErrors: string[] = [];
    page.on('pageerror', error => pageErrors.push(error.message));
    await page.getByTestId('preview-mode-mic').click();
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.meter.valid ?? false, { timeout: 10_000 }).toBe(true);
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.meter.frames ?? 0, { timeout: 10_000 }).toBeGreaterThan(0);
    const baselineUnderruns = (await getRuntimeSnapshot(page))?.meter.underruns ?? 0;
    const baselineDeadlineMisses = (await getRuntimeSnapshot(page))?.meter.callbackDeadlineMisses ?? 0;

    await page.getByTestId('project-node-drive1').click();
    await page.getByTestId('inspector-tab-atom').click();
    const knob = page.getByTestId('param-knob-drive1-drive');
    const box = await knob.boundingBox();
    expect(box).not.toBeNull();
    await clearPerfSpans(page);
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2);
    await page.mouse.down();
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 - 40, { steps: 12 });
    await page.mouse.move(box!.x + box!.width / 2, box!.y + box!.height / 2 + 24, { steps: 12 });
    await page.mouse.up();

    const controlMs = await runAndAssertBudget(page, 'runtime.control.param', 1);
    const runningSnapshot = await getRuntimeSnapshot(page);
    expect(runningSnapshot?.meter.underruns).toBe(baselineUnderruns);
    expect(runningSnapshot?.meter.callbackDeadlineMisses).toBe(baselineDeadlineMisses);
    expect(runningSnapshot?.resources.workletActive).toBe(true);
    expect(runningSnapshot?.resources.streamTracks).toBe(1);

    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('ready');
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.resources.workletActive ?? true).toBe(false);
    const stoppedSnapshot = await getRuntimeSnapshot(page);
    expect(stoppedSnapshot?.resources.workerActive).toBe(true);
    expect(stoppedSnapshot?.resources.pendingControlRequests).toBe(0);
    expect(stoppedSnapshot?.resources.pendingProcessorRequests).toBe(0);
    expect(stoppedSnapshot?.resources.streamTracks).toBe(0);
    expect(stoppedSnapshot?.resources.inputNodeActive).toBe(false);
    expect(stoppedSnapshot?.resources.fileSourceActive).toBe(false);
    expect(stoppedSnapshot?.resources.workletStarts).toBe(stoppedSnapshot?.resources.workletStops);
    expect(pageErrors).toEqual([]);
    testInfo.annotations.push({ type: 'runtime-param-control-ms', description: controlMs.toFixed(2) });
  });

  test('microphone profiler attributes callback cost and releases polling on stop', async ({ page }, testInfo) => {
    const pageErrors: string[] = [];
    page.on('pageerror', error => pageErrors.push(error.message));
    await page.getByTestId('preview-mode-mic').click();
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.meter.valid ?? false, { timeout: 10_000 }).toBe(true);
    const baseline = await getRuntimeSnapshot(page);

    await page.getByTestId('inspector-tab-contract').click();
    const diagnostics = page.locator('details.developer-diagnostics');
    await diagnostics.locator(':scope > summary').click();
    await page.getByTestId('audio-trace-profile').click();
    await expect(page.getByTestId('audio-trace-status')).toHaveText('running');
    await expect(page.getByTestId('audio-trace-report')).toBeVisible({ timeout: 10_000 });
    await expect(page.getByTestId('audio-trace-status')).toHaveText('complete');

    const profiled = await getRuntimeSnapshot(page);
    expect(profiled?.meter.underruns).toBe(baseline?.meter.underruns);
    expect(profiled?.meter.callbackDeadlineMisses).toBe(baseline?.meter.callbackDeadlineMisses);
    expect(profiled?.resources.audioTracePollingActive).toBe(false);
    await expect(page.getByTestId('audio-trace-report')).toContainText('Sample rate');
    await expect(page.getByTestId('audio-trace-report')).toContainText('WASM graph');
    await expect(page.getByTestId('audio-trace-report')).toContainText('Deadline misses');
    await page.getByTestId('audio-trace-report').scrollIntoViewIfNeeded();
    await page.screenshot({ path: testInfo.outputPath('audio-trace-report.png'), fullPage: true });

    const downloadPromise = page.waitForEvent('download');
    await page.getByTestId('audio-trace-export').click();
    const download = await downloadPromise;
    const downloadPath = await download.path();
    expect(downloadPath).not.toBeNull();
    const report = JSON.parse(readFileSync(downloadPath!, 'utf8')) as {
      schema: string;
      browser: Record<string, number | null>;
      trace: { status: string; sampleCount: number; stages: Record<string, { p95Ms: number }> };
    };
    expect(report.schema).toBe('apg.audio-trace.v1');
    expect(report.trace.status).toBe('complete');
    expect(report.trace.sampleCount).toBeGreaterThan(0);
    expect(report.trace.stages.wasmProcess.p95Ms).toBeGreaterThanOrEqual(0);
    expect(report.browser).toHaveProperty('captureLatencyMs');

    await page.getByTestId('audio-trace-profile').click();
    await expect(page.getByTestId('audio-trace-status')).toHaveText('running');
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.resources.audioTracePollingActive ?? false).toBe(true);
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('ready');
    await expect(page.getByTestId('audio-trace-status')).toHaveText('idle');
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.resources.audioTracePollingActive ?? true).toBe(false);
    const stopped = await getRuntimeSnapshot(page);
    expect(stopped?.resources.streamTracks).toBe(0);
    expect(stopped?.resources.workletActive).toBe(false);
    expect(stopped?.resources.pendingProcessorRequests).toBe(0);
    expect(pageErrors).toEqual([]);
    testInfo.annotations.push({ type: 'audio-trace-samples', description: String(report.trace.sampleCount) });
  });

  test('structural edits hot-swap while live audio remains healthy', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'The full live structural workflow runs in scheduled performance CI.');
    const pageErrors: string[] = [];
    page.on('pageerror', error => pageErrors.push(error.message));
    await page.getByTestId('preview-mode-mic').click();
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
    await expect.poll(async () => (await getRuntimeSnapshot(page))?.meter.valid ?? false, { timeout: 10_000 }).toBe(true);
    const baselineUnderruns = (await getRuntimeSnapshot(page))?.meter.underruns ?? 0;
    const baselineDeadlineMisses = (await getRuntimeSnapshot(page))?.meter.callbackDeadlineMisses ?? 0;
    const swaps: Array<{ action: string; prepareMs: number; commitMs: number }> = [];
    const waitForHotSwap = async (action: string) => {
      const prepareMs = await runAndAssertBudget(page, 'runtime.prepare.workspace', 1);
      const commitMs = await runAndAssertBudget(page, 'runtime.commit.workspace', 1);
      await expect(page.locator('.transport-state')).toHaveText('running');
      await expect.poll(async () => {
        const snapshot = await getRuntimeSnapshot(page);
        return snapshot?.activeRevision === snapshot?.preparedRevision && (snapshot?.activeRevision ?? 0) > 0;
      }).toBe(true);
      swaps.push({ action, prepareMs, commitMs });
    };

    await clearPerfSpans(page);
    await dispatchProjectEdgeDrop(page, 'route-2-unit-drive1-unit-tone1', 'tone_stack_unit');
    await expect(page.getByTestId('project-node-tone_stack')).toBeVisible();
    await waitForHotSwap('insert-unit');

    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await waitForHotSwap('undo-unit');
    await clearPerfSpans(page);
    await page.getByTestId('topbar-redo').click();
    await waitForHotSwap('redo-unit');

    await page.getByTestId('project-node-drive1').dblclick();
    await expect(page.getByTestId('contract-canvas')).toBeVisible();
    const edgeId = 'contract-edge-contract-apply_drive-contract-clip_drive-signal';
    await clearPerfSpans(page);
    await dispatchContractEdgeDrop(page, edgeId, 'amplitude_clip_hard');
    await expect(page.getByTestId('contract-node-amplitude_clip_hard')).toBeVisible();
    await waitForHotSwap('insert-atom');

    await page.getByTestId('contract-atom-item-amplitude_clip_hard').click();
    await page.getByTestId('contract-atom-replace-open').click();
    await page.getByTestId('contract-atom-replace-type').selectOption('amplitude_clip_soft');
    await page.getByTestId('contract-atom-replace-preserve').check();
    await clearPerfSpans(page);
    await page.getByTestId('contract-atom-replace-confirm').click();
    await waitForHotSwap('replace-atom');

    await page.getByTestId('contract-atom-item-clip_drive').click();
    await clearPerfSpans(page);
    await page.getByLabel('clip_drive in signal').fill('driven');
    await waitForHotSwap('reconnect-input');
    await clearPerfSpans(page);
    await page.getByTestId('topbar-undo').click();
    await waitForHotSwap('undo-reconnect');
    await clearPerfSpans(page);
    await page.getByTestId('topbar-redo').click();
    await waitForHotSwap('redo-reconnect');

    await page.keyboard.press('Control+s');
    await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();
    await clearPerfSpans(page);
    await importPerfWorkspaceFixture(page, 'test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml', 1);
    await waitForHotSwap('import-100-atoms');

    const finalSnapshot = await getRuntimeSnapshot(page);
    expect(finalSnapshot?.meter.underruns).toBe(baselineUnderruns);
    expect(finalSnapshot?.meter.callbackDeadlineMisses).toBe(baselineDeadlineMisses);
    expect(finalSnapshot?.resources.workletActive).toBe(true);
    expect(pageErrors).toEqual([]);
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('ready');
    testInfo.annotations.push({ type: 'live-hot-swaps', description: JSON.stringify(swaps) });
  });

  test('repeated file and microphone starts release transient audio resources', async ({ page }, testInfo) => {
    test.skip(process.env.APG_PERF_SCHEDULED !== '1', 'Repeated audio resource checks run in scheduled performance CI.');
    const pageErrors: string[] = [];
    page.on('pageerror', error => pageErrors.push(error.message));
    const initial = await getRuntimeSnapshot(page);
    const initialStarts = initial?.resources.workletStarts ?? 0;
    const initialStops = initial?.resources.workletStops ?? 0;
    const modes = ['file', 'mic'] as const;

    for (let cycle = 0; cycle < 20; cycle += 1) {
      const mode = modes[cycle % modes.length];
      await page.getByTestId(`preview-mode-${mode}`).click();
      await page.getByTestId('preview-start-stop').click();
      await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
      await expect.poll(async () => {
        const snapshot = await getRuntimeSnapshot(page);
        return Boolean(
          snapshot?.resources.workletActive
          && snapshot.resources.meterTimerActive
          && snapshot.resources.latencyTimerActive,
        );
      }).toBe(true);

      await page.getByTestId('preview-start-stop').click();
      await expect(page.locator('.transport-state')).toHaveText('ready');
      await expect.poll(async () => {
        const snapshot = await getRuntimeSnapshot(page);
        if (!snapshot) return false;
        const resources = snapshot.resources;
        return !resources.workletActive
          && resources.pendingControlRequests === 0
          && resources.pendingProcessorRequests === 0
          && resources.streamTracks === 0
          && !resources.inputNodeActive
          && !resources.fileSourceActive
          && !resources.meterTimerActive
          && !resources.latencyTimerActive
          && resources.workletStarts === resources.workletStops;
      }).toBe(true);
    }

    const final = await getRuntimeSnapshot(page);
    expect(final?.resources.workerActive).toBe(true);
    expect(final?.resources.contextState).toBe('running');
    expect(final?.resources.workletStarts).toBe(initialStarts + 20);
    expect(final?.resources.workletStops).toBe(initialStops + 20);
    expect(pageErrors).toEqual([]);
    testInfo.annotations.push({ type: 'audio-start-stop-cycles', description: '20' });
  });
});
