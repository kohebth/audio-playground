import { expect, test } from '@playwright/test';

test('creates and restores a visual-first local project', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('heading', { name: 'Pick up where you left off.' })).toBeVisible();
  await expect(page.locator('.project-card')).toContainText(['Guitar Pedalboard', 'New project']);

  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByPlaceholder('Midnight pedalboard').fill('Midnight Board');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page).toHaveURL(/#\/projects$/);
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.locator('.simple-library')).toBeVisible();
  await expect(page.getByText('Build from left to right')).toBeVisible();
  await page.getByRole('button', { name: 'Skip tour' }).click();

  await page.getByRole('button', { name: 'Add Overdrive', exact: true }).click();
  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(1);
  await expect(page.getByTestId('project-node-overdrive')).toBeVisible();
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();

  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.getByTestId('project-node-overdrive')).toBeVisible();

  await page.getByRole('button', { name: 'Pro', exact: true }).click();
  await expect(page.locator('.sidebar-left')).toBeVisible();
  await page.getByTitle('All projects').click();
  await expect(page.getByRole('heading', { name: 'Pick up where you left off.' })).toBeVisible();
  await expect(page.locator('.project-card')).toContainText(['Midnight Board', 'Guitar Pedalboard', 'New project']);
});

test('keeps the Simple workspace usable at phone width', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tour = page.getByRole('button', { name: 'Skip tour' });
  if (await tour.isVisible()) await tour.click();

  await expect(page.locator('.simple-library')).toBeVisible();
  await expect(page.locator('.simple-library__list')).toHaveCSS('overflow-x', 'auto');
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  const libraryBox = await page.locator('.simple-library').boundingBox();
  expect(libraryBox?.width).toBeGreaterThanOrEqual(380);
  expect(libraryBox?.height).toBeLessThanOrEqual(200);
});

test('recalls presets and scenes, builds a real parallel path, and exports .apg', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();

  await expect(page.getByTestId('preview-mode-file')).toHaveCount(0);
  await expect(page.getByTestId('preview-mode-mic')).toBeVisible();
  await page.getByTestId('project-node-drive1').click();
  await page.getByRole('button', { name: /Warm Push/ }).click();

  await page.getByTestId('scene-save-open').click();
  await page.getByLabel('Scene name').fill('Browser Scene');
  await page.locator('.scene-bar__create button[type="submit"]').click();
  await expect(page.getByTestId('scene-apply-Browser Scene')).toBeVisible();

  await page.getByRole('button', { name: 'Add Chorus in parallel' }).click();
  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(10);
  await expect(page.locator('[data-testid^="project-node-wet_dry_mix"]')).toBeVisible();

  await page.getByRole('button', { name: 'Pro', exact: true }).click();
  await expect(page.getByTestId('preview-mode-file')).toBeVisible();
  await page.getByRole('button', { name: 'Select gate1 for batch editing' }).click();
  await expect(page.getByTestId('batch-action-bar')).toContainText('2 effects selected');
  await page.locator('.topbar__status .status-pill').first().click();
  await expect(page.getByTestId('readiness-panel')).toBeVisible();
  await page.getByLabel('Close readiness').click();

  const download = page.waitForEvent('download');
  await page.getByTestId('topbar-export').click();
  expect((await download).suggestedFilename()).toMatch(/\.apg$/);
});

test('edits a unit through structured Pro controls without exposing raw source', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.getByRole('button', { name: 'Skip tour' })).toBeVisible();
  await page.getByRole('button', { name: 'Skip tour' }).click();
  await page.getByRole('button', { name: 'Pro', exact: true }).click();
  await page.getByTestId('project-instance-item-drive1').dblclick();

  await expect(page).toHaveURL(/#\/unit\/overdrive$/);
  await expect(page.getByTestId('structured-unit-editor')).toBeVisible();
  await expect(page.locator('textarea.workspace-editor')).toHaveCount(0);
  expect((await page.locator('.file-item').allTextContents()).join(' ')).not.toContain('.yaml');

  const title = page.getByLabel('Unit display name');
  await title.fill('Studio Overdrive');
  await title.press('Tab');
  await expect(title).toHaveValue('Studio Overdrive');

  await page.getByLabel('New parameter name').fill('presence_extra');
  await page.getByRole('button', { name: 'Add control' }).click();
  await expect(page.getByTestId('contract-param-row-presence_extra')).toBeVisible();
  await page.getByRole('button', { name: 'Remove presence_extra' }).click();
  await expect(page.getByTestId('contract-param-row-presence_extra')).toHaveCount(0);

  const inputName = page.getByLabel('inputs 1 name');
  await inputName.fill('source');
  await inputName.press('Tab');
  await expect(page.getByTestId('unit-boundary-input')).toHaveText('source');

  await page.getByRole('button', { name: /Save this unit to Personal Library/ }).click();
  await page.getByRole('button', { name: 'Simple', exact: true }).click();
  await expect(page.locator('.effect-library-card').filter({ hasText: 'Yours' })).toBeVisible();
});
