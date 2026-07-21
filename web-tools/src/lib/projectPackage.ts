import type { WorkspaceFile } from './backendSamples';
import {
  createWorkspacePayload,
  hydrateWorkspaceFiles,
  validateWorkspacePayload,
  type WorkspacePayload,
} from './workspacePersistence.ts';

export const APG_PACKAGE_SCHEMA = 'apg.project.package.v2';
export const APG_PACKAGE_VERSION = 2;
export const LEGACY_APG_PACKAGE_SCHEMA = 'apg.project.package.v1';

export type StudioView = 'effect-chain' | 'atom-chain';
// Kept as an alias while component props migrate from the old Simple/Pro naming.
export type StudioMode = StudioView;
export type ReadinessStatus = 'unknown' | 'ready' | 'blocked';

export type EffectChainEffectDraft = {
  kind: 'effect';
  instanceId: string;
};

export type EffectChainPathDraft = {
  id: string;
  port: string;
  levelParam: string;
  rail: EffectChainRailDraft;
};

export type EffectChainParallelDraft = {
  kind: 'parallel';
  id: string;
  section: string;
  pannerInstanceId: string | null;
  mixerInstanceId: string | null;
  storedPannerInstanceId: string;
  storedMixerInstanceId: string;
  paths: EffectChainPathDraft[];
};

export type EffectChainItemDraft = EffectChainEffectDraft | EffectChainParallelDraft;

export type EffectChainRailDraft = {
  id: string;
  items: EffectChainItemDraft[];
};

export type EffectChainEditorDraft = {
  version: 1;
  root: EffectChainRailDraft;
};

export type ApgEditorState = {
  activeView: StudioView;
  activeLibraryUnitId: string | null;
  effectChain: EffectChainEditorDraft | null;
};

export type ProjectReadinessSnapshot = {
  checkedAt: string | null;
  validation: ReadinessStatus;
  preview: ReadinessStatus;
  targets: Record<string, ReadinessStatus>;
  diagnostics: Array<{ code?: string; path?: string; message: string }>;
};

export type ApgAudioAsset = {
  id: string;
  name: string;
  mimeType: string;
  channels: 1;
  sampleRate: number | null;
  durationSeconds: number | null;
  encoding: 'base64';
  data: string;
};

export type ApgPackageManifest = {
  id: string;
  name: string;
  description: string;
  createdAt: string;
  updatedAt: string;
  lastMode: StudioMode;
};

export type ApgProjectPackage = {
  schema: typeof APG_PACKAGE_SCHEMA;
  version: typeof APG_PACKAGE_VERSION;
  manifest: ApgPackageManifest;
  workspace: WorkspacePayload;
  audio: ApgAudioAsset[];
  readiness: ProjectReadinessSnapshot;
  editor: ApgEditorState;
};

export type CreateApgPackageOptions = {
  id: string;
  name: string;
  description?: string;
  mode?: StudioMode;
  createdAt?: string;
  updatedAt?: string;
  audio?: ApgAudioAsset[];
  readiness?: ProjectReadinessSnapshot;
};

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function requiredString(value: unknown, field: string): string {
  if (typeof value !== 'string' || value.trim() === '') throw new Error(`${field} must be a non-empty string.`);
  return value;
}

function optionalFiniteNumber(value: unknown, field: string): number | null {
  if (value === null || value === undefined) return null;
  if (typeof value !== 'number' || !Number.isFinite(value) || value < 0) throw new Error(`${field} must be a positive number.`);
  return value;
}

function readinessStatus(value: unknown, field: string): ReadinessStatus {
  if (value !== 'unknown' && value !== 'ready' && value !== 'blocked') throw new Error(`${field} is invalid.`);
  return value;
}

function nullableString(value: unknown, field: string): string | null {
  if (value === null || value === undefined) return null;
  return requiredString(value, field);
}

function validateEffectChainRail(value: unknown, field: string, depth = 0): EffectChainRailDraft {
  if (depth > 32) throw new Error(`${field} nesting is too deep.`);
  if (!isRecord(value) || !Array.isArray(value.items)) throw new Error(`${field} must be a rail.`);
  const id = requiredString(value.id, `${field}.id`);
  return {
    id,
    items: value.items.map((item, index): EffectChainItemDraft => {
      const itemField = `${field}.items[${index}]`;
      if (!isRecord(item)) throw new Error(`${itemField} must be an object.`);
      if (item.kind === 'effect') {
        return { kind: 'effect', instanceId: requiredString(item.instanceId, `${itemField}.instanceId`) };
      }
      if (item.kind !== 'parallel' || !Array.isArray(item.paths)) {
        throw new Error(`${itemField} must be an effect or parallel section.`);
      }
      return {
        kind: 'parallel',
        id: requiredString(item.id, `${itemField}.id`),
        section: requiredString(item.section, `${itemField}.section`),
        pannerInstanceId: nullableString(item.pannerInstanceId, `${itemField}.pannerInstanceId`),
        mixerInstanceId: nullableString(item.mixerInstanceId, `${itemField}.mixerInstanceId`),
        storedPannerInstanceId: requiredString(item.storedPannerInstanceId, `${itemField}.storedPannerInstanceId`),
        storedMixerInstanceId: requiredString(item.storedMixerInstanceId, `${itemField}.storedMixerInstanceId`),
        paths: item.paths.map((path, pathIndex) => {
          const pathField = `${itemField}.paths[${pathIndex}]`;
          if (!isRecord(path)) throw new Error(`${pathField} must be an object.`);
          return {
            id: requiredString(path.id, `${pathField}.id`),
            port: requiredString(path.port, `${pathField}.port`),
            levelParam: requiredString(path.levelParam, `${pathField}.levelParam`),
            rail: validateEffectChainRail(path.rail, `${pathField}.rail`, depth + 1),
          };
        }),
      };
    }),
  };
}

function validateEditorState(value: unknown, fallbackView: StudioView): ApgEditorState {
  if (value === undefined || value === null) {
    return { activeView: fallbackView, activeLibraryUnitId: null, effectChain: null };
  }
  if (!isRecord(value)) throw new Error('Project editor state must be an object.');
  const activeView = value.activeView;
  if (activeView !== 'effect-chain' && activeView !== 'atom-chain') {
    throw new Error('Project editor active view is invalid.');
  }
  let effectChain: EffectChainEditorDraft | null = null;
  if (value.effectChain !== null && value.effectChain !== undefined) {
    if (!isRecord(value.effectChain) || value.effectChain.version !== 1) {
      throw new Error('Effect Chain editor state must use version 1.');
    }
    effectChain = { version: 1, root: validateEffectChainRail(value.effectChain.root, 'editor.effectChain.root') };
  }
  return {
    activeView,
    activeLibraryUnitId: nullableString(value.activeLibraryUnitId, 'editor.activeLibraryUnitId'),
    effectChain,
  };
}

export function createUnknownReadiness(): ProjectReadinessSnapshot {
  return { checkedAt: null, validation: 'unknown', preview: 'unknown', targets: {}, diagnostics: [] };
}

function validateReadiness(value: unknown): ProjectReadinessSnapshot {
  if (!isRecord(value)) throw new Error('Project readiness must be an object.');
  if (!isRecord(value.targets)) throw new Error('Project readiness targets must be an object.');
  if (!Array.isArray(value.diagnostics)) throw new Error('Project readiness diagnostics must be an array.');
  return {
    checkedAt: value.checkedAt === null ? null : requiredString(value.checkedAt, 'Project readiness checkedAt'),
    validation: readinessStatus(value.validation, 'Project readiness validation'),
    preview: readinessStatus(value.preview, 'Project readiness preview'),
    targets: Object.fromEntries(Object.entries(value.targets).map(([target, status]) => [
      target,
      readinessStatus(status, `Project readiness target ${target}`),
    ])),
    diagnostics: value.diagnostics.map((diagnostic, index) => {
      if (!isRecord(diagnostic)) throw new Error(`Project readiness diagnostic ${index} must be an object.`);
      return {
        ...(typeof diagnostic.code === 'string' ? { code: diagnostic.code } : {}),
        ...(typeof diagnostic.path === 'string' ? { path: diagnostic.path } : {}),
        message: requiredString(diagnostic.message, `Project readiness diagnostic ${index} message`),
      };
    }),
  };
}

export function validateAudioAsset(value: unknown): ApgAudioAsset {
  if (!isRecord(value)) throw new Error('Audio asset must be an object.');
  if (value.channels !== 1) throw new Error('Audio Playground project packages only support mono audio assets.');
  if (value.encoding !== 'base64') throw new Error('Audio asset encoding must be base64.');
  return {
    id: requiredString(value.id, 'Audio asset id'),
    name: requiredString(value.name, 'Audio asset name'),
    mimeType: requiredString(value.mimeType, 'Audio asset mimeType'),
    channels: 1,
    sampleRate: optionalFiniteNumber(value.sampleRate, 'Audio asset sampleRate'),
    durationSeconds: optionalFiniteNumber(value.durationSeconds, 'Audio asset durationSeconds'),
    encoding: 'base64',
    data: typeof value.data === 'string' ? value.data : (() => { throw new Error('Audio asset data must be a string.'); })(),
  };
}

export function encodeBytesBase64(bytes: Uint8Array): string {
  let binary = '';
  for (let index = 0; index < bytes.length; index += 1) binary += String.fromCharCode(bytes[index]);
  return btoa(binary);
}

export function decodeBytesBase64(value: string): Uint8Array {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index += 1) bytes[index] = binary.charCodeAt(index);
  return bytes;
}

export function createMonoAudioAsset(
  input: Omit<ApgAudioAsset, 'channels' | 'encoding' | 'data'> & { channels: number; bytes: Uint8Array },
): ApgAudioAsset {
  if (input.channels !== 1) throw new Error('Stereo and multi-channel files are not supported. Choose a mono audio file.');
  return validateAudioAsset({ ...input, channels: 1, encoding: 'base64', data: encodeBytesBase64(input.bytes) });
}

export function createApgProjectPackage(
  workspace: WorkspacePayload,
  options: CreateApgPackageOptions,
): ApgProjectPackage {
  const createdAt = options.createdAt ?? new Date().toISOString();
  return validateApgProjectPackage({
    schema: APG_PACKAGE_SCHEMA,
    version: APG_PACKAGE_VERSION,
    manifest: {
      id: options.id,
      name: options.name,
      description: options.description ?? '',
      createdAt,
      updatedAt: options.updatedAt ?? createdAt,
      lastMode: options.mode ?? 'effect-chain',
    },
    workspace,
    audio: options.audio ?? [],
    readiness: options.readiness ?? createUnknownReadiness(),
    editor: {
      activeView: options.mode ?? 'effect-chain',
      activeLibraryUnitId: null,
      effectChain: null,
    },
  });
}

export function createApgProjectPackageFromFiles(
  entryProject: string,
  files: WorkspaceFile[],
  options: CreateApgPackageOptions,
): ApgProjectPackage {
  return createApgProjectPackage(createWorkspacePayload(entryProject, files), options);
}

export function validateApgProjectPackage(value: unknown): ApgProjectPackage {
  if (!isRecord(value)) throw new Error('APG project package must be an object.');
  if (value.schema === LEGACY_APG_PACKAGE_SCHEMA && value.version === 1) {
    if (!isRecord(value.manifest)) throw new Error('APG project package manifest must be an object.');
    return validateApgProjectPackage({
      ...value,
      schema: APG_PACKAGE_SCHEMA,
      version: APG_PACKAGE_VERSION,
      manifest: { ...value.manifest, lastMode: 'effect-chain' },
      editor: {
        activeView: 'effect-chain',
        activeLibraryUnitId: null,
        effectChain: null,
      },
    });
  }
  if (value.schema !== APG_PACKAGE_SCHEMA || value.version !== APG_PACKAGE_VERSION) {
    throw new Error(`APG project package must use ${APG_PACKAGE_SCHEMA} version ${APG_PACKAGE_VERSION}.`);
  }
  if (!isRecord(value.manifest)) throw new Error('APG project package manifest must be an object.');
  const mode = value.manifest.lastMode;
  if (mode !== 'effect-chain' && mode !== 'atom-chain') throw new Error('APG project package view is invalid.');
  if (!Array.isArray(value.audio)) throw new Error('APG project package audio must be an array.');
  const audio = value.audio.map(validateAudioAsset);
  const audioIds = new Set<string>();
  for (const asset of audio) {
    if (audioIds.has(asset.id)) throw new Error(`Audio asset id "${asset.id}" is duplicated.`);
    audioIds.add(asset.id);
  }
  const editor = validateEditorState(value.editor, mode);
  return {
    schema: APG_PACKAGE_SCHEMA,
    version: APG_PACKAGE_VERSION,
    manifest: {
      id: requiredString(value.manifest.id, 'APG project id'),
      name: requiredString(value.manifest.name, 'APG project name'),
      description: typeof value.manifest.description === 'string' ? value.manifest.description : '',
      createdAt: requiredString(value.manifest.createdAt, 'APG project createdAt'),
      updatedAt: requiredString(value.manifest.updatedAt, 'APG project updatedAt'),
      lastMode: mode,
    },
    workspace: validateWorkspacePayload(value.workspace),
    audio,
    readiness: validateReadiness(value.readiness),
    editor,
  };
}

export function serializeApgProjectPackage(value: ApgProjectPackage): string {
  return JSON.stringify(validateApgProjectPackage(value), null, 2);
}

export function parseApgProjectPackage(value: string): ApgProjectPackage {
  return validateApgProjectPackage(JSON.parse(value));
}

export function hydrateApgProjectFiles(value: ApgProjectPackage, originals: WorkspaceFile[] = []): WorkspaceFile[] {
  return hydrateWorkspaceFiles(value.workspace, originals);
}

export function apgPackageFileName(value: ApgProjectPackage): string {
  const slug = value.manifest.name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '') || 'project';
  return `${slug}.apg`;
}
