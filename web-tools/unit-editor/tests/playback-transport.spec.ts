import { expect, test } from '@playwright/test';

test('controls file and microphone playback with transport shortcuts', async ({ page }) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await page.goto('/');
  await expect(page.getByTestId('launch-workspace')).toBeVisible({ timeout: 15_000 });
  await page.getByTestId('launch-workspace').click();
  await expect(page.getByTestId('launch-workspace')).toBeHidden();
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
