import { expect, test } from '@playwright/test';

test.describe('Local Marketplace & Device Flasher', () => {
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => localStorage.setItem('apg.studio.tour.v1', 'complete'));
  });

  test('opens Marketplace modal, loads preset, and executes target flashing', async ({ page }) => {
    await page.goto('/');

    // 1. Enter Studio via Guitar Pedalboard project card
    await page.locator('.project-card').filter({ hasText: 'Guitar Pedalboard' }).first().click();
    await expect(page.getByTestId('topbar-marketplace')).toBeVisible();

    // 2. Open Marketplace modal
    await page.getByTestId('topbar-marketplace').click();
    await expect(page.getByText('⚡ Marketplace & Device Flasher')).toBeVisible();

    // 3. Verify Local Marketplace tab & built-in preset card
    await expect(page.getByText('Guitar Pedalboard Preset')).toBeVisible();

    // 4. Click "Load into Studio"
    await page.getByRole('button', { name: 'Load into Studio' }).first().click();

    // Modal should close and pedalboard topbar remains responsive
    await expect(page.getByText('⚡ Marketplace & Device Flasher')).not.toBeVisible();
    await expect(page.getByTestId('topbar-marketplace')).toBeVisible();

    // 5. Re-open modal & switch to Flasher tab
    await page.getByTestId('topbar-marketplace').click();
    await page.getByRole('button', { name: '⚡ Device & Web Flasher' }).click();

    // 6. Test Flashing Target Selection & Telemetry
    await expect(page.getByText('1. Select Flashing Target')).toBeVisible();
    await expect(page.getByText('3. Telemetry & Hardware Budget Check')).toBeVisible();

    // 7. Click Flash Execution button
    await page.getByRole('button', { name: /⚡ Flash PRESET to M7/i }).click();

    // 8. Verify live execution console output
    await expect(page.getByText('Live Flash Execution Console')).toBeVisible();
    await expect(page.getByText('[SUCCESS] Flashing completed clean in PRESET mode for M7!')).toBeVisible({ timeout: 10000 });
  });
});
