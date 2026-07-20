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
};

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

export function migrateWorkspaceValue(value: unknown, options: WorkspaceMigrationOptions): ApgProjectPackage {
  const parsed = parseUnknown(value);
  if (typeof parsed === 'object' && parsed !== null && !Array.isArray(parsed)) {
    const record = parsed as Record<string, unknown>;
    if (record.schema === APG_PACKAGE_SCHEMA) return validateApgProjectPackage(record);
  }
  const workspace = Array.isArray(parsed)
    ? legacyFilesToWorkspace(parsed, options.entryProject)
    : validateWorkspacePayload(parsed);
  return createApgProjectPackage(workspace, {
    id: options.id,
    name: options.name,
    mode: options.mode,
    createdAt: options.now,
    updatedAt: options.now,
  });
}

export function findMigratableBrowserWorkspace(
  storage: LegacyWorkspaceStorage,
  options: WorkspaceMigrationOptions,
  keys = ['apg.unit-editor.workspace.v2', 'apg.unit-editor.workspace.v1'],
): MigratedBrowserWorkspace | null {
  for (const sourceKey of keys) {
    const value = storage.getItem(sourceKey);
    if (!value) continue;
    return { project: migrateWorkspaceValue(value, options), sourceKey };
  }
  return null;
}
