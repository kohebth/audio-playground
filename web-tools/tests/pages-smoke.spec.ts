import { expect, type Page, type TestInfo, test } from '@playwright/test';

type NetworkAudit = {
  badResponses: string[];
  failedRequests: string[];
  sameOriginPaths: string[];
};

type AudioLifecycleStats = {
  mediaTrackStops: number;
  workletDisconnects: number;
};

function baseUrl(testInfo: TestInfo): string {
  const value = testInfo.project.use.baseURL;
  if (typeof value !== 'string') throw new Error('Pages smoke baseURL is not configured');
  return value;
}

function deployedBasePath(testInfo: TestInfo): string {
  const path = new URL(baseUrl(testInfo)).pathname;
  return path.endsWith('/') ? path : `${path}/`;
}

function monitorNetwork(page: Page, configuredBaseUrl: string): NetworkAudit {
  const audit: NetworkAudit = { badResponses: [], failedRequests: [], sameOriginPaths: [] };
  const origin = new URL(configuredBaseUrl).origin;
  page.on('request', request => {
    const url = new URL(request.url());
    if (url.origin === origin) audit.sameOriginPaths.push(url.pathname);
  });
  page.on('requestfailed', request => {
    const url = new URL(request.url());
    if (url.origin === origin) audit.failedRequests.push(`${request.failure()?.errorText ?? 'failed'} ${url.pathname}`);
  });
  page.on('response', response => {
    const url = new URL(response.url());
    if (url.origin === origin && response.status() >= 400) audit.badResponses.push(`${response.status()} ${url.pathname}`);
  });
  return audit;
}

function expectHealthyNetwork(audit: NetworkAudit, expectedBasePath: string) {
  expect(audit.failedRequests).toEqual([]);
  expect(audit.badResponses).toEqual([]);
  expect(audit.sameOriginPaths.length).toBeGreaterThan(0);
  for (const path of audit.sameOriginPaths) expect(path.startsWith(expectedBasePath)).toBe(true);
}

async function openWorkspace(page: Page, testInfo: TestInfo) {
  await page.addInitScript(() => localStorage.setItem('apg.studio.mode.v1', 'effect-chain'));
  await page.goto(new URL('#/projects', baseUrl(testInfo)).href);
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tourClose = page.getByRole('button', { name: 'Close tour' });
  if (await tourClose.isVisible()) await tourClose.click();
  await expect(page.getByTestId('effect-chain-canvas')).toBeVisible();
}

async function openFirstAtomChain(page: Page) {
  const effect = page.locator('.effect-library-card').first();
  await effect.click({ button: 'right' });
  await page.getByRole('menuitem', { name: 'Edit Atom Chain' }).click();
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
}

test('serves base-safe Effect Chain and Graphviz Atom Chain views', async ({ page }, testInfo) => {
  const audit = monitorNetwork(page, baseUrl(testInfo));
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  await openWorkspace(page, testInfo);

  await expect(page.getByRole('button', { name: 'Effect Chain', exact: true })).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByRole('button', { name: 'Atom Chain', exact: true })).toBeVisible();
  await expect(page.locator('.effect-chain-card')).not.toHaveCount(0);
  await expect(page.locator('.react-flow__node-projectNode')).toHaveCount(0);

  const example = await page.evaluate(async () => {
    const response = await fetch(new URL('units/overdrive.unit.v2.yaml', document.baseURI));
    return { ok: response.ok, text: await response.text(), url: response.url };
  });
  expect(example.ok).toBe(true);
  expect(new URL(example.url).pathname).toBe(`${deployedBasePath(testInfo)}units/overdrive.unit.v2.yaml`);
  expect(example.text).toContain('schema: apg.unit.v2');

  await openFirstAtomChain(page);
  await expect(page.getByTestId('atom-context-inspector')).toBeVisible();
  await page.getByRole('button', { name: 'Auto Layout' }).click();
  await expect(page.locator('.contract-layout-error')).toHaveCount(0, { timeout: 20_000 });
  await expect(page.locator('.react-flow__edge')).not.toHaveCount(0);
  await expect(page.getByTestId('atom-context-inspector')).not.toContainText('Unit Contract');

  expect(pageErrors).toEqual([]);
  expectHealthyNetwork(audit, deployedBasePath(testInfo));
});

test('keeps missing routing helpers repairable and gates export', async ({ page }, testInfo) => {
  await openWorkspace(page, testInfo);
  await page.locator('.effect-library-card').first().getByRole('button', { name: /in parallel$/ }).click();
  const parallel = page.locator('.effect-chain-parallel').last();
  await expect(parallel).toBeVisible();

  await parallel.locator('.effect-chain-helper').first().getByRole('button', { name: 'Open panner actions' }).click();
  await page.getByRole('menuitem', { name: 'Remove panner' }).click();
  await expect(parallel.locator('.effect-chain-helper--missing')).toContainText('Restore panner');
  await expect(page.getByTestId('topbar-export')).toBeDisabled();
  await expect(page.getByTestId('preview-compile')).toBeDisabled();

  await parallel.getByRole('button', { name: /Restore panner/ }).click();
  await expect(parallel.locator('.effect-chain-helper--missing')).toHaveCount(0);
  await expect(page.getByTestId('topbar-export')).toBeEnabled();
  const downloadPromise = page.waitForEvent('download');
  await page.getByTestId('topbar-export').click();
  expect((await downloadPromise).suggestedFilename()).toMatch(/\.apg$/);

  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.locator('.effect-chain-parallel')).not.toHaveCount(0);
});

test('registers the AudioWorklet and releases repeated audio resources', async ({ page }, testInfo) => {
  await page.addInitScript(() => {
    const stats: AudioLifecycleStats = { mediaTrackStops: 0, workletDisconnects: 0 };
    Object.defineProperty(window, '__apgPagesAudioStats', { configurable: true, value: stats });
    const originalDisconnect = AudioWorkletNode.prototype.disconnect;
    AudioWorkletNode.prototype.disconnect = function disconnect(...args: Parameters<AudioWorkletNode['disconnect']>) {
      stats.workletDisconnects += 1;
      return originalDisconnect.apply(this, args);
    } as AudioWorkletNode['disconnect'];
    const originalTrackStop = MediaStreamTrack.prototype.stop;
    MediaStreamTrack.prototype.stop = function stop() {
      stats.mediaTrackStops += 1;
      return originalTrackStop.call(this);
    };
  });
  const audit = monitorNetwork(page, baseUrl(testInfo));
  await openWorkspace(page, testInfo);

  for (const mode of ['mic', 'file'] as const) {
    if (mode === 'file') await openFirstAtomChain(page);
    await page.getByTestId(`preview-mode-${mode}`).click({ timeout: 10_000 });
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('ready');
  }

  const lifecycle = await page.evaluate(() => (
    window as typeof window & { __apgPagesAudioStats: AudioLifecycleStats }
  ).__apgPagesAudioStats);
  expect(lifecycle.workletDisconnects).toBe(2);
  expect(lifecycle.mediaTrackStops).toBeGreaterThanOrEqual(1);
  expectHealthyNetwork(audit, deployedBasePath(testInfo));
});

test('contains invalid Unit Settings edits inside Atom Chain', async ({ page }, testInfo) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  await openWorkspace(page, testInfo);
  await openFirstAtomChain(page);
  await page.getByRole('button', { name: 'Unit Settings' }).click();
  const version = page.getByLabel('Unit version');
  await version.fill('not-a-version');
  await version.press('Tab');
  await expect(page.getByRole('alert')).toContainText('semantic versioning');
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  await version.fill('2.0.1');
  await version.press('Tab');
  await expect(page.getByRole('alert')).toHaveCount(0);
  expect(pageErrors).toEqual([]);
});
