import yaml from 'js-yaml';

import { parseProjectGraphDraft } from './projectV2Graph.ts';
import { createUnknownReadiness, type ProjectReadinessSnapshot, type ReadinessStatus } from './projectPackage.ts';
import type { WorkspacePayload } from './workspacePersistence.ts';

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function resolveWorkspacePath(baseFile: string, reference: string): string {
  const segments = baseFile.split('/');
  segments.pop();
  for (const segment of reference.split('/')) {
    if (!segment || segment === '.') continue;
    if (segment === '..') {
      if (segments.length === 0) throw new Error(`Workspace reference "${reference}" escapes its root.`);
      segments.pop();
    } else {
      segments.push(segment);
    }
  }
  return segments.join('/');
}

export function evaluateWorkspaceReadiness(
  workspace: WorkspacePayload,
  previous: ProjectReadinessSnapshot = createUnknownReadiness(),
): ProjectReadinessSnapshot {
  const diagnostics: ProjectReadinessSnapshot['diagnostics'] = [];
  const targets: Record<string, ReadinessStatus> = {};
  let validation: ReadinessStatus = 'ready';

  try {
    const projectFile = workspace.files.find(file => file.path === workspace.entryProject && file.role === 'project');
    if (!projectFile) throw new Error(`Entry project "${workspace.entryProject}" is missing.`);
    const project = parseProjectGraphDraft(projectFile.content);
    const desiredTargets = [...new Set([project.targets.default, ...project.targets.export].filter(Boolean))];
    const activeUnitIds = new Set(project.nodes.map(node => node.unit));
    for (const target of desiredTargets) targets[target] = 'ready';

    for (const reference of project.units) {
      const path = resolveWorkspacePath(projectFile.path, reference.file);
      if (!activeUnitIds.has(reference.id)) continue;
      const file = workspace.files.find(item => item.path === path && item.role === 'unit');
      if (!file) {
        validation = 'blocked';
        diagnostics.push({ code: 'APG_UI_MISSING_UNIT', path, message: `Unit ${reference.id} is missing from this project package.` });
        for (const target of desiredTargets) targets[target] = 'blocked';
        continue;
      }
      const document = yaml.load(file.content);
      if (!isRecord(document)) throw new Error(`Unit "${path}" is not a valid document.`);
      const compatibility = isRecord(document.compatibility) ? document.compatibility : {};
      for (const target of desiredTargets) {
        if (compatibility[target] !== true) {
          targets[target] = 'blocked';
          diagnostics.push({
            code: 'APG_UI_TARGET_UNSUPPORTED',
            path,
            message: `${reference.id} does not declare ${target} compatibility.`,
          });
        }
      }
    }
  } catch (error) {
    validation = 'blocked';
    diagnostics.push({
      code: 'APG_UI_PREFLIGHT_ERROR',
      message: error instanceof Error ? error.message : 'Project readiness could not be evaluated.',
    });
  }

  return {
    checkedAt: new Date().toISOString(),
    validation,
    preview: validation === 'blocked' ? 'blocked' : previous.preview,
    targets,
    diagnostics,
  };
}
