import { readFileSync } from 'node:fs';
import path from 'node:path';
import { expect, type Page, test } from '@playwright/test';

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

function getThreshold(name: string): number | null {
  return thresholds.maxMs[name] ?? null;
}

function clearPerfSpans(page: Page) {
  return page.evaluate(() => {
    const trace = (window as { __apgPerfTrace?: { samples?: unknown[]; renderSamples?: unknown[] } }).__apgPerfTrace;
    if (!trace) return;
    trace.samples = [];
    trace.renderSamples = [];
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

async function runAndAssertBudget(page: Page, name: string, minimumSamples = 1) {
  const budgetMs = getThreshold(name);
  expect(budgetMs).toBeTruthy();
  await waitForSpanCount(page, name, minimumSamples);
  const samples = await getSpans(page, name);
  const latest = samples.slice(-minimumSamples);
  const maxMs = Math.max(...latest.map(sample => sample.durationMs));
  expect(maxMs).toBeLessThanOrEqual(budgetMs ?? Number.POSITIVE_INFINITY);
  return maxMs;
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
  return page.locator('[data-testid^="project-node-"]').count();
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
