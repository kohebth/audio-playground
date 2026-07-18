import { expect, type Page, type TestInfo, test } from '@playwright/test';

const pagesPath = '/audio-playground/';

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
    if (url.origin === origin && response.status() >= 400) {
      audit.badResponses.push(`${response.status()} ${url.pathname}`);
    }
  });
  return audit;
}

async function openWorkspace(page: Page, testInfo: TestInfo, route = '/projects') {
  const url = new URL(`#${route}`, baseUrl(testInfo));
  await page.goto(url.href);
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
}

function expectHealthyNetwork(audit: NetworkAudit) {
  expect(audit.failedRequests).toEqual([]);
  expect(audit.badResponses).toEqual([]);
  expect(audit.sameOriginPaths.length).toBeGreaterThan(0);
  for (const path of audit.sameOriginPaths) expect(path.startsWith(pagesPath)).toBe(true);
}

test('serves base-safe project and unit routes with release diagnostics', async ({ page }, testInfo) => {
  const audit = monitorNetwork(page, baseUrl(testInfo));
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page, testInfo);
  await expect(page).toHaveURL(/\/audio-playground\/#\/projects$/);
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  const toneNode = page.getByTestId('project-node-tone1');
  await expect(toneNode).toBeVisible();
  for (const control of ['gain', 'bass', 'mid', 'treble', 'presence', 'volume']) {
    await expect(toneNode.getByTestId(`param-knob-tone1-${control}`)).toBeVisible();
  }
  await page.getByTestId('inspector-tab-contract').click();
  const diagnostics = page.locator('details.developer-diagnostics');
  await diagnostics.locator(':scope > summary').click();
  await expect(page.getByTestId('build-base-path')).toHaveText(pagesPath);
  const expectedCommit = process.env.APG_EXPECTED_COMMIT_SHA;
  if (expectedCommit) await expect(page.getByTestId('build-commit-sha')).toHaveText(expectedCommit);

  const example = await page.evaluate(async () => {
    const response = await fetch(new URL('units/overdrive.unit.v2.yaml', document.baseURI));
    return { ok: response.ok, text: await response.text(), url: response.url };
  });
  expect(example.ok).toBe(true);
  expect(new URL(example.url).pathname).toBe(`${pagesPath}units/overdrive.unit.v2.yaml`);
  expect(example.text).toContain('schema: apg.unit.v2');
  expect(example.text).toContain('wasm_realtime: true');

  await openWorkspace(page, testInfo, '/unit/tone_stack');
  await expect(page).toHaveURL(/\/audio-playground\/#\/unit\/tone_stack$/);
  const contractCanvas = page.getByTestId('contract-canvas');
  await expect(contractCanvas).toBeVisible();
  await expect(contractCanvas).toHaveAttribute('data-atom-count', '21');
  await expect(contractCanvas).toHaveAttribute('data-boundary-count', '2');
  await expect(page.getByTestId('contract-node-mid_bandpass')).toContainText('filter_biquad');
  await expect(page.getByRole('button', { name: 'preamp_saturation amplitude_clip_soft' })).toHaveCount(1);
  await expect(page.getByRole('button', { name: 'power_saturation amplitude_clip_soft' })).toHaveCount(1);

  // Keep the boundary-node sizing check on a compact graph: the larger tone
  // stack intentionally virtualizes its far-edge nodes at the minimum zoom.
  await openWorkspace(page, testInfo, '/unit/overdrive');
  await expect(page).toHaveURL(/\/audio-playground\/#\/unit\/overdrive$/);
  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const inputBoundary = page.getByTestId('unit-boundary-input');
  const outputBoundary = page.getByTestId('unit-boundary-output');
  await expect(inputBoundary).toBeVisible();
  await expect(inputBoundary).toHaveText('input');
  await expect(inputBoundary).toHaveCSS('width', '10px');
  await expect(inputBoundary).toHaveCSS('height', '10px');
  await expect(outputBoundary).toBeVisible();
  await expect(outputBoundary).toHaveText('output');
  await expect(outputBoundary).toHaveCSS('width', '10px');
  await expect(outputBoundary).toHaveCSS('height', '10px');
  await expect(page.locator('.react-flow__edge.contract-edge--boundary-input')).toHaveCount(1);
  await expect(page.locator('.react-flow__edge.contract-edge--boundary-output')).toHaveCount(1);
  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(contractCanvas).toBeVisible();
  await expect(inputBoundary).toBeVisible();
  await expect(outputBoundary).toBeVisible();
  await expect(page.getByTestId('atom-palette-browser-hidden')).toContainText('3 browser-incompatible hidden');
  await page.getByTestId('atom-palette-show-advanced').check();
  await expect(page.getByTestId('atom-palette-item-freq_fft')).toHaveCount(0);

  expect(pageErrors).toEqual([]);
  expectHealthyNetwork(audit);
});

test('persists drag-and-drop edits and exports the workspace', async ({ page }, testInfo) => {
  await openWorkspace(page, testInfo);
  const unitNodes = page.locator('.react-flow__node[data-id^="unit-"]');
  const initialCount = await unitNodes.count();
  await page.getByTestId('project-unit-item-overdrive_unit').dragTo(page.getByTestId('project-canvas'), {
    targetPosition: { x: 480, y: 280 },
  });
  await expect(unitNodes).toHaveCount(initialCount + 1);
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();

  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(unitNodes).toHaveCount(initialCount + 1);
  await expect(page.getByTestId('topbar-import-input')).toHaveCount(1);

  const downloadPromise = page.waitForEvent('download');
  await page.getByTestId('topbar-export').click();
  const download = await downloadPromise;
  expect(download.suggestedFilename()).toBe('audio-playground-workspace.json');
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
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  await openWorkspace(page, testInfo);

  for (const mode of ['file', 'mic', 'file', 'mic'] as const) {
    await page.getByTestId(`preview-mode-${mode}`).click();
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
    await page.getByTestId('preview-start-stop').click();
    await expect(page.locator('.transport-state')).toHaveText('ready');
  }

  const lifecycle = await page.evaluate(() => (
    window as typeof window & { __apgPagesAudioStats: AudioLifecycleStats }
  ).__apgPagesAudioStats);
  const processorLoads = audit.sameOriginPaths.filter(path => path.endsWith('/wasm/apg_processor.wasm')).length;
  expect(processorLoads).toBe(4);
  expect(lifecycle.workletDisconnects).toBe(4);
  expect(lifecycle.mediaTrackStops).toBeGreaterThanOrEqual(2);
  expect(pageErrors).toEqual([]);
  expectHealthyNetwork(audit);
});

test('shows microphone permission failure without crashing the editor', async ({ page }, testInfo) => {
  await openWorkspace(page, testInfo);
  await expect(page.locator('.transport-state')).toHaveText(/idle|ready/, { timeout: 20_000 });
  await page.getByTestId('preview-compile').click();
  await expect(page.locator('.transport-state')).toHaveText('ready', { timeout: 20_000 });
  await page.evaluate(() => {
    Object.defineProperty(navigator.mediaDevices, 'getUserMedia', {
      configurable: true,
      value: () => Promise.reject(new DOMException('Injected microphone permission denial', 'NotAllowedError')),
    });
  });

  await page.getByTestId('preview-mode-mic').click();
  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-state')).toHaveText('error');
  await expect(page.locator('.transport-state')).toHaveAttribute(
    'aria-label',
    /Injected microphone permission denial/,
  );
  await expect(page.getByTestId('project-canvas')).toBeVisible();
});

test('contains and recovers from an invalid DSP edit', async ({ page }, testInfo) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  await openWorkspace(page, testInfo, '/unit/overdrive');
  const diagnostics = page.locator('details.developer-diagnostics');
  await diagnostics.locator(':scope > summary').click();
  const editor = page.getByLabel(/^Workspace file .*overdrive\.unit\.v2\.yaml$/);
  const original = await editor.inputValue();
  const invalid = original.replace('atom: generation_dc', 'atom: missing_browser_atom');
  expect(invalid).not.toBe(original);

  await editor.fill(invalid);
  await expect(page.locator('.transport-state')).toHaveText('error', { timeout: 20_000 });
  await expect(page.getByTestId('contract-canvas')).toBeVisible();

  await editor.fill(original);
  await expect(page.locator('.transport-state')).toHaveText(/idle|ready/, { timeout: 20_000 });
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  expect(pageErrors).toEqual([]);
});
