import { expect, test, type Page } from '@playwright/test';

async function confirmNextDialog(page: Page, action: () => Promise<void>) {
  const dialog = page.waitForEvent('dialog');
  await action();
  await (await dialog).accept();
}

async function addParallelEffect(page: Page, effect: string) {
  await page.locator('.react-flow__edge[data-id*="system-output"] .project-route__rail').last().hover();
  await page.locator('.project-route__action--branch.project-route__action--visible').click();
  await page.getByRole('group', { name: 'Choose an effect for this branch' })
    .getByRole('button', { name: new RegExp(`^${effect}`) }).click();
}

test('creates and restores a visual-first local project', async ({ page }) => {
  await page.goto('/');
  await expect(page.locator('body')).toHaveCSS('font-family', /JetBrains Mono Variable/);
  await expect(page.getByRole('button', { name: 'New project', exact: true }).first())
    .toHaveCSS('font-family', /JetBrains Mono Variable/);
  expect(await page.evaluate(async () => {
    await document.fonts.ready;
    return {
      loaded: document.fonts.check('12px "JetBrains Mono Variable"'),
      remoteFonts: performance.getEntriesByType('resource').some(entry => entry.name.includes('fonts.googleapis.com')),
    };
  })).toEqual({ loaded: true, remoteFonts: false });
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

  await page.getByTestId('effect-library-item-built-in-overdrive').dragTo(page.getByTestId('project-canvas'));
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
  await expect(tour).toBeVisible();
  await tour.click();

  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(8);
  await expect(page.locator('.react-flow__edge .project-route__rail')).toHaveCount(9);
  await expect(page.locator('.react-flow__edge[data-id*="system-input"] .project-route__rail')).toHaveCount(1);
  await expect(page.locator('.header-project-name strong')).toHaveText('Eight Rail');
  await expect(page.getByTestId('topbar-save')).toContainText('Saved');

  const railAnchors = await page.locator('.react-flow__node').evaluateAll(nodes => nodes.flatMap(node => {
    const handle = node.querySelector('.project-node__handle');
    if (!handle) return [];
    const bounds = handle.getBoundingClientRect();
    return [bounds.top + bounds.height / 2];
  }));
  expect(Math.max(...railAnchors) - Math.min(...railAnchors)).toBeLessThan(1);
  const routeEndpointYs = await page.locator('.project-route__rail').evaluateAll(paths => paths.flatMap(path => {
    const route = path as SVGPathElement;
    return [route.getPointAtLength(0).y, route.getPointAtLength(route.getTotalLength()).y];
  }));
  expect(Math.max(...routeEndpointYs) - Math.min(...routeEndpointYs)).toBeLessThan(1);
});

test('removing every placed unit recovers to a valid pass-through rail', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByRole('radio', { name: /8 effects/ }).check();
  await page.getByPlaceholder('Midnight pedalboard').fill('Empty Recovery');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tour = page.getByRole('button', { name: 'Skip tour' });
  await expect(tour).toBeVisible();
  await tour.click();

  for (const instanceId of ['gate1', 'phaser1', 'drive1', 'tone1', 'trem1', 'chorus1', 'delay1', 'reverb1']) {
    const node = page.getByTestId(`project-node-${instanceId}`);
    await node.click({ button: 'right' });
    await confirmNextDialog(page, () => page.getByRole('menu', { name: `${instanceId} actions` }).getByRole('menuitem', { name: 'Remove' }).click());
    await expect(node).toHaveCount(0);
  }

  await expect(page.locator('.react-flow__node[data-id^="unit-"]')).toHaveCount(0);
  await expect(page.locator('.react-flow__edge[data-id*="system-input-system-output"] .project-route__rail')).toHaveCount(1);
  await expect(page.locator('.transport-state')).toHaveText(/idle|ready/, { timeout: 20_000 });
  await page.getByTestId('preview-compile').click();
  await expect(page.locator('.transport-state')).toHaveText('ready', { timeout: 20_000 });
  await expect(page.getByTestId('project-issue-banner')).toHaveCount(0);
  await expect(page.locator('.topbar__status button').first()).toHaveText('Ready');
});

test('confirms Pipeline removal from the context menu and Delete key', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByPlaceholder('Midnight pedalboard').fill('Delete Guard');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await page.getByRole('button', { name: 'Skip tour' }).click();

  const node = page.getByTestId('project-node-overdrive');
  await page.getByTestId('effect-library-item-built-in-overdrive').dragTo(page.getByTestId('project-canvas'));
  await node.click({ button: 'right' });
  const menu = page.getByRole('menu', { name: 'overdrive actions' });
  const dismissed = page.waitForEvent('dialog');
  await menu.getByRole('menuitem', { name: 'Remove' }).click();
  await (await dismissed).dismiss();
  await expect(menu).toBeVisible();
  await expect(node).toBeVisible();
  await page.keyboard.press('Escape');

  await node.click();
  await confirmNextDialog(page, () => page.keyboard.press('Delete'));
  await expect(node).toHaveCount(0);
});

test('drags effect units onto the Pipeline and a specific rail', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByPlaceholder('Midnight pedalboard').fill('Drag Board');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await page.getByRole('button', { name: 'Skip tour' }).click();

  const canvas = page.getByTestId('project-canvas');
  const overdrive = page.getByTestId('effect-library-item-built-in-overdrive');
  await expect(overdrive).toHaveAttribute('draggable', 'true');
  await overdrive.dragTo(canvas, { targetPosition: { x: 420, y: 260 } });
  await expect(page.getByTestId('project-node-overdrive')).toBeVisible();

  const inputRail = page.getByTestId('rf__edge-route-0-system-input-unit-overdrive');
  const chorus = page.getByTestId('effect-library-item-built-in-chorus');
  await chorus.dragTo(page.locator('[data-project-route-index="0"]'));
  await expect(page.getByTestId('project-node-chorus')).toBeVisible();
  await expect(page.getByTestId('rf__edge-route-0-system-input-unit-chorus').locator('.project-route__rail'))
    .toHaveCount(1);
  await expect(page.getByTestId('rf__edge-route-1-unit-chorus-unit-overdrive').locator('.project-route__rail'))
    .toHaveAttribute('d', /^M /);
  await expect(inputRail).toHaveCount(0);
});

test('touch dragging inserts an effect inline on the targeted rail', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByPlaceholder('Midnight pedalboard').fill('Touch Rail');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await page.getByRole('button', { name: 'Skip tour' }).click();

  const overdrive = page.getByTestId('effect-library-item-built-in-overdrive');
  const overdriveBox = await overdrive.boundingBox();
  const blankRailTarget = page.locator('[data-project-route-index="0"]');
  const blankRailBox = await blankRailTarget.boundingBox();
  expect(overdriveBox).not.toBeNull();
  expect(blankRailBox).not.toBeNull();
  await overdrive.dispatchEvent('pointerdown', {
    bubbles: true,
    buttons: 1,
    clientX: overdriveBox!.x + overdriveBox!.width / 2,
    clientY: overdriveBox!.y + overdriveBox!.height / 2,
    isPrimary: true,
    pointerId: 41,
    pointerType: 'touch',
  });
  await overdrive.dispatchEvent('pointermove', {
    bubbles: true,
    buttons: 1,
    clientX: blankRailBox!.x + blankRailBox!.width / 2,
    clientY: blankRailBox!.y + blankRailBox!.height / 2,
    isPrimary: true,
    pointerId: 41,
    pointerType: 'touch',
  });

  await overdrive.dispatchEvent('pointerup', {
    bubbles: true,
    buttons: 0,
    clientX: blankRailBox!.x + blankRailBox!.width / 2,
    clientY: blankRailBox!.y + blankRailBox!.height / 2,
    isPrimary: true,
    pointerId: 41,
    pointerType: 'touch',
  });
  await expect(page.getByTestId('project-node-overdrive')).toBeVisible();

  await expect(page.getByTestId('project-route-drop-choices-0')).toHaveCount(0);
});

test('moves placed effects between rail positions without enabling free layout', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByRole('radio', { name: /8 effects/ }).check();
  await page.getByPlaceholder('Midnight pedalboard').fill('Move Rail');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tour = page.getByRole('button', { name: 'Skip tour' });
  await expect(tour).toBeVisible();
  await tour.click();

  const drive = page.getByTestId('project-node-drive1');
  const targetRail = page.locator('[data-project-route-index="6"]');
  await expect(drive).toHaveAttribute('draggable', 'true');
  const driveKnob = page.getByTestId('param-knob-drive1-drive');
  const knobBounds = await driveKnob.boundingBox();
  expect(knobBounds).not.toBeNull();
  const knobValue = driveKnob.locator('xpath=..').locator('.knob-value');
  const initialValue = Number((await knobValue.textContent())?.split(' ')[0]);
  await page.mouse.move(knobBounds!.x + knobBounds!.width / 2, knobBounds!.y + knobBounds!.height / 2);
  await page.mouse.down();
  await page.mouse.move(knobBounds!.x + knobBounds!.width / 2 + 18, knobBounds!.y + knobBounds!.height / 2, { steps: 4 });
  await page.mouse.up();
  const horizontalValue = Number((await knobValue.textContent())?.split(' ')[0]);
  expect(horizontalValue).toBeGreaterThan(initialValue);
  await page.mouse.move(knobBounds!.x + knobBounds!.width / 2, knobBounds!.y + knobBounds!.height / 2);
  await page.mouse.down();
  await page.mouse.move(knobBounds!.x + knobBounds!.width / 2, knobBounds!.y - 18, { steps: 4 });
  await page.mouse.up();
  expect(Number((await knobValue.textContent())?.split(' ')[0])).toBe(horizontalValue);
  await expect(page.getByTestId('project-canvas')).not.toHaveClass(/flow-shell--dragging-instance/);
  await expect(page.locator(
    '.react-flow__edge[data-id="route-2-unit-phaser1-unit-drive1"] .project-route__rail',
  )).toHaveCount(1);
  await drive.locator('.node-pedal-header').dragTo(targetRail);
  await expect(page.locator(
    '.react-flow__edge[data-id="route-2-unit-phaser1-unit-tone1"] .project-route__rail',
  )).toHaveCount(1);
  await expect(page.locator(
    '.react-flow__edge[data-id="route-5-unit-chorus1-unit-drive1"] .project-route__rail',
  )).toHaveCount(1);
  await expect(page.locator(
    '.react-flow__edge[data-id="route-6-unit-drive1-unit-delay1"] .project-route__rail',
  )).toHaveCount(1);

  await drive.click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Move…' }).click();
  await expect(page.locator('.project-move-prompt')).toContainText('Moving drive1');
  await expect(page.locator('[data-testid^="project-route-move-"]')).toHaveCount(9);
  await expect(page.locator('[data-testid^="project-route-move-"]:disabled')).toHaveCount(2);
  await page.getByTestId('project-route-move-0').click();
  await expect(page.locator(
    '.react-flow__edge[data-id="route-0-system-input-unit-drive1"] .project-route__rail',
  )).toHaveCount(1);
  await expect(page.locator('.project-move-prompt')).toHaveCount(0);
  await page.getByTestId('topbar-undo').click();
  await expect(page.locator(
    '.react-flow__edge[data-id="route-5-unit-chorus1-unit-drive1"] .project-route__rail',
  )).toHaveCount(1);
  await drive.click({ button: 'right' });
  await page.getByRole('menu', { name: 'drive1 actions' }).getByRole('menuitem', { name: 'Move…' }).click();
  await page.keyboard.press('Escape');
  await expect(page.locator('.project-move-prompt')).toHaveCount(0);
});

test('reveals branch hints on every rail and keeps routing helpers fixed', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: 'New project', exact: true }).first().click();
  await page.getByPlaceholder('Midnight pedalboard').fill('Branch Hints');
  await page.getByRole('button', { name: 'Create project' }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tour = page.getByRole('button', { name: 'Skip tour' });
  await expect(tour).toBeVisible();
  await tour.click();

  const rail = page.locator('.react-flow__edge[data-id*="system-input-system-output"] .project-route__rail');
  const hint = page.getByTestId('project-route-branch-0');
  await expect(hint).toHaveCSS('opacity', '0');
  await rail.hover({ force: true });
  await expect(hint).toHaveCSS('opacity', '1');
  await rail.click({ force: true });
  await page.mouse.move(4, 4);
  await expect(hint).toHaveCSS('opacity', '1');
  await hint.click();
  const picker = page.getByRole('group', { name: 'Choose an effect for this branch' });
  await picker.getByRole('button', { name: /Overdrive/ }).click();

  const panner = page.locator('[data-testid^="project-node-path_panner_2"]');
  const mixer = page.locator('[data-testid^="project-node-path_mixer_2"]');
  await expect(panner).toHaveAttribute('draggable', 'false');
  await expect(mixer).toHaveAttribute('draggable', 'false');
  await expect(page.getByTestId('project-node-overdrive')).toHaveAttribute('draggable', 'true');
  await expect(page.locator('[data-testid^="project-route-branch-"]')).toHaveCount(5);
  await expect(page.getByText('Parallel section')).toHaveCount(0);

  const pannerFlowNode = page.locator('.react-flow__node[data-id^="unit-path_panner_2"]');
  await pannerFlowNode.focus();
  await pannerFlowNode.press('Shift+F10');
  await expect(page.getByRole('menu', { name: /path_panner_2.* actions/ })
    .getByRole('menuitem', { name: 'Move…' })).toBeDisabled();
});

test('opens a unit for editing with one click in Contract', async ({ page }) => {
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  await page.getByRole('button', { name: 'Skip tour' }).click();

  await page.getByTestId('view-effect-contract').click();
  await expect(page.getByRole('heading', { name: 'Choose a unit to edit' })).toBeVisible();
  const overdrive = page.getByRole('button', { name: 'Edit Overdrive Contract' });
  await expect(overdrive).toHaveAttribute('draggable', 'false');
  await overdrive.click();

  await expect(page).toHaveURL(/#\/unit\/overdrive_copy$/);
  await expect(page.getByTestId('contract-canvas')).toBeVisible();
  await expect(page.getByTestId('contract-atom-library')).toBeVisible();
  await expect(page.getByTestId('atom-context-inspector')).toBeVisible();
});

test('keeps the Pipeline usable at phone width', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto('/');
  await page.getByRole('button', { name: /Guitar Pedalboard/ }).click();
  await expect(page.locator('.launch-screen')).toBeHidden({ timeout: 20_000 });
  const tour = page.getByRole('button', { name: 'Skip tour' });
  await expect(tour).toBeVisible();
  await tour.click();

  await expect(page.locator('.simple-library')).toBeVisible();
  await expect(page.locator('.simple-library__list')).toHaveCSS('overflow-x', 'auto');
  await expect(page.getByTestId('project-canvas')).toBeVisible();
  await expect(page.locator('.canvas--project .react-flow__minimap')).toBeHidden();
  await expect(page.locator('.app--simple .header-project')).toBeHidden();
  const save = page.getByTestId('topbar-save');
  await expect(save).toBeVisible();
  await expect(save).toContainText('Saved');
  await page.getByTestId('effect-library-item-built-in-overdrive').dragTo(page.getByTestId('project-canvas'));
  await expect(save).toBeEnabled();
  await expect(save).toContainText('Save');
  await save.click();
  await expect(save).toBeDisabled();
  await expect(save).toContainText('Saved');
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

  await addParallelEffect(page, 'Chorus');
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
  await expect(menu.getByRole('menuitem')).toContainText(['Turn off', 'Move…', 'Edit Contract', 'Replace…', 'Cut', 'Copy', 'Paste', 'Remove']);
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
  await confirmNextDialog(page, () => page.getByRole('menu', { name: 'overdrive_copy_2 actions' }).getByRole('menuitem', { name: 'Remove' }).click());
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

  await addParallelEffect(page, 'Chorus');
  const panner = page.locator('[data-testid^="project-node-path_panner_2"]');
  const mixer = page.locator('[data-testid^="project-node-path_mixer_2"]');
  const branchEffect = page.getByTestId('project-node-chorus');
  await expect(panner.locator('.unit-knob')).toHaveCount(2);
  await expect(mixer.locator('.unit-knob')).toHaveCount(2);
  await expect(page.getByText('Parallel section')).toHaveCount(0);

  await branchEffect.click({ button: 'right' });
  await confirmNextDialog(page, () => page.getByRole('menu', { name: 'chorus actions' }).getByRole('menuitem', { name: 'Remove' }).click());
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
  await confirmNextDialog(page, () => menu.getByRole('menuitem', { name: 'Remove split/join' }).click());
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

  await addParallelEffect(page, 'Chorus');
  await page.getByRole('button', { name: 'Fit View' }).click();
  await page.locator('.react-flow__edge[data-id="route-10-unit-path_panner_2-unit-chorus"]')
    .click({ button: 'right', force: true });
  const parallelGroup = page.getByRole('group', { name: 'Choose an effect for this branch' });
  await parallelGroup.getByRole('button', { name: /Delay/ }).click();

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

  const boundaryGeometry = await contractCanvas.evaluate(canvas => {
    const bounds = (selector: string) => canvas.querySelector(selector)?.getBoundingClientRect();
    const input = bounds('[data-id="contract-unit-input"]');
    const output = bounds('[data-id="contract-unit-output"]');
    const atoms = [...canvas.querySelectorAll('.react-flow__node-contractNode')]
      .map(node => node.getBoundingClientRect());
    if (!input || !output || atoms.length === 0) return null;
    return {
      inputCenterY: input.top + input.height / 2,
      inputRight: input.right,
      outputCenterY: output.top + output.height / 2,
      outputLeft: output.left,
      atomLeft: Math.min(...atoms.map(atom => atom.left)),
      atomRight: Math.max(...atoms.map(atom => atom.right)),
    };
  });
  expect(boundaryGeometry).not.toBeNull();
  expect(Math.abs(boundaryGeometry!.inputCenterY - boundaryGeometry!.outputCenterY)).toBeLessThan(1);
  expect(boundaryGeometry!.inputRight).toBeLessThan(boundaryGeometry!.atomLeft);
  expect(boundaryGeometry!.outputLeft).toBeGreaterThan(boundaryGeometry!.atomRight);

  const wireObstructions = await contractCanvas.locator('.react-flow__edge-path').evaluateAll(paths => {
    const atoms = [...document.querySelectorAll<HTMLElement>('.react-flow__node-contractNode')].map(node => ({
      id: node.closest<HTMLElement>('.react-flow__node')?.dataset.id ?? 'unknown',
      bounds: node.getBoundingClientRect(),
    }));
    return paths.flatMap(pathElement => {
      const path = pathElement as SVGPathElement;
      const transform = path.getScreenCTM();
      if (!transform) return [];
      const edgeId = path.closest<SVGGElement>('.react-flow__edge')?.dataset.id ?? 'unknown';
      const length = path.getTotalLength();
      return atoms.flatMap(atom => {
        for (let distance = 2; distance < length - 2; distance += 2) {
          const point = path.getPointAtLength(distance);
          const x = transform.a * point.x + transform.c * point.y + transform.e;
          const y = transform.b * point.x + transform.d * point.y + transform.f;
          if (x > atom.bounds.left + 2 && x < atom.bounds.right - 2
            && y > atom.bounds.top + 2 && y < atom.bounds.bottom - 2) {
            return [`${edgeId} crosses ${atom.id}`];
          }
        }
        return [];
      });
    });
  });
  expect(wireObstructions).toEqual([]);

  const levelValue = page.getByTestId('contract-node-level_value');
  const inputBoundary = page.getByTestId('unit-boundary-input');
  const levelBounds = await levelValue.boundingBox();
  const inputBounds = await inputBoundary.boundingBox();
  expect(levelBounds).not.toBeNull();
  expect(inputBounds).not.toBeNull();
  await page.mouse.move(levelBounds!.x + levelBounds!.width / 2, levelBounds!.y + levelBounds!.height / 2);
  await page.mouse.down();
  await page.mouse.move(inputBounds!.x - 40, levelBounds!.y + levelBounds!.height / 2, { steps: 8 });
  await page.mouse.up();
  await page.waitForTimeout(200);
  await expect(contractCanvas).toHaveAttribute('data-layout-status', 'ready', { timeout: 30_000 });
  const clampedLevelBounds = await levelValue.boundingBox();
  const stableInputBounds = await inputBoundary.boundingBox();
  const stableOutputBounds = await page.getByTestId('unit-boundary-output').boundingBox();
  expect(clampedLevelBounds).not.toBeNull();
  expect(stableInputBounds).not.toBeNull();
  expect(stableOutputBounds).not.toBeNull();
  expect(clampedLevelBounds!.x).toBeLessThan(levelBounds!.x - 50);
  expect(clampedLevelBounds!.x).toBeGreaterThan(stableInputBounds!.x + stableInputBounds!.width);
  expect(Math.abs(
    stableInputBounds!.y + stableInputBounds!.height / 2 - boundaryGeometry!.inputCenterY,
  )).toBeLessThan(1);
  expect(Math.abs(
    stableOutputBounds!.y + stableOutputBounds!.height / 2 - boundaryGeometry!.outputCenterY,
  )).toBeLessThan(1);

  const initialAtomCount = Number(await contractCanvas.getAttribute('data-atom-count'));
  const clipHard = page.getByTestId('atom-palette-item-amplitude_clip_hard');
  await expect(clipHard).toHaveAttribute('draggable', 'true');
  await clipHard.dragTo(contractCanvas, { targetPosition: { x: 420, y: 260 } });
  await expect(contractCanvas).toHaveAttribute('data-atom-count', String(initialAtomCount + 1));
  await expect(page.getByTestId('contract-node-amplitude_clip_hard')).toBeVisible();
  await expect(clipHard).not.toHaveClass(/atom-palette__item--dragging/);

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
