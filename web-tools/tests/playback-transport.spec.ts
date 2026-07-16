import { expect, type Page, test } from '@playwright/test';

async function openWorkspace(page: Page) {
  await page.goto('/');
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 15_000 });
  await expect(page.getByTestId('launch-workspace')).toHaveCount(0);
}

test('controls file and microphone playback with transport shortcuts', async ({ page }) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page);
  await page.locator('.topbar__logo').click();

  const state = page.locator('.transport-state');
  const transport = page.getByTestId('preview-start-stop');

  await expect(state).toHaveText(/idle|ready/, { timeout: 15_000 });
  await page.keyboard.press('Space');
  await expect(state).toHaveText('running', { timeout: 15_000 });
  await page.keyboard.press('m');
  await expect(page.locator('button[title="Unmute output"]')).toBeVisible();
  await page.keyboard.press('Space');
  await expect(state).toHaveText('ready');

  await page.evaluate(() => localStorage.removeItem('apg.unit-editor.workspace.v2'));
  await page.keyboard.press('Control+s');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();

  await page.evaluate(() => localStorage.removeItem('apg.unit-editor.workspace.v2'));
  await page.keyboard.press('b');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();
  await expect(state).toHaveText('ready');

  await page.getByTestId('preview-mode-mic').click();
  await transport.click();
  await expect(state).toHaveText('running', { timeout: 15_000 });
  await transport.click();
  await expect(state).toHaveText('ready');

  expect(pageErrors).toEqual([]);
});

test('reconfigures live audio devices and restores running controls', async ({ page }, testInfo) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page);
  await page.locator('.topbar__logo').click();
  await expect(page.locator('.transport-state')).toHaveText(/idle|ready/, { timeout: 15_000 });
  await page.getByTestId('preview-mode-mic').click();
  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 15_000 });
  await page.keyboard.press('m');
  await expect(page.locator('button[title="Unmute output"]')).toBeVisible();

  const audioIo = page.getByTestId('audio-io-panel');
  await audioIo.locator(':scope > summary').click();
  await expect(page.getByTestId('audio-latency-chirp')).toBeEnabled();
  const input = page.getByTestId('audio-input-device');
  await expect(input.locator('option')).not.toHaveCount(0);
  const selectedInput = await input.inputValue();
  await input.selectOption(selectedInput);
  await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
  await expect(page.locator('button[title="Unmute output"]')).toBeVisible();
  await expect(audioIo).toContainText('Context');

  await page.evaluate(() => {
    const mediaDevices = navigator.mediaDevices;
    const original = mediaDevices.getUserMedia.bind(mediaDevices);
    let failNext = true;
    Object.defineProperty(mediaDevices, 'getUserMedia', {
      configurable: true,
      value: (constraints: MediaStreamConstraints) => {
        if (failNext) {
          failNext = false;
          return Promise.reject(new DOMException('Injected device failure', 'NotReadableError'));
        }
        return original(constraints);
      },
    });
  });
  await input.selectOption(selectedInput);
  await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 20_000 });
  await expect(page.locator('button[title="Unmute output"]')).toBeVisible();
  await audioIo.scrollIntoViewIfNeeded();
  await page.screenshot({ path: testInfo.outputPath('audio-io-running.png'), fullPage: true });

  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-state')).toHaveText('ready');
  expect(pageErrors).toEqual([]);
});

test('calibrates latency candidates and retains a stable configuration', async ({ page }, testInfo) => {
  test.setTimeout(90_000);
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page);
  await page.locator('.topbar__logo').click();
  await expect(page.locator('.transport-state')).toHaveText(/idle|ready/, { timeout: 15_000 });
  await page.getByTestId('preview-mode-mic').click();
  const audioIo = page.getByTestId('audio-io-panel');
  await audioIo.locator(':scope > summary').click();
  await page.getByTestId('audio-calibrate').click();
  await expect(page.getByTestId('audio-calibrate')).toContainText('Calibrating');
  await expect(audioIo.locator('.audio-io-result')).toBeVisible({ timeout: 45_000 });

  await page.getByTestId('inspector-tab-contract').click();
  const diagnostics = page.locator('details.developer-diagnostics');
  await diagnostics.locator(':scope > summary').click();
  await expect(page.getByTestId('audio-calibration-report')).toBeVisible();
  await expect(page.getByTestId('audio-calibration-report').locator('.audio-calibration-table__row')).toHaveCount(5);
  await page.getByTestId('audio-calibration-report').scrollIntoViewIfNeeded();
  await page.screenshot({ path: testInfo.outputPath('audio-calibration-report.png'), fullPage: true });

  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-state')).toHaveText('running', { timeout: 15_000 });
  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-state')).toHaveText('ready');
  expect(pageErrors).toEqual([]);
});
