import { expect, type Page, test } from '@playwright/test';

async function openWorkspace(page: Page) {
  await page.addInitScript(() => {
    localStorage.setItem('apg.studio.mode.v1', 'pro');
    localStorage.setItem('apg.studio.tour.v1', 'complete');
  });
  await page.goto('/#/projects');
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 15_000 });
  await expect(page.getByTestId('launch-workspace')).toHaveCount(0);
  await expect(page).toHaveURL(/#\/projects$/);
}

test('opens projects in Pipeline and keeps Personal Contract routes stable', async ({ page }) => {
  await page.addInitScript(() => localStorage.setItem('apg.studio.tour.v1', 'complete'));
  await page.goto('/#/unit/overdrive');
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 15_000 });
  await expect(page).toHaveURL(/#\/projects$/);
  await page.getByTestId('project-node-drive1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
  await expect(page).toHaveURL(/#\/unit\/overdrive_copy$/);
  await expect(page.getByTestId('contract-canvas')).toBeVisible();

  await page.getByTestId('view-effect-pipeline').click();
  await expect(page).toHaveURL(/#\/projects$/);
  await expect(page.getByTestId('project-canvas')).toBeVisible();
});

test('controls file and microphone playback with transport shortcuts', async ({ page }) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page);

  const state = page.locator('.transport-island');
  const transport = page.getByTestId('preview-start-stop');

  await expect(state).toHaveAttribute('data-transport-phase', /idle|ready/, { timeout: 15_000 });
  await expect(page.getByRole('group', { name: 'Audio input mode' }).getByRole('button')).toHaveText(['Mic', 'Audio File']);
  await expect(page.getByTestId('preview-mode-mic')).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByTestId('preview-mode-file')).toHaveAttribute('aria-pressed', 'false');
  await expect(transport).toBeEnabled();
  await page.keyboard.press('Space');
  await expect(page.locator('.transport-island')).toHaveClass(/transport-island--running/, { timeout: 15_000 });
  await page.keyboard.press('Space');
  await expect(state).toHaveAttribute('data-transport-phase', 'ready');

  await page.evaluate(() => localStorage.removeItem('apg.unit-editor.workspace.v2'));
  await page.keyboard.press('Control+s');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();

  await page.evaluate(() => localStorage.removeItem('apg.unit-editor.workspace.v2'));
  await page.keyboard.press('b');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();
  await expect(state).toHaveAttribute('data-transport-phase', 'ready');

  await page.getByTestId('preview-mode-file').click();
  await expect(page.getByTestId('preview-mode-file')).toHaveAttribute('aria-pressed', 'true');
  await transport.click();
  await expect(page.locator('.transport-island')).toHaveClass(/transport-island--running/, { timeout: 15_000 });
  await transport.click();
  await expect(state).toHaveAttribute('data-transport-phase', 'ready');

  expect(pageErrors).toEqual([]);
});

test('reconfigures live audio devices and restores running controls', async ({ page }, testInfo) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page);
  await expect(page.locator('.transport-island')).toHaveAttribute('data-transport-phase', /idle|ready/, { timeout: 15_000 });
  await page.getByTestId('preview-mode-mic').click();
  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-island')).toHaveClass(/transport-island--running/, { timeout: 15_000 });

  await page.getByTestId('audio-io-open').click();
  const audioIo = page.getByTestId('audio-io-panel');
  await expect(audioIo).toBeVisible();
  await expect(page.getByTestId('audio-latency-chirp')).toBeEnabled();
  const input = page.getByTestId('audio-input-device');
  await expect(input.locator('option')).not.toHaveCount(0);
  const selectedInput = await input.inputValue();
  await input.selectOption(selectedInput);
  await expect(page.locator('.transport-island')).toHaveClass(/transport-island--running/, { timeout: 20_000 });
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
  await expect(page.locator('.transport-island')).toHaveClass(/transport-island--running/, { timeout: 20_000 });
  await audioIo.scrollIntoViewIfNeeded();
  await page.screenshot({ path: testInfo.outputPath('audio-io-running.png'), fullPage: true });

  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-island')).toHaveAttribute('data-transport-phase', 'ready');
  expect(pageErrors).toEqual([]);
});

test('calibrates latency candidates and retains a stable configuration', async ({ page }, testInfo) => {
  test.setTimeout(90_000);
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await openWorkspace(page);
  await expect(page.locator('.transport-island')).toHaveAttribute('data-transport-phase', /idle|ready/, { timeout: 15_000 });
  await page.getByTestId('preview-mode-mic').click();
  await page.getByTestId('audio-io-open').click();
  const audioIo = page.getByTestId('audio-io-panel');
  await expect(audioIo).toBeVisible();
  await page.getByTestId('audio-calibrate').click();
  await expect(page.getByTestId('audio-calibrate')).toContainText('Calibrating');
  await expect(audioIo.locator('.audio-io-result')).toBeVisible({ timeout: 45_000 });
  await expect(page.getByTestId('audio-calibration-report')).toBeVisible();
  await expect(page.getByTestId('audio-calibration-report').locator('.audio-calibration-table__row')).toHaveCount(5);

  await audioIo.scrollIntoViewIfNeeded();
  await page.screenshot({ path: testInfo.outputPath('audio-calibration-report.png'), fullPage: true });

  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-island')).toHaveClass(/transport-island--running/, { timeout: 15_000 });
  await page.getByTestId('preview-start-stop').click();
  await expect(page.locator('.transport-island')).toHaveAttribute('data-transport-phase', 'ready');
  expect(pageErrors).toEqual([]);
});
