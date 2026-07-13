import type { WorkspaceFile } from './backendSamples';

export const WORKSPACE_SCHEMA = 'apg.ui.workspace.v2';
export const WORKSPACE_FORMAT_VERSION = 2;

export type PersistedWorkspaceFile = Pick<WorkspaceFile, 'path' | 'role' | 'content'>;

export type WorkspacePayload = {
  schema: typeof WORKSPACE_SCHEMA;
  version: typeof WORKSPACE_FORMAT_VERSION;
  entryProject: string;
  files: PersistedWorkspaceFile[];
};

function normalizedPath(path: string): string | null {
  if (!path || path.startsWith('/') || path.includes('\\') || path.includes(':')) return null;
  const result: string[] = [];
  for (const segment of path.split('/')) {
    if (!segment || segment === '.') continue;
    if (segment === '..') {
      if (result.length === 0) return null;
      result.pop();
    } else {
      result.push(segment);
    }
  }
  return result.length > 0 ? result.join('/') : null;
}

export function createWorkspacePayload(entryProject: string, files: WorkspaceFile[]): WorkspacePayload {
  return validateWorkspacePayload({
    schema: WORKSPACE_SCHEMA,
    version: WORKSPACE_FORMAT_VERSION,
    entryProject,
    files: files.map(({ path, role, content }) => ({ path, role, content })),
  });
}

export function validateWorkspacePayload(value: unknown): WorkspacePayload {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) throw new Error('Workspace payload must be an object.');
  const raw = value as Partial<WorkspacePayload>;
  if (raw.schema !== WORKSPACE_SCHEMA || raw.version !== WORKSPACE_FORMAT_VERSION) {
    throw new Error(`Workspace payload must use ${WORKSPACE_SCHEMA} format version ${WORKSPACE_FORMAT_VERSION}.`);
  }
  if (!Array.isArray(raw.files) || raw.files.length === 0) throw new Error('Workspace payload must contain files.');
  const seen = new Set<string>();
  const files = raw.files.map((file, index) => {
    if (typeof file !== 'object' || file === null || Array.isArray(file)) throw new Error(`Workspace file ${index} is invalid.`);
    const candidate = file as Partial<PersistedWorkspaceFile>;
    const path = typeof candidate.path === 'string' ? normalizedPath(candidate.path) : null;
    if (!path || path !== candidate.path) throw new Error(`Workspace file ${index} has an invalid path.`);
    if (seen.has(path)) throw new Error(`Workspace file path "${path}" is duplicated.`);
    seen.add(path);
    if (candidate.role !== 'project' && candidate.role !== 'unit') throw new Error(`Workspace file "${path}" has an invalid role.`);
    if (typeof candidate.content !== 'string') throw new Error(`Workspace file "${path}" has invalid content.`);
    return { path, role: candidate.role, content: candidate.content };
  });
  const entryProject = typeof raw.entryProject === 'string' ? normalizedPath(raw.entryProject) : null;
  const entry = entryProject ? files.find(file => file.path === entryProject) : null;
  if (!entry || entry.role !== 'project') throw new Error('Workspace entry project is missing or is not a project file.');
  return { schema: WORKSPACE_SCHEMA, version: WORKSPACE_FORMAT_VERSION, entryProject: entry.path, files };
}

export function parseWorkspacePayload(text: string): WorkspacePayload {
  return validateWorkspacePayload(JSON.parse(text));
}

export function hydrateWorkspaceFiles(payload: WorkspacePayload, initialFiles: WorkspaceFile[]): WorkspaceFile[] {
  const originals = new Map(initialFiles.map(file => [file.path, file]));
  return payload.files.map(file => ({
    ...file,
    originalContent: originals.get(file.path)?.originalContent ?? '',
  }));
}
