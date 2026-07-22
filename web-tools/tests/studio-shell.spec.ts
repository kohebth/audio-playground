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
  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(11);
  await expect(page.locator('[data-testid^="project-node-path_panner_2"]')).toBeVisible();
  await expect(page.locator('[data-testid^="project-node-path_mixer_2"]')).toBeVisible();
  await expect(page.locator('[data-testid^="project-node-path_panner_2"] .unit-knob')).toHaveCount(2);
  await expect(page.locator('[data-testid^="project-node-path_mixer_2"] .unit-knob')).toHaveCount(2);
  await expect(page.getByText('Parallel section')).toHaveCount(0);

  await page.getByRole('button', { name: 'Pro', exact: true }).click();
  await expect(page.getByTestId('preview-mode-file')).toBeVisible();
  await expect(page.getByTestId('project-unit-item-path_panner_2_unit')).toBeDisabled();
  await expect(page.getByTestId('project-unit-item-path_mixer_2_unit')).toBeDisabled();
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

test('connects units by click and exposes undoable unit context actions', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();
  await page.getByRole('button', { name: 'Pro', exact: true }).click();

  const drive = page.getByTestId('project-node-drive1');
  await drive.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: 'drive1 actions' });
  await expect(menu).toBeVisible();
  await expect(menu.getByRole('menuitem')).toContainText(['Turn off', 'Replace…', 'Cut', 'Copy', 'Paste', 'Remove']);
  await menu.getByRole('menuitem', { name: 'Copy' }).click();

  await drive.click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Paste' }).click();
  const pasted = page.getByTestId('project-node-overdrive_copy');
  await expect(pasted).toBeVisible();
  await expect(page.getByTestId('route-item-9')).toHaveCount(0);

  await pasted.click({ button: 'right' });
  await page.getByRole('menu', { name: 'overdrive_copy actions' }).getByRole('menuitem', { name: 'Copy' }).click();
  await pasted.click({ button: 'right' });
  await page.getByRole('menu', { name: 'overdrive_copy actions' }).getByRole('menuitem', { name: 'Paste' }).click();
  const secondPasted = page.getByTestId('project-node-overdrive_copy_2');
  await expect(secondPasted).toBeVisible();

  await pasted.locator('.project-node__handle--output').click({ force: true });
  await expect(page.getByTestId('project-canvas')).toHaveClass(/flow-shell--connecting/);
  await secondPasted.locator('.project-node__handle--input').click({ force: true });
  await expect(page.getByTestId('route-item-9')).toContainText('overdrive_copy.output');
  await expect(page.getByTestId('route-item-9')).toContainText('overdrive_copy_2.input');

  await secondPasted.click({ button: 'right' });
  await page.getByRole('menu', { name: 'overdrive_copy_2 actions' }).getByRole('menuitem', { name: 'Remove' }).click();
  await expect(secondPasted).toHaveCount(0);
  await page.getByTestId('topbar-undo').click();
  await expect(page.getByTestId('project-node-overdrive_copy_2')).toBeVisible();
  const restoredFlowNode = page.locator('.react-flow__node[data-id="unit-overdrive_copy_2"]');
  await restoredFlowNode.focus();
  await restoredFlowNode.press('Shift+F10');
  await expect(page.getByRole('menu', { name: 'overdrive_copy_2 actions' })).toBeVisible();
  await page.keyboard.press('Escape');
});

test('collapses an empty split and join without exposing routing containers', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();

  await page.getByRole('button', { name: 'Add Chorus in parallel' }).click();
  const panner = page.locator('[data-testid^="project-node-path_panner_2"]');
  const mixer = page.locator('[data-testid^="project-node-path_mixer_2"]');
  const branchEffect = page.getByTestId('project-node-chorus');
  await expect(panner.locator('.unit-knob')).toHaveCount(2);
  await expect(mixer.locator('.unit-knob')).toHaveCount(2);
  await expect(page.getByText('Parallel section')).toHaveCount(0);

  await branchEffect.click({ button: 'right' });
  await page.getByRole('menu', { name: 'chorus actions' }).getByRole('menuitem', { name: 'Remove' }).click();
  await expect(branchEffect).toHaveCount(0);
  const directRails = page.locator('.react-flow__edge[data-id*="unit-path_panner_2-unit-path_mixer_2"]');
  await expect(directRails).toHaveCount(2);

  await panner.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: /path_panner_2.* actions/ });
  await expect(menu.getByRole('menuitem', { name: 'Remove split/join' })).toBeVisible();
  await menu.getByRole('menuitem', { name: 'Remove split/join' }).click();
  await expect(panner).toHaveCount(0);
  await expect(mixer).toHaveCount(0);
  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(8);
});

test('offers atom replace preview and disconnected clipboard actions', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();
  await page.getByRole('button', { name: 'Pro', exact: true }).click();
  await page.getByTestId('project-instance-item-drive1').dblclick();

  const clip = page.getByTestId('contract-node-clip_drive');
  await clip.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: 'clip_drive actions' });
  await expect(menu.getByRole('menuitem')).toContainText(['Replace…', 'Cut', 'Copy', 'Paste', 'Remove']);
  await menu.getByRole('menuitem', { name: 'Replace…' }).click();
  await expect(menu.getByRole('group', { name: 'Atom replacement preview' })).toContainText(/Keeps .* bindings/);
  await menu.getByRole('menuitem', { name: 'Copy' }).click();

  await clip.click({ button: 'right' });
  await page.getByRole('menu', { name: 'clip_drive actions' }).getByRole('menuitem', { name: 'Paste' }).click();
  const pasted = page.getByTestId('contract-node-clip_drive_copy');
  await expect(pasted).toBeVisible();
  await expect(page.locator('.react-flow__edge[data-id*="contract-clip_drive_copy"]')).toHaveCount(0);

  await clip.locator('.contract-node__handle--out').click({ force: true });
  await expect(page.getByTestId('contract-canvas')).toHaveClass(/flow-shell--connecting/);
  await pasted.locator('.contract-node__handle--in').click({ force: true });
  await expect(page.locator('.react-flow__edge[data-id*="contract-clip_drive-contract-clip_drive_copy"]')).toHaveCount(1);

  await clip.click({ button: 'right' });
  await page.getByRole('menu', { name: 'clip_drive actions' }).getByRole('menuitem', { name: 'Remove' }).click();
  await expect(clip).toHaveCount(0);
  await page.getByTestId('topbar-undo').click();
  await expect(page.getByTestId('contract-node-clip_drive')).toBeVisible();
});
