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

  await page.getByRole('button', { name: /Overdrive Warm saturation/ }).click();
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
