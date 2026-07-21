import { expect, test } from '@playwright/test';

test('keeps routing-helper drafts repairable and lays out Personal atom chains', async ({ page }, testInfo) => {
  const pageErrors: string[] = [];
  page.on('pageerror', error => pageErrors.push(error.message));

  await page.goto('/#/projects');
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.getByTestId('effect-chain-canvas')).toBeVisible();
  await expect(page.getByRole('button', { name: 'Effect Chain', exact: true })).toBeVisible();
  await expect(page.getByRole('button', { name: 'Atom Chain', exact: true })).toBeVisible();
  const tourClose = page.getByRole('button', { name: 'Close tour' });
  if (await tourClose.isVisible()) await tourClose.click();

  const firstEffect = page.locator('.effect-library-card').first();
  await firstEffect.getByRole('button', { name: /in parallel$/ }).click();
  const parallel = page.locator('.effect-chain-parallel').last();
  await expect(parallel).toBeVisible();

  const panner = parallel.locator('.effect-chain-helper').first();
  await panner.getByRole('button', { name: 'Open panner actions' }).click();
  await page.getByRole('menuitem', { name: 'Remove panner' }).click({ timeout: 10_000 });
  await expect(parallel.locator('.effect-chain-helper--missing')).toContainText('Restore panner');
  await expect(page.getByTestId('topbar-export')).toBeDisabled();

  await parallel.getByRole('button', { name: /Restore panner/ }).click();
  await expect(parallel.locator('.effect-chain-helper--missing')).toHaveCount(0);
  await expect(page.getByTestId('topbar-export')).toBeEnabled();
  await page.screenshot({ path: testInfo.outputPath('effect-chain.png'), fullPage: true });

  await firstEffect.click({ button: 'right' });
  await page.getByRole('menuitem', { name: 'Edit Atom Chain' }).click();
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  await expect(page.getByTestId('atom-context-inspector')).toBeVisible();
  await page.getByRole('button', { name: 'Auto Layout' }).click();
  await expect(page.locator('.contract-layout-error')).toHaveCount(0, { timeout: 20_000 });

  await page.getByRole('button', { name: 'Unit Settings' }).click();
  await expect(page.getByTestId('unit-settings-drawer')).toBeVisible();
  await expect(page.getByTestId('atom-context-inspector')).not.toContainText('Unit Contract');
  await page.screenshot({ path: testInfo.outputPath('atom-chain.png'), fullPage: true });

  expect(pageErrors).toEqual([]);
});
