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
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  await expect.poll(() => page.locator('.contract-node').count(), { timeout: 30_000 }).toBe(expectedAtoms);
  return page.evaluate(start => performance.now() - start, startedAt);
}

function median(values: number[]): number {
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.floor(sorted.length / 2)];
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
    const selected = page.getByTestId('contract-node-atom_0050');
    await selected.click();
    await expect(selected).toHaveClass(/contract-node--selected/);
    await expect.poll(async () => Object.keys(await getComponentRenders(page, 'ContractNode')).length).toBeGreaterThan(0);

    const renders = await getComponentRenders(page, 'ContractNode');
    const renderedNodes = Object.keys(renders);
    testInfo.annotations.push({ type: 'contract-node-renders', description: renderedNodes.join(',') });
    expect(renderedNodes).toEqual(['ContractNode:atom_0050']);
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
