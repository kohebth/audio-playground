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
    if (url.origin === origin && response.status() >= 400) {
      audit.badResponses.push(`${response.status()} ${url.pathname}`);
    }
  });
  return audit;
}

async function openWorkspace(page: Page, testInfo: TestInfo, route = '/projects') {
  await page.addInitScript(() => {
    localStorage.setItem('apg.studio.mode.v1', 'pro');
    localStorage.setItem('apg.studio.tour.v1', 'complete');
  });
  const url = new URL(`#${route}`, baseUrl(testInfo));
  await page.goto(url.href);
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
}

async function expectProjectNodeLocked(page: Page, testId: string) {
  const node = page.getByTestId(testId);
  const before = await node.boundingBox();
  expect(before).not.toBeNull();
  await node.click({ position: { x: 12, y: 12 } });
  await page.keyboard.press('ArrowRight');
  await page.mouse.move(before!.x + 12, before!.y + 12);
  await page.mouse.down();
  await page.mouse.move(before!.x + 112, before!.y + 72, { steps: 8 });
  await page.mouse.up();
  const after = await node.boundingBox();
  expect(after).not.toBeNull();
  expect(after!.x).toBeCloseTo(before!.x, 1);
  expect(after!.y).toBeCloseTo(before!.y, 1);
}

function expectHealthyNetwork(audit: NetworkAudit, expectedBasePath: string) {
  expect(audit.failedRequests).toEqual([]);
  expect(audit.badResponses).toEqual([]);
  expect(audit.sameOriginPaths.length).toBeGreaterThan(0);
  for (const path of audit.sameOriginPaths) expect(path.startsWith(expectedBasePath)).toBe(true);
}

test('serves base-safe Pipeline and Contract routes', async ({ page }, testInfo) => {
  const audit = monitorNetwork(page, baseUrl(testInfo));
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page, testInfo);
  await expect(page).toHaveURL(/#\/projects$/);
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  await expect(page.locator('.app')).toHaveClass(/app--simple/);
  const gateNode = page.getByTestId('project-node-gate1');
  await expect(gateNode).toBeVisible();
  await expectProjectNodeLocked(page, 'project-node-gate1');
  const gateBypass = gateNode.getByTestId('project-node-bypass-gate1');
  await expect(gateBypass).toBeEnabled();
  await expect(gateBypass).toHaveClass(/node-pedal-footer/);
  await expect(gateBypass).toHaveText('ON');
  await expect(gateBypass).toHaveAttribute('aria-pressed', 'true');
  const bypassHitArea = await gateBypass.evaluate(button => {
    const pedal = button.parentElement;
    if (!pedal) throw new Error('Bypass footer has no pedal container');
    return {
      footerWidth: button.offsetWidth,
      pedalWidth: pedal.clientWidth,
      minHeight: Number.parseFloat(getComputedStyle(button).minHeight),
    };
  });
  expect(bypassHitArea.footerWidth).toBe(bypassHitArea.pedalWidth);
  expect(bypassHitArea.minHeight).toBe(28);
  await expect(page.locator('.react-flow__edge-text')).toHaveCount(0);
  await gateBypass.click();
  await expect(gateBypass).toHaveText('OFF');
  await expect(gateBypass).toHaveAttribute('aria-pressed', 'false');
  await expect(gateNode).toHaveCSS('opacity', '0.5');
  await gateBypass.click();
  await expect(gateBypass).toHaveText('ON');
  await expect(gateNode).toHaveCSS('opacity', '1');
  for (const control of ['threshold', 'attack', 'release']) {
    await expect(gateNode.getByTestId(`param-knob-gate1-${control}`)).toBeVisible();
  }
  await expect(gateNode.locator('.knob-label')).toHaveText(['Threshold', 'Attack', 'Release']);
  const phaserNode = page.getByTestId('project-node-phaser1');
  await expect(phaserNode).toBeVisible();
  for (const control of ['rate', 'depth', 'center', 'feedback', 'mix']) {
    await expect(phaserNode.getByTestId(`param-knob-phaser1-${control}`)).toBeVisible();
  }
  await expect(phaserNode.locator('.knob-label')).toHaveText(['Rate', 'Depth', 'Center', 'Resonance', 'Mix']);
  const toneNode = page.getByTestId('project-node-tone1');
  await expect(toneNode).toBeVisible();
  for (const control of ['gain', 'bass', 'mid', 'treble', 'presence', 'volume']) {
    await expect(toneNode.getByTestId(`param-knob-tone1-${control}`)).toBeVisible();
  }
  const toneKnobLabels = toneNode.locator('.knob-label');
  await expect(toneKnobLabels).toHaveText([
    'Preamp Gain',
    'Bass',
    'Middle',
    'Treble',
    'Presence',
    'Master Volume',
  ]);
  const toneKnobRows = await toneNode.locator('.unit-knob').evaluateAll(knobs => {
    const rows = new Map<number, number>();
    for (const knob of knobs) {
      const top = Math.round(knob.getBoundingClientRect().top);
      rows.set(top, (rows.get(top) ?? 0) + 1);
    }
    return [...rows.values()];
  });
  expect(toneKnobRows).toEqual([3, 3]);
  const chorusNode = page.getByTestId('project-node-chorus1');
  await expect(chorusNode).toBeVisible();
  for (const control of ['rate', 'depth', 'mix']) {
    await expect(chorusNode.getByTestId(`param-knob-chorus1-${control}`)).toBeVisible();
  }
  await expect(chorusNode.locator('.knob-label')).toHaveText(['Rate', 'Depth', 'Mix']);
  const delayNode = page.getByTestId('project-node-delay1');
  await expect(delayNode).toBeVisible();
  for (const control of ['time_samples', 'feedback', 'mix']) {
    await expect(delayNode.getByTestId(`param-knob-delay1-${control}`)).toBeVisible();
  }
  await expect(delayNode.locator('.knob-label')).toHaveText(['Time', 'Feedback', 'Mix']);
  await expect(page.getByTestId('project-node-blend1')).toHaveCount(0);

  await page.getByTestId('view-effect-contract').click();
  await expect(page.getByTestId('contract-empty-state')).toBeVisible();
  await page.getByTestId('view-effect-pipeline').click();
  await expectProjectNodeLocked(page, 'project-node-gate1');
  await page.locator('.topbar__status .status-pill').first().click();
  await expect(page.getByTestId('build-base-path')).toHaveText(deployedBasePath(testInfo));
  const expectedCommit = process.env.APG_EXPECTED_COMMIT_SHA;
  if (expectedCommit) await expect(page.getByTestId('build-commit-sha')).toHaveText(expectedCommit);
  await page.getByLabel('Close readiness').click();

  const example = await page.evaluate(async () => {
    const response = await fetch(new URL('units/overdrive.unit.v2.yaml', document.baseURI));
    return { ok: response.ok, text: await response.text(), url: response.url };
  });
  expect(example.ok).toBe(true);
  expect(new URL(example.url).pathname).toBe(`${deployedBasePath(testInfo)}units/overdrive.unit.v2.yaml`);
  expect(example.text).toContain('schema: apg.unit.v2');
  expect(example.text).toContain('wasm_realtime: true');

  await page.getByTestId('project-node-tone1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'tone1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
  await expect(page).toHaveURL(/#\/unit\/tone_stack_copy$/);
  const contractCanvas = page.getByTestId('contract-canvas');
  await expect(contractCanvas).toBeVisible();
  await expect(contractCanvas).toHaveAttribute('data-atom-count', '21');
  await expect(contractCanvas).toHaveAttribute('data-boundary-count', '2');
  await expect(page.getByTestId('contract-node-mid_bandpass')).toContainText('filter_biquad');
  await page.getByRole('button', { name: 'Contract Settings' }).click();
  const contractParamNames = page.locator('.structured-param input[aria-label$=" name"]');
  await expect.poll(() => contractParamNames.evaluateAll(inputs => inputs.map(input => (input as HTMLInputElement).value)))
    .toEqual(['gain', 'bass', 'mid', 'treble', 'presence', 'volume']);
  await page.getByTestId('contract-param-volume-up').click();
  await expect.poll(() => contractParamNames.evaluateAll(inputs => inputs.map(input => (input as HTMLInputElement).value)))
    .toEqual(['gain', 'bass', 'mid', 'treble', 'volume', 'presence']);
  await page.getByLabel('Close Contract Settings').last().click();
  await page.getByTestId('view-effect-pipeline').click();
  await expect(page).toHaveURL(/#\/projects$/);
  await expect(toneKnobLabels).toHaveText([
    'Preamp Gain',
    'Bass',
    'Middle',
    'Treble',
    'Master Volume',
    'Presence',
  ]);

  // Keep the boundary-node sizing check on a compact graph: the larger tone
  // stack intentionally virtualizes its far-edge nodes at the minimum zoom.
  await page.getByTestId('project-node-drive1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
  await expect(page).toHaveURL(/#\/unit\/overdrive_copy$/);
  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page).toHaveURL(/#\/projects$/);
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  await page.getByTestId('project-node-drive1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
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
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  await page.getByTestId('project-node-drive1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  await expect(inputBoundary).toBeVisible();
  await expect(outputBoundary).toBeVisible();
  await expect(page.getByTestId('atom-palette-browser-hidden')).toContainText('3 browser-incompatible hidden');
  await page.getByTestId('atom-palette-show-advanced').check();
  await expect(page.getByTestId('atom-palette-item-freq_fft')).toHaveCount(0);

  expect(pageErrors).toEqual([]);
  expectHealthyNetwork(audit, deployedBasePath(testInfo));
});

test('persists locked-layout edits and exports the workspace', async ({ page }, testInfo) => {
  await openWorkspace(page, testInfo);
  const unitNodes = page.locator('.react-flow__node[data-id^="unit-"]');
  await expect(unitNodes).toHaveCount(8);
  const initialCount = await unitNodes.count();
  const viewport = page.locator('.react-flow__viewport');
  const initialTransform = await viewport.evaluate(element => getComputedStyle(element).transform);
  const initialStoredWorkspace = await page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'));
  await page.getByRole('button', { name: 'Add Overdrive', exact: true }).click();
  await expect(unitNodes).toHaveCount(initialCount + 1);
  await expect(viewport).toHaveCSS('transform', initialTransform);
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2')))
    .not.toBe(initialStoredWorkspace);
  const storedAfterDrop = await page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'));

  const simpleTransform = await viewport.evaluate(element => getComputedStyle(element).transform);
  await page.getByRole('button', { name: 'Add Chorus in parallel', exact: true }).click();
  await expect(unitNodes).toHaveCount(initialCount + 4);
  await expect(viewport).toHaveCSS('transform', simpleTransform);
  const routePaths = await page.locator('.react-flow__edge-path').evaluateAll(paths => (
    paths.map(path => path.getAttribute('d') ?? '')
  ));
  expect(routePaths.some(path => !path.includes('Q'))).toBe(true);
  expect(routePaths.every(path => !path.includes('C'))).toBe(true);
  const routingPaths = await page.locator(
    '.react-flow__edge[data-id*="unit-path_panner_2"], .react-flow__edge[data-id*="unit-path_mixer_2"]',
  ).locator('.react-flow__edge-path').evaluateAll(paths => paths.map(path => path.getAttribute('d') ?? ''));
  expect(routingPaths).toHaveLength(5);
  expect(routingPaths.every(path => !path.includes('Q') && !path.includes('C'))).toBe(true);
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2')))
    .not.toBe(storedAfterDrop);

  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(unitNodes).toHaveCount(initialCount + 4);
  await expect(page.getByTestId('topbar-import-input')).toHaveCount(1);

  await page.getByTestId('view-effect-contract').click();
  const downloadPromise = page.waitForEvent('download');
  await page.getByTestId('topbar-export').click();
  const download = await downloadPromise;
  expect(download.suggestedFilename()).toMatch(/\.apg$/);
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
  expectHealthyNetwork(audit, deployedBasePath(testInfo));
});

test('keeps Mic selected and explains how to recover on insecure LAN HTTP', async ({ page }, testInfo) => {
  await page.addInitScript(() => {
    Object.defineProperty(window, 'isSecureContext', {
      configurable: true,
      value: false,
    });
    Object.defineProperty(navigator, 'mediaDevices', {
      configurable: true,
      value: undefined,
    });
  });
  await openWorkspace(page, testInfo);

  const micMode = page.getByTestId('preview-mode-mic');
  const startStop = page.getByTestId('preview-start-stop');
  await expect(micMode).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByTestId('preview-compile')).toBeEnabled({ timeout: 20_000 });
  await expect(startStop).toBeDisabled();
  await expect(page.locator('.transport-state')).toHaveText('error');

  const errorBadge = page.getByTestId('preview-error-badge');
  const errorDetails = page.getByTestId('preview-error-details');
  await errorBadge.hover();
  await expect(errorDetails).toBeVisible();
  await expect(errorDetails).toContainText('Microphone unavailable');
  await expect(errorDetails).toContainText('APG_WEB_MIC_INSECURE_CONTEXT');
  await expect(errorDetails).toContainText(new URL(baseUrl(testInfo)).origin);
  await expect(errorDetails).toContainText('trusted HTTPS');
  await expect(errorDetails).toContainText('npm run dev:https');

  const issueBanner = page.getByTestId('project-issue-banner');
  await expect(issueBanner).toContainText('APG_WEB_MIC_INSECURE_CONTEXT');
  await issueBanner.getByRole('button', { name: 'Audio I/O' }).click();
  await expect(page.getByTestId('audio-io-issue')).toContainText('APG_WEB_MIC_INSECURE_CONTEXT');
  await expect(page.getByTestId('audio-input-device')).toBeDisabled();
  await expect(page.getByTestId('audio-calibrate')).toBeDisabled();
  await page.getByRole('button', { name: 'Close Audio I/O' }).click();

  await page.getByTestId('preview-mode-file').click();
  await expect(page.getByTestId('preview-mode-file')).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByText('APG_WEB_MIC_INSECURE_CONTEXT')).toHaveCount(0);
  await expect(startStop).toBeEnabled();
  await startStop.click();
  await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
  await startStop.click();
  await expect(page.locator('.transport-state')).toHaveText('ready');

  await micMode.click();
  await expect(micMode).toHaveAttribute('aria-pressed', 'true');
  await expect(startStop).toBeDisabled();
  await expect(page.locator('.transport-state')).toHaveText('error');
});

test('shows microphone permission failure without crashing the editor', async ({ page }, testInfo) => {
  await page.addInitScript(() => {
    Object.defineProperty(navigator.mediaDevices, 'getUserMedia', {
      configurable: true,
      value: () => Promise.reject(new DOMException('Injected microphone permission denial', 'NotAllowedError')),
    });
  });
  await openWorkspace(page, testInfo);
  await expect(page.locator('.transport-state')).toHaveText(/idle|ready/, { timeout: 20_000 });
  await page.getByTestId('preview-compile').click();
  await expect(page.locator('.transport-state')).toHaveText('ready', { timeout: 20_000 });

  await page.getByTestId('preview-mode-mic').click();
  await expect(page.getByTestId('preview-mode-mic')).toHaveAttribute('aria-pressed', 'true');
  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-state')).toHaveText('error');
  await expect(page.locator('.transport-state')).toHaveAttribute(
    'aria-label',
    /Injected microphone permission denial/,
  );
  const errorBadge = page.getByTestId('preview-error-badge');
  const errorDetails = page.getByTestId('preview-error-details');
  await errorBadge.hover();
  await expect(errorDetails).toBeVisible();
  await expect(errorDetails).toContainText('Microphone access was denied');
  await expect(errorDetails).toContainText('NotAllowedError');
  await expect(errorDetails).toContainText('Injected microphone permission denial');
  await errorBadge.click();
  await expect(errorBadge).toHaveAttribute('aria-expanded', 'true');
  await page.mouse.move(2, 2);
  await expect(errorDetails).toBeVisible();
  const issueBanner = page.getByTestId('project-issue-banner');
  await expect(issueBanner).toContainText('Microphone');
  await expect(issueBanner).toContainText('Microphone access was denied');
  await expect(issueBanner).toContainText('NotAllowedError');
  await expect(issueBanner).toContainText('Injected microphone permission denial');
  await expect(page.locator('.topbar__status button').first()).toHaveText('Ready');
  await issueBanner.getByRole('button', { name: 'Audio I/O' }).click();
  await expect(page.getByTestId('audio-io-issue')).toContainText('Injected microphone permission denial');
  await expect(page.getByTestId('project-canvas')).toBeVisible();
});

test('contains and recovers from an invalid structured unit edit', async ({ page }, testInfo) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));
  await openWorkspace(page, testInfo);
  await page.getByTestId('project-node-drive1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
  await expect(page).toHaveURL(/#\/unit\/overdrive_copy$/);
  await page.getByRole('button', { name: 'Contract Settings' }).click();
  const version = page.getByLabel('Unit version');
  await expect(version).toBeVisible();
  await version.fill('not-a-version');
  await version.press('Tab');
  await expect(page.getByRole('alert')).toContainText('semantic versioning');
  await expect(page.getByTestId('contract-canvas')).toBeVisible();

  await version.fill('2.0.1');
  await version.press('Tab');
  await expect(page.getByRole('alert')).toHaveCount(0);
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  expect(pageErrors).toEqual([]);
});
