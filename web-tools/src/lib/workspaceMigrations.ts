import yaml from 'js-yaml';

import type { WorkspaceFile } from './backendSamples';
import {
  APG_PACKAGE_SCHEMA,
  createApgProjectPackage,
  validateApgProjectPackage,
  type ApgProjectPackage,
  type StudioMode,
} from './projectPackage.ts';
import {
  WORKSPACE_FORMAT_VERSION,
  WORKSPACE_SCHEMA,
  validateWorkspacePayload,
  type WorkspacePayload,
} from './workspacePersistence.ts';

export type WorkspaceMigrationOptions = {
  id: string;
  name: string;
  entryProject?: string;
  mode?: StudioMode;
  now?: string;
};

export type LegacyWorkspaceStorage = {
  getItem: (key: string) => string | null;
};

export type MigratedBrowserWorkspace = {
  project: ApgProjectPackage;
  sourceKey: string;
  migratedSections: number;
  ambiguousSections: number;
};

export type RoutingMigrationHelpers = {
  panner: Pick<WorkspaceFile, 'content'>;
  mixer: Pick<WorkspaceFile, 'content'>;
};

export type RoutingMigrationResult = {
  workspace: WorkspacePayload;
  migratedSections: number;
  ambiguousSections: number;
};

export type ProjectRoutingMigrationResult = RoutingMigrationResult & {
  project: ApgProjectPackage;
};

type YamlRecord = Record<string, unknown>;

const PANNER_PATH = 'units/path_panner_2.unit.v2.yaml';
const MIXER_PATH = 'units/path_mixer_2.unit.v2.yaml';

function isRecord(value: unknown): value is YamlRecord {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function loadYamlRecord(content: string): YamlRecord | null {
  try {
    const value = yaml.load(content);
    return isRecord(value) ? value : null;
  } catch {
    return null;
  }
}

function normalizePath(path: string): string | null {
  const parts: string[] = [];
  for (const part of path.split('/')) {
    if (!part || part === '.') continue;
    if (part === '..') {
      if (parts.length === 0) return null;
      parts.pop();
    } else {
      parts.push(part);
    }
  }
  return parts.length > 0 ? parts.join('/') : null;
}

function resolveWorkspacePath(baseFile: string, reference: string): string | null {
  const base = baseFile.split('/');
  base.pop();
  return normalizePath([...base, ...reference.split('/')].join('/'));
}

function relativeWorkspaceReference(baseFile: string, targetFile: string): string {
  const base = baseFile.split('/');
  base.pop();
  const target = targetFile.split('/');
  while (base.length > 0 && target.length > 0 && base[0] === target[0]) {
    base.shift();
    target.shift();
  }
  return [...base.map(() => '..'), ...target].join('/');
}

function uniqueId(existing: Set<string>, base: string): string {
  let candidate = base;
  let suffix = 1;
  while (existing.has(candidate)) candidate = `${base}_${++suffix}`;
  existing.add(candidate);
  return candidate;
}

function uniqueSection(existing: Set<string>): string {
  let suffix = 1;
  while (existing.has(`parallel_${suffix}`)) suffix += 1;
  const section = `parallel_${suffix}`;
  existing.add(section);
  return section;
}

function levelDb(linear: number): number {
  if (!Number.isFinite(linear) || linear <= 0) return -60;
  return Math.max(-60, Math.min(6, 20 * Math.log10(linear)));
}

function formatDb(value: number): number {
  return Number(value.toFixed(4));
}

function ratioLevels(value: unknown): { path_1_db: number; path_2_db: number } {
  const raw = Number(value);
  const ratio = Number.isFinite(raw) ? Math.max(0, Math.min(1, raw)) : 0.5;
  return { path_1_db: formatDb(levelDb(1 - ratio)), path_2_db: formatDb(levelDb(ratio)) };
}

function endpointInstance(endpoint: unknown): string {
  return typeof endpoint === 'string' ? endpoint.split('.')[0] : '';
}

type LegacyPattern = {
  source: string;
  dryRoute: YamlRecord;
  branchRoute: YamlRecord;
  wetRoute: YamlRecord;
};

function findLegacyPattern(nodes: YamlRecord[], routes: YamlRecord[], mixerId: string): LegacyPattern | null {
  const dryRoutes = routes.filter(route => route.to === `${mixerId}.dry`);
  const wetRoutes = routes.filter(route => route.to === `${mixerId}.wet`);
  const outputRoutes = routes.filter(route => route.from === `${mixerId}.output`);
  if (dryRoutes.length !== 1 || wetRoutes.length !== 1 || outputRoutes.length !== 1) return null;
  const dryRoute = dryRoutes[0];
  const source = typeof dryRoute.from === 'string' ? dryRoute.from : '';
  const sourceRoutes = routes.filter(route => route.from === source);
  if (!source || sourceRoutes.length !== 2) return null;
  const branchRoute = sourceRoutes.find(route => route !== dryRoute);
  if (!branchRoute || branchRoute.to === `${mixerId}.wet`) return null;

  const nodeIds = new Set(nodes.map(node => String(node.id ?? '')));
  let target = typeof branchRoute.to === 'string' ? branchRoute.to : '';
  const visited = new Set<string>();
  while (target !== `${mixerId}.wet`) {
    const instance = endpointInstance(target);
    if (!instance || instance === 'system' || instance === mixerId || !nodeIds.has(instance) || visited.has(instance)) return null;
    visited.add(instance);
    const incoming = routes.filter(route => endpointInstance(route.to) === instance);
    const outgoing = routes.filter(route => endpointInstance(route.from) === instance);
    if (incoming.length !== 1 || outgoing.length !== 1) return null;
    const next = outgoing[0];
    target = typeof next.to === 'string' ? next.to : '';
  }
  if (visited.size === 0) return null;
  return { source, dryRoute, branchRoute, wetRoute: wetRoutes[0] };
}

function ensureHelperFile(files: WorkspacePayload['files'], path: string, content: string): boolean {
  const existing = files.find(file => file.path === path);
  if (existing) {
    const document = loadYamlRecord(existing.content);
    const expected = path === PANNER_PATH ? 'path_panner_2' : 'path_mixer_2';
    return document?.name === expected;
  }
  files.push({ path, role: 'unit', content });
  return true;
}

function ensureUnitReference(
  units: YamlRecord[],
  projectPath: string,
  helperPath: string,
  preferredId: string,
  usedIds: Set<string>,
): string {
  const reference = units.find(unit => (
    typeof unit.file === 'string' && resolveWorkspacePath(projectPath, unit.file) === helperPath
  ));
  if (reference) return String(reference.id);
  const id = uniqueId(usedIds, preferredId);
  units.push({ id, file: relativeWorkspaceReference(projectPath, helperPath) });
  return id;
}

export function migrateLegacyWetDryWorkspace(
  input: WorkspacePayload,
  helpers?: RoutingMigrationHelpers,
): RoutingMigrationResult {
  const workspace = structuredClone(validateWorkspacePayload(input));
  if (!helpers) return { workspace, migratedSections: 0, ambiguousSections: 0 };
  const filesByPath = new Map(workspace.files.map(file => [file.path, file]));
  let migratedSections = 0;
  let ambiguousSections = 0;

  for (const projectFile of workspace.files.filter(file => file.role === 'project')) {
    const document = loadYamlRecord(projectFile.content);
    if (!document) continue;
    const units = Array.isArray(document.units) ? document.units.filter(isRecord) : [];
    const chain = isRecord(document.chain) ? document.chain : null;
    const nodes = chain && Array.isArray(chain.nodes) ? chain.nodes.filter(isRecord) : [];
    const routes = chain && Array.isArray(chain.routes) ? chain.routes.filter(isRecord) : [];
    if (!chain) continue;

    const legacyUnitIds = new Set(units.flatMap(unit => {
      if (typeof unit.file !== 'string') return [];
      const resolved = resolveWorkspacePath(projectFile.path, unit.file);
      const unitDocument = resolved ? loadYamlRecord(filesByPath.get(resolved)?.content ?? '') : null;
      const legacy = unitDocument?.name === 'wet_dry_mix'
        || /wet_dry_mix\.unit\.v2\.yaml$/i.test(unit.file)
        || String(unit.id ?? '') === 'wet_dry_mix_unit';
      return legacy ? [String(unit.id ?? '')] : [];
    }));
    if (legacyUnitIds.size === 0) continue;

    const candidates = nodes.filter(node => legacyUnitIds.has(String(node.unit ?? '')));
    const patterns = candidates.map(node => ({ node, pattern: findLegacyPattern(nodes, routes, String(node.id ?? '')) }));
    ambiguousSections += patterns.filter(item => !item.pattern).length;
    if (!patterns.some(item => item.pattern)) continue;
    if (!ensureHelperFile(workspace.files, PANNER_PATH, helpers.panner.content)
      || !ensureHelperFile(workspace.files, MIXER_PATH, helpers.mixer.content)) {
      ambiguousSections += patterns.filter(item => item.pattern).length;
      continue;
    }

    const usedIds = new Set(units.map(unit => String(unit.id ?? '')));
    const pannerUnitId = ensureUnitReference(units, projectFile.path, PANNER_PATH, 'path_panner_2_unit', usedIds);
    const mixerUnitId = ensureUnitReference(units, projectFile.path, MIXER_PATH, 'path_mixer_2_unit', usedIds);
    const nodeIds = new Set(nodes.map(node => String(node.id ?? '')));
    const sectionIds = new Set(nodes.flatMap(node => (
      isRecord(node.routing) && typeof node.routing.section === 'string' ? [node.routing.section] : []
    )));
    const scenes = Array.isArray(document.scenes) ? document.scenes.filter(isRecord) : [];

    for (const { node: mixerNode, pattern } of patterns) {
      if (!pattern) continue;
      const mixerId = String(mixerNode.id ?? '');
      const pannerId = uniqueId(nodeIds, `${mixerId}_pan`);
      const section = uniqueSection(sectionIds);
      const mixerParams = isRecord(mixerNode.params) ? mixerNode.params : {};
      const mixRatio = mixerParams.mix ?? 0.5;
      const defaults = ratioLevels(mixRatio);
      delete mixerParams.mix;
      Object.assign(mixerParams, defaults);
      mixerNode.params = mixerParams;
      mixerNode.unit = mixerUnitId;
      mixerNode.routing = { section };

      const pannerNode: YamlRecord = {
        id: pannerId,
        unit: pannerUnitId,
        params: { path_1_db: 0, path_2_db: 0 },
        routing: { section },
      };
      const mixerIndex = nodes.indexOf(mixerNode);
      nodes.splice(mixerIndex < 0 ? nodes.length : mixerIndex, 0, pannerNode);

      pattern.dryRoute.to = `${pannerId}.input`;
      pattern.branchRoute.from = `${pannerId}.path_2`;
      pattern.wetRoute.to = `${mixerId}.path_2`;
      const dryIndex = routes.indexOf(pattern.dryRoute);
      routes.splice(dryIndex + 1, 0, { from: `${pannerId}.path_1`, to: `${mixerId}.path_1` });

      for (const scene of scenes) {
        const params = isRecord(scene.params) ? scene.params : {};
        const sceneMixKey = `${mixerId}.mix`;
        const levels = ratioLevels(sceneMixKey in params ? params[sceneMixKey] : mixRatio);
        delete params[sceneMixKey];
        params[`${mixerId}.path_1_db`] = levels.path_1_db;
        params[`${mixerId}.path_2_db`] = levels.path_2_db;
        scene.params = params;
        if (isRecord(scene.bypass)) delete scene.bypass[mixerId];
      }
      migratedSections += 1;
    }

    chain.nodes = nodes;
    chain.routes = routes;
    document.units = units.filter(unit => (
      !legacyUnitIds.has(String(unit.id ?? ''))
      || nodes.some(node => String(node.unit ?? '') === String(unit.id ?? ''))
    ));
    projectFile.content = yaml.dump(document, { lineWidth: 120, noRefs: true, quotingType: '"' });
  }

  return { workspace: validateWorkspacePayload(workspace), migratedSections, ambiguousSections };
}

export function migrateApgProjectRouting(
  input: ApgProjectPackage,
  helpers?: RoutingMigrationHelpers,
): ProjectRoutingMigrationResult {
  const project = validateApgProjectPackage(input);
  const result = migrateLegacyWetDryWorkspace(project.workspace, helpers);
  return { ...result, project: result.migratedSections > 0 ? { ...project, workspace: result.workspace } : project };
}

function parseUnknown(value: unknown): unknown {
  return typeof value === 'string' ? JSON.parse(value) : value;
}

function legacyFilesToWorkspace(value: unknown[], entryProject?: string): WorkspacePayload {
  const files = value as Array<Pick<WorkspaceFile, 'path' | 'role' | 'content'>>;
  const entry = entryProject ?? files.find(file => file?.role === 'project')?.path;
  return validateWorkspacePayload({
    schema: WORKSPACE_SCHEMA,
    version: WORKSPACE_FORMAT_VERSION,
    entryProject: entry,
    files,
  });
}

export function migrateWorkspaceValue(
  value: unknown,
  options: WorkspaceMigrationOptions,
  helpers?: RoutingMigrationHelpers,
): ApgProjectPackage {
  const parsed = parseUnknown(value);
  if (typeof parsed === 'object' && parsed !== null && !Array.isArray(parsed)) {
    const record = parsed as Record<string, unknown>;
    if (record.schema === APG_PACKAGE_SCHEMA) return migrateApgProjectRouting(validateApgProjectPackage(record), helpers).project;
  }
  const workspace = Array.isArray(parsed)
    ? legacyFilesToWorkspace(parsed, options.entryProject)
    : validateWorkspacePayload(parsed);
  const project = createApgProjectPackage(workspace, {
    id: options.id,
    name: options.name,
    mode: options.mode,
    createdAt: options.now,
    updatedAt: options.now,
  });
  return migrateApgProjectRouting(project, helpers).project;
}

export function findMigratableBrowserWorkspace(
  storage: LegacyWorkspaceStorage,
  options: WorkspaceMigrationOptions,
  keys = ['apg.unit-editor.workspace.v2', 'apg.unit-editor.workspace.v1'],
  helpers?: RoutingMigrationHelpers,
): MigratedBrowserWorkspace | null {
  for (const sourceKey of keys) {
    const value = storage.getItem(sourceKey);
    if (!value) continue;
    const result = migrateApgProjectRouting(migrateWorkspaceValue(value, options), helpers);
    return {
      project: result.project,
      sourceKey,
      migratedSections: result.migratedSections,
      ambiguousSections: result.ambiguousSections,
    };
  }
  return null;
}
