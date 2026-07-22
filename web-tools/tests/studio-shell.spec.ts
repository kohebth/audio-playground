import { expect, test } from '@playwright/test';

test('creates and restores a visual-first local project', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByRole('heading', { name: 'Pick up where you left off.' })).toBeVisible();
  await expect(page.locator('.project-card')).toContainText(['Guitar Pedalboard', 'New project']);

  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await expect(page.getByRole('radio', { name: /Blank rail/ })).toBeChecked();
  await page.getByPlaceholder('Midnight pedalboard').fill('Midnight Board');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page).toHaveURL(/#\/projects$/);
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.locator('.simple-library')).toBeVisible();
  await expect(page.getByText('Build from left to right')).toBeVisible();
  await expect(page.getByTestId('view-effect-pipeline')).toHaveText('Pipeline');
  await expect(page.getByTestId('view-effect-contract')).toHaveText('Contract');
  await expect(page.locator('.react-flow__edge .project-route__rail')).toHaveCount(1);
  await page.getByRole('button', { name: 'Skip tour' }).click();

  await page.getByRole('button', { name: 'Add Overdrive', exact: true }).click();
  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(1);
  await expect(page.getByTestId('project-node-overdrive')).toBeVisible();
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2'))).not.toBeNull();

  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.getByTestId('project-node-overdrive')).toBeVisible();

  await page.getByTestId('view-effect-contract').click();
  await expect(page.getByTestId('contract-empty-state')).toBeVisible();
  await page.getByTitle('All projects').click();
  await expect(page.getByRole('heading', { name: 'Pick up where you left off.' })).toBeVisible();
  await expect(page.locator('.project-card')).toContainText(['Midnight Board', 'Guitar Pedalboard', 'New project']);
});

test('creates a project from the eight-effect rail template', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByRole('radio', { name: /8 effects/ }).check();
  await page.getByPlaceholder('Midnight pedalboard').fill('Eight Rail');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tour = page.getByRole('button', { name: 'Skip tour' });
  if (await tour.isVisible()) await tour.click();

  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(8);
  await expect(page.locator('.react-flow__edge .project-route__rail')).toHaveCount(9);
  await expect(page.locator('.react-flow__edge[data-id*="system-input"] .project-route__rail')).toHaveCount(1);
  await expect(page.locator('.header-project-name strong')).toHaveText('Eight Rail');
  await expect(page.getByText('Saved locally', { exact: true })).toBeVisible();
});

test('keeps the Pipeline usable at phone width', async ({ page }) => {
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

  const audioInputMode = page.getByRole('group', { name: 'Audio input mode' });
  await expect(audioInputMode.getByRole('button')).toHaveText(['Mic', 'Audio File']);
  await expect(page.getByTestId('preview-mode-mic')).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByTestId('preview-mode-file')).toHaveAttribute('aria-pressed', 'false');
  const firstRail = page.locator('.react-flow__edge').first();
  await expect(firstRail.locator('.project-route__rail')).toHaveCount(1);
  await expect(firstRail.locator('.project-route__bed')).toHaveCount(0);
  const driveNode = page.getByTestId('project-node-drive1');
  const driveOutput = driveNode.locator('.project-node__handle--output');
  await expect(driveOutput).toHaveCSS('opacity', '0');
  await driveNode.hover();
  await expect(driveOutput).toHaveCSS('opacity', '1');
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
  for (const [helper, handleSelector] of [
    [page.locator('[data-testid^="project-node-path_panner_2"]'), '.project-node__handle--output'],
    [page.locator('[data-testid^="project-node-path_mixer_2"]'), '.project-node__handle--input'],
  ] as const) {
    const alignment = await helper.evaluate((node, selector) => {
      const center = (element: Element) => {
        const bounds = element.getBoundingClientRect();
        return bounds.top + bounds.height / 2;
      };
      return {
        height: (node.querySelector('.node-pedal') as HTMLElement | null)?.offsetHeight ?? 0,
        knobs: [...node.querySelectorAll('.unit-knob .knob')].map(center),
        handles: [...node.querySelectorAll(selector)].map(center),
      };
    }, handleSelector);
    expect(alignment.height).toBeGreaterThanOrEqual(266);
    expect(alignment.knobs).toHaveLength(2);
    expect(alignment.handles).toHaveLength(2);
    expect(Math.abs(alignment.knobs[0] - alignment.handles[0])).toBeLessThan(1);
    expect(Math.abs(alignment.knobs[1] - alignment.handles[1])).toBeLessThan(1);
    expect(alignment.knobs[1] - alignment.knobs[0]).toBeGreaterThan(10);
  }

  await page.getByTestId('view-effect-contract').click();
  await expect(page.getByTestId('preview-mode-file')).toBeVisible();
  await expect(page.getByTestId('contract-empty-state')).toBeVisible();
  await page.locator('.topbar__status .status-pill').first().click();
  await expect(page.getByTestId('readiness-panel')).toBeVisible();
  await page.getByLabel('Close readiness').click();

  const download = page.waitForEvent('download');
  await page.getByTestId('topbar-export').click();
  expect((await download).suggestedFilename()).toMatch(/\.apg$/);
});

test('clones a built-in contract to Personal and propagates edits through the active project', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.getByRole('button', { name: 'Skip tour' })).toBeVisible();
  await page.getByRole('button', { name: 'Skip tour' }).click();
  const drive = page.getByTestId('project-node-drive1');
  await drive.click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();

  await expect(page).toHaveURL(/#\/unit\/overdrive_copy$/);
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  await expect(page.getByTestId('atom-context-inspector')).toBeVisible();
  await expect(page.locator('.project-inspector')).toHaveCount(0);
  await page.getByRole('button', { name: 'Contract Settings' }).click();
  await expect(page.getByTestId('unit-settings-drawer')).toBeVisible();
  await expect(page.getByTestId('structured-unit-editor')).toBeVisible();
  await expect(page.locator('textarea.workspace-editor')).toHaveCount(0);

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
  await page.getByLabel('outputs 1 name').fill('result');
  await page.getByLabel('outputs 1 name').press('Tab');
  await expect(page.getByTestId('unit-boundary-output')).toHaveText('result');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2') ?? ''))
    .toContain('drive1.source');
  await expect.poll(() => page.evaluate(() => localStorage.getItem('apg.unit-editor.workspace.v2') ?? ''))
    .toContain('drive1.result');

  await page.getByLabel('Close Contract Settings').last().click();
  await page.getByTestId('view-effect-pipeline').click();
  await expect(page.getByTestId('project-node-drive1')).toBeVisible();
  await expect(page.locator('.effect-library-card').filter({ hasText: 'Studio Overdrive' })).toContainText('Yours');

  await page.reload();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  await expect(page.getByTestId('contract-canvas')).toHaveCount(0);
});

test('connects units by click and exposes undoable unit context actions', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();

  const drive = page.getByTestId('project-node-drive1');
  await drive.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: 'drive1 actions' });
  await expect(menu).toBeVisible();
  await expect(menu.getByRole('menuitem')).toContainText(['Turn off', 'Edit Contract', 'Replace…', 'Cut', 'Copy', 'Paste', 'Remove']);
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
  await expect(page.locator(
    '.react-flow__edge[data-id*="unit-overdrive_copy-unit-overdrive_copy_2"]',
  )).toHaveCount(1);

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
  const directRailPaths = await directRails.locator('.react-flow__edge-path').evaluateAll(paths => (
    paths.map(path => path.getAttribute('d') ?? '')
  ));
  expect(directRailPaths.every(path => path.startsWith('M ') && !path.includes('C'))).toBe(true);
  expect(new Set(directRailPaths).size).toBe(2);

  await panner.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: /path_panner_2.* actions/ });
  await expect(menu.getByRole('menuitem', { name: 'Remove split/join' })).toBeVisible();
  await menu.getByRole('menuitem', { name: 'Remove split/join' }).click();
  await expect(panner).toHaveCount(0);
  await expect(mixer).toHaveCount(0);
  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(8);
});

test('renders nested branches as knob units with separate orthogonal rails', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();

  await page.getByRole('button', { name: 'Add Chorus in parallel' }).click();
  await page.getByRole('button', { name: 'Fit View' }).click();
  await page.locator('.react-flow__edge[data-id="route-10-unit-path_panner_2-unit-chorus"]')
    .click({ button: 'right', force: true });
  const parallelGroup = page.getByRole('group', { name: 'Parallel effect' });
  await parallelGroup.getByLabel('Parallel effect').selectOption('delay');
  await page.getByRole('button', { name: 'Create parallel path' }).click();

  const panners = page.locator('[data-testid^="project-node-path_panner_2"]');
  const mixers = page.locator('[data-testid^="project-node-path_mixer_2"]');
  await expect(panners).toHaveCount(2);
  await expect(mixers).toHaveCount(2);
  await expect(panners.locator('.unit-knob')).toHaveCount(4);
  await expect(mixers.locator('.unit-knob')).toHaveCount(4);
  await expect(page.getByText('Parallel section')).toHaveCount(0);
  const pannerHeights = await panners.evaluateAll(nodes => nodes.map(node => (
    (node.querySelector('.node-pedal') as HTMLElement | null)?.offsetHeight ?? 0
  )));
  const mixerHeights = await mixers.evaluateAll(nodes => nodes.map(node => (
    (node.querySelector('.node-pedal') as HTMLElement | null)?.offsetHeight ?? 0
  )));
  expect(Math.min(...pannerHeights, ...mixerHeights)).toBeGreaterThanOrEqual(266);
  expect(Math.max(...pannerHeights)).toBeGreaterThan(Math.min(...pannerHeights));
  expect(Math.max(...mixerHeights)).toBeGreaterThan(Math.min(...mixerHeights));

  const directRails = page.locator(
    '.react-flow__edge[data-id*="unit-path_panner"][data-id*="unit-path_mixer"] .react-flow__edge-path',
  );
  await expect(directRails).toHaveCount(2);
  const paths = await directRails.evaluateAll(items => items.map(item => item.getAttribute('d') ?? ''));
  expect(paths.every(path => path.startsWith('M ') && !path.includes('C'))).toBe(true);
  expect(new Set(paths).size).toBe(2);
});

test('offers a Graphviz atom editor with atom-only inspection and safe graph actions', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const skipTour = page.getByRole('button', { name: 'Skip tour' });
  await expect(skipTour).toBeVisible();
  await skipTour.click();
  await page.getByTestId('project-node-drive1').click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Edit Contract' }).click();
  const contractCanvas = page.getByTestId('contract-canvas');
  await expect(contractCanvas).toHaveAttribute('data-layout-engine', 'graphviz');
  await expect(contractCanvas).toHaveAttribute('data-layout-status', 'ready', { timeout: 30_000 });
  await expect(page.getByTestId('contract-auto-layout')).toHaveText('Auto Layout');
  await expect.poll(async () => Number(await contractCanvas.getAttribute('data-routed-edge-count'))).toBeGreaterThan(0);
  const routedPaths = await contractCanvas.locator('.react-flow__edge-path').evaluateAll(paths => (
    paths.map(path => path.getAttribute('d') ?? '')
  ));
  expect(routedPaths.some(path => path.includes('Q'))).toBe(true);
  expect(routedPaths.every(path => !path.includes('C'))).toBe(true);

  const clip = page.getByTestId('contract-node-clip_drive');
  await clip.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: 'clip_drive actions' });
  await expect(menu.getByRole('menuitem')).toContainText(['Replace…', 'Cut', 'Copy', 'Paste', 'Remove']);
  const inspector = page.getByTestId('atom-context-inspector');
  await expect(inspector.getByRole('heading', { name: 'clip_drive' })).toBeVisible();
  await expect(inspector.getByTestId('structured-unit-editor')).toHaveCount(0);
  await expect(inspector.getByText('Unit Contract', { exact: true })).toHaveCount(0);
  await expect(inspector.getByText('Unit Inspector', { exact: true })).toHaveCount(0);
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
  await pasted.click();
  await inspector.getByRole('button', { name: 'Remove' }).click();
  await expect(pasted).toHaveCount(0);
});
