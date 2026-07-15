import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { expect, type Page, test } from '@playwright/test';
import { load as parseYaml } from 'js-yaml';

type PerfSample = {
  name: string;
  durationMs: number;
  startedAt: number;
  endedAt: number;
};

type PerfThresholdMap = {
  maxMs: Record<string, number>;
};

const thresholds: PerfThresholdMap = JSON.parse(
  readFileSync(path.resolve(process.cwd(), 'scripts', 'perf-ui-thresholds.json'), 'utf8'),
);

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
      };
    }).__apgPerfTrace;
    if (!trace) return;
    trace.samples = [];
    trace.renderSamples = [];
    trace.componentRenders = {};
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

async function importPerfWorkspaceFixture(page: Page, profilePath: string, expectedNodes: number): Promise<number> {
  const payload = buildPerfWorkspacePayload(profilePath);
  const startedAt = await page.evaluate(() => performance.now());
  await page.getByTestId('topbar-import-input').setInputFiles({
    name: 'perf-workspace.json',
    mimeType: 'application/json',
    buffer: Buffer.from(payload),
  });
  await expect.poll(() => countProjectNodes(page), { timeout: 20_000 }).toBe(expectedNodes);
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

test.describe('UI performance checkpoints', () => {
  test.beforeEach(async ({ page }) => {
    await launchWorkspace(page);
  });

  test('drag-and-drop add on project graph', async ({ page }) => {
    await clearPerfSpans(page);

    const before = await countProjectNodes(page);
    await page.getByTestId('project-unit-item-overdrive_unit').dragTo(page.getByTestId('project-canvas'), {
      targetPosition: { x: 280, y: 220 },
    });

    await expect.poll(() => countProjectNodes(page), { timeout: 5000 }).toBeGreaterThan(before);

    await runAndAssertBudget(page, 'ui.dragStart.projectUnit');
    await runAndAssertBudget(page, 'ui.dragOver.projectNode');
    await runAndAssertBudget(page, 'ui.drop.projectNode');
    await runAndAssertBudget(page, 'graph.add.projectNode');
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

    await page.getByTestId('inspector-tab-atom').click();
    const latestRoute = page.getByTestId(/^route-item-/).last();
    await latestRoute.click();

    await clearPerfSpans(page);
    await page.getByTestId('inspector-route-disconnect').click();
    await expect.poll(() => page.getByTestId(/^route-item-/).count(), { timeout: 5000 }).toBe(beforeCount);
    await runAndAssertBudget(page, 'graph.delete.route');
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
    test(`graph load and synchronize ${fixture.profile}`, async ({ page }, testInfo) => {
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
});

test.describe('Contract graph atom scalability', () => {
  test.beforeEach(async ({ page }) => {
    await launchWorkspace(page);
  });

  const fixtures = [
    { profile: 'small-atoms', bucket: 'small', path: 'test/fixtures/projects-v2/perf/small-atoms.project.v2.yaml' },
    { profile: 'medium-atoms', bucket: 'medium', path: 'test/fixtures/projects-v2/perf/medium-atoms.project.v2.yaml' },
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
  });

  test('explicit replacement is controlled and undoable in a medium graph', async ({ page }, testInfo) => {
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
