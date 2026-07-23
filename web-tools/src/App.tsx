import { Profiler, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useEdgesState, useNodesState, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { AppLogo } from './components/AppLogo';
import { AtomCatalogInfo, AtomCatalogPanel } from './components/AtomCatalogPanel';
import { AtomContextInspector } from './components/AtomContextInspector';
import { ContractGraphCanvas } from './components/ContractGraphCanvas';
import { GuidedTour } from './components/GuidedTour';
import { LiveLatencyBadge } from './components/LiveLatencyBadge';
import { ProjectCanvas } from './components/ProjectCanvas';
import type { ProjectParallelOption, ProjectReplacementOption } from './components/ProjectCanvas';
import { ProjectTopbar } from './components/ProjectTopbar';
import { SceneBar } from './components/SceneBar';
import { SimpleInspector } from './components/SimpleInspector';
import { SimpleLibraryPanel, type EffectLibraryItem } from './components/SimpleLibraryPanel';
import { UnitSettingsDrawer } from './components/UnitSettingsDrawer';
import {
  backendSamples,
  initialWorkspaceFiles,
  pathMixer2WorkspaceFile,
  pathPanner2WorkspaceFile,
  type WorkspaceFile,
} from './lib/backendSamples';
import { buildProjectGraph, type ProjectNodeData, type ProjectParamControl } from './lib/projectGraph';
import { LiveBypassContext, type LiveBypassController } from './lib/liveBypass';
import {
  buildParamDrafts,
  buildParamOriginals,
  buildParamOverridesFromOriginals,
  paramDraftKey,
} from './lib/projectParams';
import {
  addProjectInstance,
  addProjectRoute,
  addProjectUnitReference,
  applyProjectScene,
  copyProjectInstance,
  duplicateProjectInstance,
  insertProjectParallelOnRoute,
  insertProjectInstanceOnRoute,
  moveProjectInstanceOnRoute,
  parseProjectGraphDraft,
  parseUnitPortNames,
  pasteProjectInstance,
  projectDraftToInspect,
  removeEmptyProjectRoutingSection,
  removeProjectInstanceWithTopology,
  removeProjectScene,
  rebindProjectInstanceUnit,
  renameProjectInstance,
  renameProjectScene,
  replaceProjectInstance,
  syncProjectUnitContract,
  upsertProjectScene,
  type ProjectPortCatalog,
  type ProjectInstanceClipboard,
  type ProjectRouteDraft,
} from './lib/projectV2Graph';
import {
  parseApgProjectPackage,
  type ApgProjectPackage,
  type StudioMode,
} from './lib/projectPackage';
import { migrateApgProjectRouting, migrateLegacyWetDryWorkspace } from './lib/workspaceMigrations';
import {
  createPersonalPreset,
  listPresetsForUnit,
  type PersonalUnitRecord,
  type UnitPreset,
} from './lib/presetLibrary';
import {
  addAtomNodeToUnit,
  assertUserPlaceableUnit,
  classifyUserEffectContent,
  connectUnitNodes,
  disconnectUnitInput,
  insertAtomNodeOnConnection,
  moveUnitParam,
  pasteAtomNodeIntoUnit,
  parseUnitGraphDraft,
  removeAtomNodeWithTopology,
  replaceAtomNodeInUnit,
  reconnectUnitConnection,
  serializeUnitGraphNodeUpdate,
  setAtomNodePosition,
  setAtomNodePositions,
  updateUnitDefinition,
  updateProjectInstanceParam,
  type GraphPosition as UnitGraphPosition,
  type UnitGraphNode,
  type UnitConnectionEndpoint,
} from './lib/unitV2Graph';
import {
  createWorkspacePayload,
  hydrateWorkspaceFiles,
  parseWorkspacePayload,
  persistWorkspacePayload,
  validateWorkspacePayload,
  WORKSPACE_FORMAT_VERSION,
  WORKSPACE_SCHEMA,
  type WorkspacePayload,
} from './lib/workspacePersistence';
import { incrementPerfCounter, markPerfSpan, markRenderPerfSpan } from './lib/perfTelemetry';
import type { ProjectLibraryPointerDrag } from './lib/graphDragTypes';
import './App.css';

function findUnitNode(nodes: Node<ProjectNodeData>[], id: string | null): ProjectNodeData | null {
  if (!id) return null;
  return nodes.find(node => node.id === id)?.data ?? null;
}

function uniqueInstanceId(existing: string[], unitId: string): string {
  const normalized = unitId
    .replace(/_unit$/, '')
    .replace(/[^a-z0-9_]+/g, '_')
    .replace(/^_+/, '')
    || 'unit';
  const base = /^[a-z]/.test(normalized) ? normalized : `unit_${normalized}`;
  let suffix = 1;
  let candidate = base;
  const used = new Set(existing);
  while (used.has(candidate)) candidate = `${base}_${++suffix}`;
  return candidate;
}

function withoutInstanceValues(values: Record<string, string>, instanceId: string): Record<string, string> {
  return Object.fromEntries(Object.entries(values).filter(([key]) => !key.startsWith(`${instanceId}.`)));
}

type InspectorView = 'project' | 'atom' | 'contract';
type CanvasMode = 'project' | 'contract';
type WorkspaceHistoryEntry = {
  entryProject: string;
  files: WorkspaceFile[];
  activeContractUnit: PersonalUnitRecord | null;
  selectedWorkspacePath: string;
  selectedId: string | null;
  selectedRouteIndex: number | null;
  selectedAtomId: string | null;
  canvasMode: CanvasMode;
  inspectorView: InspectorView;
};

const WORKSPACE_STORAGE_KEY = 'apg.unit-editor.workspace.v2';
const LEGACY_WORKSPACE_STORAGE_KEY = 'apg.unit-editor.workspace.v1';
const PROJECT_ROUTE = '/projects';
const ROUTING_MIGRATION_HELPERS = { panner: pathPanner2WorkspaceFile, mixer: pathMixer2WorkspaceFile };

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

function unitRouteId(path: string): string {
  const filename = path.split('/').at(-1) ?? path;
  return filename.replace(/\.unit\.v2\.yaml$/i, '');
}

function unitRoute(path: string): string {
  return `/unit/${encodeURIComponent(unitRouteId(path))}`;
}

function loadWorkspaceState(projectPackage?: ApgProjectPackage): { entryProject: string; files: WorkspaceFile[] } {
  if (projectPackage) {
    const migrated = migrateApgProjectRouting(projectPackage, ROUTING_MIGRATION_HELPERS).project;
    return {
      entryProject: migrated.workspace.entryProject,
      files: hydrateWorkspaceFiles(migrated.workspace, initialWorkspaceFiles),
    };
  }
  const fallback = { entryProject: backendSamples.project.file, files: initialWorkspaceFiles };
  if (typeof window === 'undefined') return fallback;
  try {
    const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
    if (saved) {
      const result = migrateLegacyWetDryWorkspace(parseWorkspacePayload(saved), ROUTING_MIGRATION_HELPERS);
      return { entryProject: result.workspace.entryProject, files: hydrateWorkspaceFiles(result.workspace, initialWorkspaceFiles) };
    }
    const legacy = window.localStorage.getItem(LEGACY_WORKSPACE_STORAGE_KEY);
    if (legacy) {
      const files = JSON.parse(legacy) as Array<Pick<WorkspaceFile, 'path' | 'role' | 'content'>>;
      const payload = validateWorkspacePayload({
        schema: WORKSPACE_SCHEMA,
        version: WORKSPACE_FORMAT_VERSION,
        entryProject: backendSamples.project.file,
        files,
      });
      window.localStorage.removeItem(LEGACY_WORKSPACE_STORAGE_KEY);
      const result = migrateLegacyWetDryWorkspace(payload, ROUTING_MIGRATION_HELPERS);
      return {
        entryProject: result.workspace.entryProject,
        files: hydrateWorkspaceFiles(result.workspace, initialWorkspaceFiles),
      };
    }
    return fallback;
  } catch {
    return fallback;
  }
}

type EditorWorkspaceProps = {
  projectPackage: ApgProjectPackage;
  mode: StudioMode;
  personalPresets: UnitPreset[];
  personalUnits: PersonalUnitRecord[];
  onModeChange: (mode: StudioMode) => void;
  onHome: () => void;
  onDeletePersonalUnit: (id: string) => void;
  onDeletePreset: (id: string) => void;
  onExportProject: (workspace: WorkspacePayload) => void;
  onProjectPackageChange: (update: (project: ApgProjectPackage) => ApgProjectPackage) => void;
  onSavePersonalUnit: (unit: PersonalUnitRecord) => Promise<void>;
  onSavePreset: (preset: UnitPreset) => void;
  onWorkspaceChange: (workspace: WorkspacePayload) => void;
};

const effectLibraryCopy: Record<string, Omit<EffectLibraryItem, 'id' | 'scope' | 'recordId'>> = {
  noise_gate: { title: 'Noise Gate', category: 'dynamics', description: 'Tames background noise between notes.' },
  phaser: { title: 'Phaser', category: 'modulation', description: 'Adds a moving, liquid sweep.' },
  overdrive: { title: 'Overdrive', category: 'drive', description: 'Warm saturation and extra bite.' },
  tone_stack: { title: 'Amp & Tone', category: 'amp', description: 'Shapes gain, EQ, presence, and level.' },
  tremolo: { title: 'Tremolo', category: 'modulation', description: 'Creates a rhythmic volume pulse.' },
  chorus: { title: 'Chorus', category: 'modulation', description: 'Adds width and gentle movement.' },
  delay: { title: 'Delay', category: 'delay', description: 'Repeats notes with feedback and blend.' },
  schroeder_reverb: { title: 'Reverb', category: 'reverb', description: 'Places the sound in a smooth room.' },
};

const builtInSimpleEffectLibrary: EffectLibraryItem[] = initialWorkspaceFiles
  .filter(file => file.role === 'unit')
  .map(file => {
    const id = file.path.split('/').at(-1)?.replace(/\.unit\.v2\.yaml$/i, '') ?? file.path;
    return {
      id,
      ...(effectLibraryCopy[id] ?? { title: id.replace(/_/g, ' '), category: 'other', description: 'Custom effect.' }),
      scope: 'built-in' as const,
    };
  });

function libraryWorkspaceSource(item: EffectLibraryItem, personalUnits: PersonalUnitRecord[]): WorkspaceFile | null {
  if (item.scope === 'personal') {
    const personal = personalUnits.find(unit => unit.id === item.recordId);
    if (!personal) return null;
    const path = `personal/${personal.name}.unit.v2.yaml`;
    return { path, role: 'unit', content: personal.content, originalContent: personal.content };
  }
  return initialWorkspaceFiles.find(file => (
    file.role === 'unit' && file.path.endsWith(`/${item.id}.unit.v2.yaml`)
  )) ?? null;
}

function personalUnitWorkspacePath(unit: PersonalUnitRecord): string {
  return `personal/${unit.name}.unit.v2.yaml`;
}

function personalUnitFromRoute(pathname: string, units: PersonalUnitRecord[]): PersonalUnitRecord | null {
  const match = /^\/unit\/([^/]+)\/?$/.exec(pathname);
  if (!match) return null;
  try {
    const routeId = decodeURIComponent(match[1]);
    return units.find(unit => unitRouteId(personalUnitWorkspacePath(unit)) === routeId) ?? null;
  } catch {
    return null;
  }
}

function uniquePersonalUnitName(seed: string, units: PersonalUnitRecord[]): string {
  const normalized = seed
    .toLowerCase()
    .replace(/[^a-z0-9_]+/g, '_')
    .replace(/^_+|_+$/g, '') || 'effect';
  const base = `${/^[a-z]/.test(normalized) ? normalized : `effect_${normalized}`}_copy`;
  const used = new Set(units.map(unit => unit.name));
  let name = base;
  let suffix = 2;
  while (used.has(name)) name = `${base}_${suffix++}`;
  return name;
}

function createPersonalUnitCopy(
  item: Pick<EffectLibraryItem, 'id' | 'title' | 'category' | 'description'>,
  content: string,
  units: PersonalUnitRecord[],
): PersonalUnitRecord {
  const name = uniquePersonalUnitName(item.id, units);
  const title = `${item.title} Copy`;
  const copiedContent = updateUnitDefinition(content, {
    name,
    title,
    category: item.category,
    description: item.description,
  });
  assertUserPlaceableUnit(copiedContent);
  const now = new Date().toISOString();
  return {
    schema: 'apg.personal-unit.v1',
    version: 1,
    id: globalThis.crypto?.randomUUID?.() ?? `unit-${Date.now()}`,
    name,
    title,
    category: item.category,
    description: item.description,
    content: copiedContent,
    createdAt: now,
    updatedAt: now,
  };
}

export function EditorWorkspace({
  projectPackage,
  mode,
  personalPresets,
  personalUnits,
  onModeChange,
  onHome,
  onDeletePersonalUnit,
  onDeletePreset,
  onExportProject,
  onProjectPackageChange,
  onSavePersonalUnit,
  onSavePreset,
  onWorkspaceChange,
}: EditorWorkspaceProps) {
  const location = useLocation();
  const navigate = useNavigate();
  const [runtimeReady, setRuntimeReady] = useState(false);
  const [initialWorkspace] = useState(() => loadWorkspaceState(projectPackage));
  const [tourOpen, setTourOpen] = useState(() => (
    mode === 'simple' && typeof window !== 'undefined' && !window.localStorage.getItem('apg.studio.tour.v1')
  ));
  const simpleEffectLibrary = useMemo<EffectLibraryItem[]>(() => {
    const items: EffectLibraryItem[] = [
      ...builtInSimpleEffectLibrary,
      ...personalUnits.map(unit => ({
        id: unit.name,
        title: unit.title,
        category: unit.category,
        description: unit.description,
        scope: 'personal' as const,
        recordId: unit.id,
      })),
    ];
    return items.map(item => {
      const source = libraryWorkspaceSource(item, personalUnits);
      if (!source) return { ...item, placementError: 'Unit source is unavailable.' };
      try {
        const policy = classifyUserEffectContent(source.content);
        return policy.userPlaceable ? item : { ...item, placementError: policy.reason ?? 'Not a mono effect.' };
      } catch {
        return { ...item, placementError: 'Unit ports are invalid.' };
      }
    });
  }, [personalUnits]);
  const initialContractUnit = personalUnitFromRoute(location.pathname, personalUnits);
  const initialProjectInspect = useMemo(() => {
    try {
      const file = initialWorkspace.files.find(item => item.path === initialWorkspace.entryProject);
      if (!file) return backendSamples.project;
      return projectDraftToInspect(parseProjectGraphDraft(file.content), backendSamples.project, file.path);
    } catch {
      return backendSamples.project;
    }
  }, [initialWorkspace]);
  const initialProjectDraft = useMemo(() => {
    try {
      const file = initialWorkspace.files.find(item => item.path === initialWorkspace.entryProject);
      return file ? parseProjectGraphDraft(file.content) : undefined;
    } catch {
      return undefined;
    }
  }, [initialWorkspace]);
  const initialGraph = useMemo(() => buildProjectGraph(initialProjectInspect), [initialProjectInspect]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initialGraph.edges);
  const [liveBypassController, setLiveBypassController] = useState<LiveBypassController | null>(null);
  const liveBypassControllerRef = useRef<LiveBypassController | null>(liveBypassController);
  liveBypassControllerRef.current = liveBypassController;
  const setProjectNodeBypass = useCallback(async (instanceId: string, enabled: boolean) => {
    await liveBypassControllerRef.current?.setBypass(instanceId, enabled);
  }, []);
  const nodeBypassByInstance = liveBypassController?.bypassByInstance;
  const nodeBypassAvailable = liveBypassController !== null;
  const liveBypassContextValue = useMemo(
    () => ({ controller: liveBypassController, setController: setLiveBypassController }),
    [liveBypassController],
  );
  const [selectedId, setSelectedId] = useState<string | null>(() => (
    initialProjectInspect.nodes[0] ? `unit-${initialProjectInspect.nodes[0].id}` : null
  ));
  const [activeScene, setActiveScene] = useState<string | null>(null);
  const [selectedRouteIndex, setSelectedRouteIndex] = useState<number | null>(null);
  const [inspectorView, setInspectorView] = useState<InspectorView>(initialContractUnit ? 'atom' : 'project');
  const [canvasMode, setCanvasMode] = useState<CanvasMode>(initialContractUnit ? 'contract' : 'project');
  const [activeContractUnit, setActiveContractUnit] = useState<PersonalUnitRecord | null>(initialContractUnit);
  const activeContractUnitRef = useRef<PersonalUnitRecord | null>(initialContractUnit);
  const selectActiveContractUnit = useCallback((unit: PersonalUnitRecord | null) => {
    activeContractUnitRef.current = unit;
    setActiveContractUnit(unit);
  }, []);
  const [unitSettingsOpen, setUnitSettingsOpen] = useState(false);
  const openUnitSettings = useCallback(() => setUnitSettingsOpen(true), []);
  const closeUnitSettings = useCallback(() => setUnitSettingsOpen(false), []);
  const [selectedAtomId, setSelectedAtomId] = useState<string | null>(null);
  const [atomClipboard, setAtomClipboard] = useState<UnitGraphNode | null>(null);
  const [unitClipboard, setUnitClipboard] = useState<ProjectInstanceClipboard | null>(null);
  const [graphEditError, setGraphEditError] = useState<string | null>(null);
  const [libraryPointerDrag, setLibraryPointerDrag] = useState<ProjectLibraryPointerDrag | null>(null);
  const finishLibraryPointerDrag = useCallback(() => setLibraryPointerDrag(null), []);
  const [paramDrafts, setParamDrafts] = useState(() => buildParamDrafts(initialProjectInspect));
  const [paramOriginals, setParamOriginals] = useState(() => buildParamOriginals(initialProjectInspect));
  const [entryProject, setEntryProject] = useState(initialWorkspace.entryProject);
  const [workspaceFiles, setWorkspaceFilesState] = useState(initialWorkspace.files);
  const setWorkspaceFiles = useCallback((update: WorkspaceFile[] | ((files: WorkspaceFile[]) => WorkspaceFile[])) => {
    incrementPerfCounter('state.workspace.dispatches');
    setWorkspaceFilesState(update);
  }, []);
  const [selectedWorkspacePath, setSelectedWorkspacePath] = useState(
    initialContractUnit ? personalUnitWorkspacePath(initialContractUnit) : initialWorkspace.entryProject,
  );
  const appliedRoutePath = useRef<string | null>(null);
  const undoStack = useRef<WorkspaceHistoryEntry[]>([]);
  const redoStack = useRef<WorkspaceHistoryEntry[]>([]);
  const personalUnitSaveQueue = useRef<Promise<void>>(Promise.resolve());
  const [workspaceSaveError, setWorkspaceSaveError] = useState<string | null>(null);
  const [historyCounts, setHistoryCounts] = useState({ undo: 0, redo: 0 });
  const persistPersonalUnit = useCallback((unit: PersonalUnitRecord) => {
    const operation = personalUnitSaveQueue.current
      .catch(() => undefined)
      .then(() => onSavePersonalUnit(unit));
    personalUnitSaveQueue.current = operation;
    return operation;
  }, [onSavePersonalUnit]);
  const currentHistoryEntry = useCallback((): WorkspaceHistoryEntry => ({
    entryProject,
    files: workspaceFiles,
    activeContractUnit,
    selectedWorkspacePath,
    selectedId,
    selectedRouteIndex,
    selectedAtomId,
    canvasMode,
    inspectorView,
  }), [activeContractUnit, canvasMode, entryProject, inspectorView, selectedAtomId, selectedId, selectedRouteIndex, selectedWorkspacePath, workspaceFiles]);
  const restoreHistoryEntry = useCallback((entry: WorkspaceHistoryEntry) => {
    setEntryProject(entry.entryProject);
    setWorkspaceFiles(entry.files);
    selectActiveContractUnit(entry.activeContractUnit);
    setSelectedWorkspacePath(entry.selectedWorkspacePath);
    setSelectedId(entry.selectedId);
    setSelectedRouteIndex(entry.selectedRouteIndex);
    setSelectedAtomId(entry.selectedAtomId);
    setCanvasMode(entry.canvasMode);
    setInspectorView(entry.inspectorView);
    if (entry.activeContractUnit) {
      void persistPersonalUnit(entry.activeContractUnit).catch(error => {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to restore the Personal effect.');
      });
      navigate(unitRoute(personalUnitWorkspacePath(entry.activeContractUnit)));
    } else {
      navigate(PROJECT_ROUTE);
    }
    try {
      const projectFile = entry.files.find(file => file.path === entry.entryProject);
      const inspect = projectFile
        ? projectDraftToInspect(parseProjectGraphDraft(projectFile.content), backendSamples.project, entry.entryProject)
        : backendSamples.project;
      setParamDrafts(buildParamDrafts(inspect));
      setParamOriginals(buildParamOriginals(inspect));
    } catch {
      setParamDrafts(buildParamDrafts(backendSamples.project));
      setParamOriginals(buildParamOriginals(backendSamples.project));
    }
  }, [navigate, persistPersonalUnit, selectActiveContractUnit, setWorkspaceFiles]);
  const pushHistory = useCallback(() => {
    undoStack.current = [...undoStack.current.slice(-49), currentHistoryEntry()];
    redoStack.current = [];
    setHistoryCounts({ undo: undoStack.current.length, redo: 0 });
  }, [currentHistoryEntry]);
  const undoWorkspace = useCallback(() => {
    const previous = undoStack.current.pop();
    if (!previous) return;
    redoStack.current = [...redoStack.current.slice(-49), currentHistoryEntry()];
    restoreHistoryEntry(previous);
    setHistoryCounts({ undo: undoStack.current.length, redo: redoStack.current.length });
  }, [currentHistoryEntry, restoreHistoryEntry]);

  const undoWorkspaceWithPerf = useCallback(() => {
    markPerfSpan('ui.undo', () => {
      undoWorkspace();
    });
  }, [undoWorkspace]);

  const redoWorkspace = useCallback(() => {
    const next = redoStack.current.pop();
    if (!next) return;
    undoStack.current = [...undoStack.current.slice(-49), currentHistoryEntry()];
    restoreHistoryEntry(next);
    setHistoryCounts({ undo: undoStack.current.length, redo: redoStack.current.length });
  }, [currentHistoryEntry, restoreHistoryEntry]);

  const redoWorkspaceWithPerf = useCallback(() => {
    markPerfSpan('ui.redo', () => {
      redoWorkspace();
    });
  }, [redoWorkspace]);
  const projectWorkspaceFile = workspaceFiles.find(file => file.path === entryProject) ?? workspaceFiles[0];
  const lastValidProjectDraft = useRef(initialProjectDraft ?? parseProjectGraphDraft(initialWorkspaceFiles[0].content));
  const parsedProjectDraft = useMemo(() => {
    try {
      return parseProjectGraphDraft(projectWorkspaceFile.content);
    } catch {
      return null;
    }
  }, [projectWorkspaceFile.content]);
  if (parsedProjectDraft) lastValidProjectDraft.current = parsedProjectDraft;
  const projectDraft = parsedProjectDraft ?? lastValidProjectDraft.current;
  const project = useMemo(() => {
    const inspect = projectDraftToInspect(projectDraft, backendSamples.project, projectWorkspaceFile.path);
    return {
      ...inspect,
      units: inspect.units.map(unit => {
        const reference = projectDraft.units.find(item => item.id === unit.id);
        const path = reference ? resolveWorkspacePath(projectWorkspaceFile.path, reference.file) : null;
        const file = path ? workspaceFiles.find(item => item.path === path && item.role === 'unit') : null;
        if (!file) return unit;
        try {
          const draft = parseUnitGraphDraft(file.content);
          return { ...unit, name: draft.meta.title || draft.name, compatibility: draft.compatibility };
        } catch {
          return unit;
        }
      }),
    };
  }, [projectDraft, projectWorkspaceFile.path, workspaceFiles]);
  const selectedNode = findUnitNode(nodes, selectedId);
  const paramOverrides = useMemo(
    () => buildParamOverridesFromOriginals(project, paramDrafts, paramOriginals),
    [paramDrafts, paramOriginals, project],
  );
  const dirtyParamCount = paramOverrides.length;
  const workspaceDraftCount = workspaceFiles.filter(file => file.content !== file.originalContent).length;
  const hasWorkspaceDrafts = workspaceDraftCount > 0;
  const hasDirtyDrafts = dirtyParamCount > 0 || workspaceDraftCount > 0;
  const selectedWorkspaceFile = workspaceFiles.find(file => file.path === selectedWorkspacePath) ?? workspaceFiles[0];
  const selectedUnitWorkspaceFile = useMemo(() => {
    if (selectedNode?.kind !== 'unit') return selectedWorkspaceFile;

    const path = resolveWorkspacePath(project.file, selectedNode.unit.file);
    return workspaceFiles.find(file => file.path === path) ?? selectedWorkspaceFile;
  }, [project.file, selectedNode, selectedWorkspaceFile, workspaceFiles]);
  const selectedUnitGraph = useMemo(() => {
    try {
      if (selectedUnitWorkspaceFile.role !== 'unit') return null;
      return parseUnitGraphDraft(selectedUnitWorkspaceFile.content);
    } catch {
      return null;
    }
  }, [selectedUnitWorkspaceFile]);
  const contractWorkspaceFile = useMemo<WorkspaceFile | null>(() => activeContractUnit ? ({
    path: personalUnitWorkspacePath(activeContractUnit),
    role: 'unit',
    content: activeContractUnit.content,
    originalContent: activeContractUnit.content,
  }) : null, [activeContractUnit]);
  const contractUnitGraph = useMemo(() => {
    try {
      return contractWorkspaceFile ? parseUnitGraphDraft(contractWorkspaceFile.content) : null;
    } catch {
      return null;
    }
  }, [contractWorkspaceFile]);
  const selectedAtom =
    contractUnitGraph?.nodes.find(node => node.id === selectedAtomId) ?? contractUnitGraph?.nodes[0] ?? null;

  useEffect(() => {
    if (appliedRoutePath.current === location.pathname) return;
    appliedRoutePath.current = location.pathname;

    if (location.pathname === '/' || location.pathname === '') {
      navigate(PROJECT_ROUTE, { replace: true });
      return;
    }

    if (location.pathname === PROJECT_ROUTE) {
      selectActiveContractUnit(null);
      setUnitSettingsOpen(false);
      setSelectedWorkspacePath(entryProject);
      setCanvasMode('project');
      setInspectorView('project');
      return;
    }

    const pendingContract = activeContractUnitRef.current;
    const contract = personalUnitFromRoute(location.pathname, personalUnits)
      ?? (pendingContract && unitRoute(personalUnitWorkspacePath(pendingContract)) === location.pathname
        ? pendingContract
        : null);
    if (!contract) {
      navigate(PROJECT_ROUTE, { replace: true });
      return;
    }

    selectActiveContractUnit(contract);
    setSelectedWorkspacePath(personalUnitWorkspacePath(contract));
    setSelectedRouteIndex(null);
    setSelectedAtomId(null);
    setCanvasMode('contract');
    setInspectorView('atom');
    if (mode !== 'pro') onModeChange('pro');
  }, [entryProject, location.pathname, mode, navigate, onModeChange, personalUnits, selectActiveContractUnit]);

  const projectPorts = useMemo<ProjectPortCatalog>(() => Object.fromEntries(projectDraft.units.map(reference => {
    const path = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
    const file = workspaceFiles.find(item => item.path === path);
    if (file?.role !== 'unit') return [reference.id, { inputs: [], outputs: [] }];
    try {
      return [reference.id, parseUnitPortNames(file.content)];
    } catch {
      return [reference.id, { inputs: [], outputs: [] }];
    }
  })), [projectDraft.units, projectWorkspaceFile.path, workspaceFiles]);
  const projectUnitPlacement = useMemo(() => Object.fromEntries(projectDraft.units.map(reference => [
    reference.id,
    {
      allowed: projectPorts[reference.id]?.userPlaceable ?? false,
      reason: projectPorts[reference.id]?.reason ?? 'Unit metadata is unavailable.',
    },
  ])), [projectDraft.units, projectPorts]);

  const openPersonalContract = useCallback((unit: PersonalUnitRecord) => {
    const path = personalUnitWorkspacePath(unit);
    setGraphEditError(null);
    selectActiveContractUnit(unit);
    setUnitSettingsOpen(false);
    setSelectedWorkspacePath(path);
    setSelectedRouteIndex(null);
    setSelectedAtomId(null);
    setCanvasMode('contract');
    setInspectorView('atom');
    onModeChange('pro');
    navigate(unitRoute(path));
  }, [navigate, onModeChange, selectActiveContractUnit]);

  const editLibraryContract = useCallback(async (item: EffectLibraryItem) => {
    try {
      const source = libraryWorkspaceSource(item, personalUnits);
      if (!source) throw new Error(`Effect "${item.title}" is unavailable.`);
      assertUserPlaceableUnit(source.content);
      const existing = item.scope === 'personal'
        ? personalUnits.find(unit => unit.id === item.recordId)
        : null;
      if (existing) {
        openPersonalContract(existing);
        return;
      }
      const copy = createPersonalUnitCopy(item, source.content, personalUnits);
      await persistPersonalUnit(copy);
      openPersonalContract(copy);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to open that Contract.');
    }
  }, [openPersonalContract, persistPersonalUnit, personalUnits]);

  const editInstanceContract = useCallback(async (instanceId: string) => {
    try {
      const instance = projectDraft.nodes.find(node => node.id === instanceId);
      if (!instance) throw new Error(`Effect instance "${instanceId}" was not found.`);
      if (instance.routing || projectPorts[instance.unit]?.routing) {
        throw new Error('Panning and mixing helpers are managed by the Pipeline.');
      }
      const reference = projectDraft.units.find(unit => unit.id === instance.unit);
      if (!reference) throw new Error(`Effect definition "${instance.unit}" was not found.`);
      const sourcePath = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
      const source = workspaceFiles.find(file => file.role === 'unit' && file.path === sourcePath);
      if (!source) throw new Error(`Effect source "${sourcePath}" is unavailable.`);
      assertUserPlaceableUnit(source.content);

      const existingPersonal = personalUnits.find(unit => (
        resolveWorkspacePath(projectWorkspaceFile.path, reference.file) === personalUnitWorkspacePath(unit)
      ));
      if (existingPersonal) {
        setWorkspaceFiles(files => files.map(file => (
          file.path === personalUnitWorkspacePath(existingPersonal)
            ? { ...file, content: existingPersonal.content }
            : file
        )));
        openPersonalContract(existingPersonal);
        return;
      }

      const sourceDraft = parseUnitGraphDraft(source.content);
      const copy = createPersonalUnitCopy({
        id: sourceDraft.name,
        title: sourceDraft.meta.title || sourceDraft.name.replace(/_/g, ' '),
        category: sourceDraft.meta.category || 'personal',
        description: sourceDraft.meta.description || `Personal copy of ${sourceDraft.name}.`,
      }, source.content, personalUnits);
      await persistPersonalUnit(copy);

      const targetPath = personalUnitWorkspacePath(copy);
      let nextContent = projectWorkspaceFile.content;
      let nextReference = parseProjectGraphDraft(nextContent).units.find(unit => (
        resolveWorkspacePath(projectWorkspaceFile.path, unit.file) === targetPath
      ));
      if (!nextReference) {
        const used = new Set(projectDraft.units.map(unit => unit.id));
        const base = `${copy.name}_unit`;
        let id = base;
        let suffix = 2;
        while (used.has(id)) id = `${copy.name}_${suffix++}_unit`;
        nextContent = addProjectUnitReference(
          nextContent,
          id,
          relativeWorkspaceReference(projectWorkspaceFile.path, targetPath),
        );
        nextReference = { id, file: relativeWorkspaceReference(projectWorkspaceFile.path, targetPath) };
      }
      const nextPorts = { ...projectPorts, [nextReference.id]: parseUnitPortNames(copy.content) };
      nextContent = rebindProjectInstanceUnit(nextContent, nextPorts, instanceId, nextReference.id);

      pushHistory();
      setWorkspaceFiles(files => {
        const next = files.map(file => file.path === projectWorkspaceFile.path
          ? { ...file, content: nextContent }
          : file);
        return next.some(file => file.path === targetPath)
          ? next.map(file => file.path === targetPath ? { ...file, content: copy.content } : file)
          : [...next, { path: targetPath, role: 'unit', content: copy.content, originalContent: copy.content }];
      });
      openPersonalContract(copy);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to open that Contract.');
    }
  }, [
    openPersonalContract,
    personalUnits,
    persistPersonalUnit,
    projectDraft.nodes,
    projectDraft.units,
    projectPorts,
    projectWorkspaceFile.content,
    projectWorkspaceFile.path,
    pushHistory,
    setWorkspaceFiles,
    workspaceFiles,
  ]);

  const projectParamControls = useMemo<Record<string, ProjectParamControl[]>>(() => Object.fromEntries(
    projectDraft.units.map(reference => {
      const path = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
      const file = workspaceFiles.find(item => item.path === path);
      if (file?.role !== 'unit') return [reference.id, []];
      try {
        return [reference.id, parseUnitGraphDraft(file.content).params.map(param => ({
          key: param.name,
          label: param.ui?.label ?? param.name,
          type: param.type,
          min: param.min,
          max: param.max,
          unit: param.ui?.unit,
          control: param.ui?.control,
        }))];
      } catch {
        return [reference.id, []];
      }
    }),
  ), [projectDraft.units, projectWorkspaceFile.path, workspaceFiles]);
  const projectReplacementOptions = useMemo<ProjectReplacementOption[]>(() => project.units.flatMap(unit => {
    const contract = projectPorts[unit.id];
    if (!contract || (!contract.userPlaceable && !contract.routing)) return [];
    return [{
      id: unit.id,
      label: unit.name,
      paramCount: projectParamControls[unit.id]?.length ?? 0,
      routing: contract.routing,
    }];
  }), [project.units, projectParamControls, projectPorts]);
  const projectParallelOptions = useMemo<ProjectParallelOption[]>(() => simpleEffectLibrary.map(item => ({
    id: item.id,
    label: item.title,
    disabled: Boolean(item.placementError),
    reason: item.placementError,
  })), [simpleEffectLibrary]);
  const canPasteUnit = Boolean(unitClipboard);

  const updateParamDraft = useCallback((instanceId: string, paramKey: string, value: string) => {
    markPerfSpan('param.update', () => {
      const key = paramDraftKey(instanceId, paramKey);
      setParamDrafts(drafts => (drafts[key] === value ? drafts : { ...drafts, [key]: value }));
      setWorkspaceFiles(files => {
        let changed = false;
        const next = files.map(file => {
          if (file.role !== 'project') return file;
          const content = updateProjectInstanceParam(file.content, instanceId, paramKey, value);
          if (content === file.content) return file;
          changed = true;
          return { ...file, content };
        });
        return changed ? next : files;
      });
    });
  }, [setWorkspaceFiles]);

  const graphTopologySignature = useMemo(
    () => JSON.stringify({
      nodes: projectDraft.nodes.map(node => [node.id, node.unit]),
      routes: project.routes.map(route => [route.from, route.to]),
      ports: projectPorts,
    }),
    [project.routes, projectDraft.nodes, projectPorts],
  );
  const graphTopologyRef = useRef('');

  useEffect(() => {
    markPerfSpan('graph.sync.project', () => {
      const topologyChanged = graphTopologyRef.current !== graphTopologySignature;
      graphTopologyRef.current = graphTopologySignature;

      if (topologyChanged) {
        const next = buildProjectGraph(project, projectPorts);
        setNodes(current => next.nodes.map(node => {
          const existing = current.find(item => item.id === node.id);
          let data = node.data;
          if (node.data.kind === 'unit') {
            const unitData = node.data;
            data = {
              ...unitData,
              ports: projectPorts[unitData.unit.id],
              paramControls: projectParamControls[unitData.unit.id] ?? [],
              onParamChange: updateParamDraft,
              bypassed: nodeBypassByInstance?.[unitData.instance.id] ?? false,
              bypassAvailable: nodeBypassAvailable && !unitData.instance.routing && !projectPorts[unitData.unit.id]?.routing,
              onBypassChange: setProjectNodeBypass,
            };
          }
          const callbacksMatch = existing?.data.kind !== 'unit' || data.kind !== 'unit'
            || (existing.data.onParamChange === data.onParamChange
              && existing.data.onBypassChange === data.onBypassChange);
          if (existing
            && existing.position.x === node.position.x
            && existing.position.y === node.position.y
            && callbacksMatch
            && JSON.stringify(existing.data) === JSON.stringify(data)) {
            return existing;
          }
          return { ...node, data, selected: existing?.selected };
        }));
        setEdges(next.edges);
        return;
      }

      const unitsById = new Map(project.units.map(unit => [unit.id, unit]));
      const instancesById = new Map(project.nodes.map(instance => [instance.id, instance]));
      setNodes(current => {
        let changed = false;
        const next = current.map(node => {
          if (node.data.kind !== 'unit') return node;
          const instance = instancesById.get(node.data.instance.id);
          const unit = instance ? unitsById.get(instance.unit) : null;
          if (!instance || !unit) return node;
          const data = {
            ...node.data,
            instance,
            unit,
            ports: projectPorts[unit.id],
            paramControls: projectParamControls[unit.id] ?? [],
            onParamChange: updateParamDraft,
            bypassed: nodeBypassByInstance?.[instance.id] ?? false,
            bypassAvailable: nodeBypassAvailable && !instance.routing && !projectPorts[unit.id]?.routing,
            onBypassChange: setProjectNodeBypass,
          };
          const previous = JSON.stringify({
            instance: node.data.instance,
            unit: node.data.unit,
            controls: node.data.paramControls,
          });
          const candidate = JSON.stringify({ instance, unit, controls: data.paramControls });
          if (previous === candidate
            && node.data.bypassed === data.bypassed
            && node.data.bypassAvailable === data.bypassAvailable
            && node.data.onParamChange === data.onParamChange
            && node.data.onBypassChange === data.onBypassChange) return node;
          changed = true;
          return { ...node, data };
        });
        return changed ? next : current;
      });
    });
  }, [graphTopologySignature, nodeBypassAvailable, nodeBypassByInstance, project, projectParamControls, projectPorts, setEdges, setNodes, setProjectNodeBypass, updateParamDraft]);

  const selectProjectNode = useCallback((id: string) => {
    markPerfSpan('ui.select.projectNode', () => {
      setSelectedId(id);
      setSelectedRouteIndex(null);
      setSelectedAtomId(null);
      setInspectorView('project');
    });
  }, []);

  const selectRoute = useCallback((index: number) => {
    markPerfSpan('ui.select.route', () => {
      setSelectedRouteIndex(index);
      setSelectedId(null);
      setCanvasMode('project');
      setSelectedAtomId(null);
      setInspectorView('project');
    });
  }, []);

  const updateProjectFile = useCallback((update: (content: string) => string) => {
    return markPerfSpan('graph.update.project', () => {
      setGraphEditError(null);
      try {
        const content = update(projectWorkspaceFile.content);
        pushHistory();
        setWorkspaceFiles(files => files.map(file =>
          file.path === projectWorkspaceFile.path ? { ...file, content } : file));
        return content;
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : String(error));
        return null;
      }
    });
  }, [projectWorkspaceFile.content, projectWorkspaceFile.path, pushHistory, setWorkspaceFiles]);

  const addProjectNode = useCallback((unitId: string, instanceId: string) => {
    markPerfSpan('graph.add.projectNode', () => {
      const reference = projectDraft.units.find(unit => unit.id === unitId);
      if (!reference) return;
      const unitPath = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
      const unitFile = workspaceFiles.find(file => file.path === unitPath);
      if (unitFile?.role !== 'unit') throw new Error(`Unit source for "${unitId}" is unavailable.`);
      assertUserPlaceableUnit(unitFile.content);
      const defaults = unitFile?.role === 'unit'
        ? Object.fromEntries(parseUnitGraphDraft(unitFile.content).params.map(param => [param.name, param.default]))
        : {};
      const result = addProjectInstance(projectWorkspaceFile.content, unitId, instanceId, defaults);
      if (!updateProjectFile(() => result.content)) return;
      setParamDrafts(values => ({ ...values, ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value])) }));
      setParamOriginals(values => ({ ...values, ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value])) }));
      setSelectedId(`unit-${result.id}`);
    });
  }, [projectDraft.units, projectWorkspaceFile.content, projectWorkspaceFile.path, updateProjectFile, workspaceFiles]);

  const addProjectNodeFromLibrary = useCallback((unitId: string) => {
    markPerfSpan('graph.add.projectNodeFromLibrary', () => {
      addProjectNode(unitId, uniqueInstanceId(project.nodes.map(node => node.id), unitId));
    });
  }, [addProjectNode, project.nodes]);

  const insertProjectNodeOnRoute = useCallback((
    unitId: string,
    routeIndex: number,
  ) => {
    markPerfSpan('graph.insert.projectNode', () => {
      const reference = projectDraft.units.find(unit => unit.id === unitId);
      if (!reference) return;
      const unitPath = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
      const unitFile = workspaceFiles.find(file => file.path === unitPath);
      if (unitFile?.role !== 'unit') throw new Error(`Unit source for "${unitId}" is unavailable.`);
      assertUserPlaceableUnit(unitFile.content);
      const defaults = unitFile?.role === 'unit'
        ? Object.fromEntries(parseUnitGraphDraft(unitFile.content).params.map(param => [param.name, param.default]))
        : {};
      const instanceId = uniqueInstanceId(project.nodes.map(node => node.id), unitId);
      const result = insertProjectInstanceOnRoute(
        projectWorkspaceFile.content,
        projectPorts,
        unitId,
        instanceId,
        routeIndex,
        defaults,
      );
      if (!updateProjectFile(() => result.content)) return;
      const values = Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value]));
      setParamDrafts(current => ({ ...current, ...values }));
      setParamOriginals(current => ({ ...current, ...values }));
      setSelectedId(`unit-${result.id}`);
    });
  }, [project.nodes, projectDraft.units, projectPorts, projectWorkspaceFile.content, projectWorkspaceFile.path, updateProjectFile, workspaceFiles]);

  const addSimpleEffect = useCallback((item: EffectLibraryItem, requestedRouteIndex?: number) => {
    markPerfSpan('graph.add.simpleEffect', () => {
      try {
        const source = libraryWorkspaceSource(item, personalUnits);
        if (!source) throw new Error(`Effect "${item.title}" is unavailable.`);
        assertUserPlaceableUnit(source.content);
        const fallbackRouteIndex = project.routes.findIndex(route => route.to === 'system.output');
        const routeIndex = requestedRouteIndex ?? fallbackRouteIndex;
        if (!project.routes[routeIndex]) {
          throw new Error(requestedRouteIndex === undefined
            ? 'Connect the board to Output before adding another effect.'
            : 'Drop that effect on a connected rail.');
        }
        const existing = projectDraft.units.find(reference => reference.id === `${item.id}_unit`
          || reference.file.endsWith(`/${item.id}.unit.v2.yaml`));
        if (existing) {
          insertProjectNodeOnRoute(existing.id, routeIndex);
          return;
        }

        const unitId = `${item.id}_unit`;
        const instanceId = uniqueInstanceId(project.nodes.map(node => node.id), unitId);
        const targetPath = item.scope === 'personal'
          ? `personal/${item.id}.unit.v2.yaml`
          : `units/${item.id}.unit.v2.yaml`;
        const reference = relativeWorkspaceReference(projectWorkspaceFile.path, targetPath);
        const defaults = Object.fromEntries(parseUnitGraphDraft(source.content).params.map(param => [
          param.name,
          param.default,
        ]));
        const ports = parseUnitPortNames(source.content);
        const registered = addProjectUnitReference(projectWorkspaceFile.content, unitId, reference);
        const result = insertProjectInstanceOnRoute(
          registered,
          { ...projectPorts, [unitId]: ports },
          unitId,
          instanceId,
          routeIndex,
          defaults,
        );
        pushHistory();
        setWorkspaceFiles(files => {
          const updated = files.map(file => file.path === projectWorkspaceFile.path
            ? { ...file, content: result.content }
            : file);
          return updated.some(file => file.path === targetPath)
            ? updated
            : [...updated, { ...source, path: targetPath }];
        });
        const values = Object.fromEntries(Object.entries(defaults).map(([key, value]) => [
          paramDraftKey(instanceId, key),
          value,
        ]));
        setParamDrafts(current => ({ ...current, ...values }));
        setParamOriginals(current => ({ ...current, ...values }));
        setSelectedId(`unit-${instanceId}`);
      } catch (caught) {
        setGraphEditError(caught instanceof Error ? caught.message : 'Unable to add that effect.');
      }
    });
  }, [
    insertProjectNodeOnRoute,
    project.nodes,
    project.routes,
    projectDraft.units,
    projectPorts,
    projectWorkspaceFile.content,
    projectWorkspaceFile.path,
    personalUnits,
    pushHistory,
    setWorkspaceFiles,
  ]);

  const placeLibraryEffect = useCallback((itemId: string, routeIndex?: number) => {
    const item = simpleEffectLibrary.find(candidate => candidate.id === itemId);
    if (item) {
      addSimpleEffect(item, routeIndex);
      return;
    }
    if (routeIndex === undefined) addProjectNodeFromLibrary(itemId);
    else insertProjectNodeOnRoute(itemId, routeIndex);
  }, [addProjectNodeFromLibrary, addSimpleEffect, insertProjectNodeOnRoute, simpleEffectLibrary]);

  const addSimpleParallelEffect = useCallback((item: EffectLibraryItem, requestedRouteIndex?: number) => {
    markPerfSpan('graph.add.simpleParallelEffect', () => {
      try {
        const source = libraryWorkspaceSource(item, personalUnits);
        if (!source) throw new Error(`Effect "${item.title}" is unavailable.`);
        assertUserPlaceableUnit(source.content);
        const fallbackRouteIndex = project.routes.findIndex(route => route.to === 'system.output');
        const routeIndex = requestedRouteIndex ?? selectedRouteIndex ?? fallbackRouteIndex;
        if (!project.routes[routeIndex]) throw new Error('Select a connected route before adding a parallel effect.');

        let content = projectWorkspaceFile.content;
        const nextPorts = { ...projectPorts };
        const filesToAdd: WorkspaceFile[] = [];
        let effectReference = projectDraft.units.find(reference => (
          reference.id === `${item.id}_unit` || reference.file.endsWith(`/${item.id}.unit.v2.yaml`)
        ));
        if (!effectReference) {
          const id = `${item.id}_unit`;
          const path = item.scope === 'personal' ? `personal/${item.id}.unit.v2.yaml` : `units/${item.id}.unit.v2.yaml`;
          content = addProjectUnitReference(content, id, relativeWorkspaceReference(projectWorkspaceFile.path, path));
          effectReference = { id, file: relativeWorkspaceReference(projectWorkspaceFile.path, path) };
          nextPorts[id] = parseUnitPortNames(source.content);
          filesToAdd.push({ ...source, path });
        }

        let pannerReference = parseProjectGraphDraft(content).units.find(reference => (
          reference.id === 'path_panner_2_unit' || reference.file.endsWith('/path_panner_2.unit.v2.yaml')
        ));
        if (!pannerReference) {
          const pannerPath = 'units/path_panner_2.unit.v2.yaml';
          content = addProjectUnitReference(
            content,
            'path_panner_2_unit',
            relativeWorkspaceReference(projectWorkspaceFile.path, pannerPath),
          );
          pannerReference = {
            id: 'path_panner_2_unit',
            file: relativeWorkspaceReference(projectWorkspaceFile.path, pannerPath),
          };
          nextPorts.path_panner_2_unit = parseUnitPortNames(pathPanner2WorkspaceFile.content);
          filesToAdd.push({ ...pathPanner2WorkspaceFile, path: pannerPath });
        }

        let mixerReference = parseProjectGraphDraft(content).units.find(reference => (
          reference.id === 'path_mixer_2_unit' || reference.file.endsWith('/path_mixer_2.unit.v2.yaml')
        ));
        if (!mixerReference) {
          const mixerPath = 'units/path_mixer_2.unit.v2.yaml';
          content = addProjectUnitReference(
            content,
            'path_mixer_2_unit',
            relativeWorkspaceReference(projectWorkspaceFile.path, mixerPath),
          );
          mixerReference = {
            id: 'path_mixer_2_unit',
            file: relativeWorkspaceReference(projectWorkspaceFile.path, mixerPath),
          };
          nextPorts.path_mixer_2_unit = parseUnitPortNames(pathMixer2WorkspaceFile.content);
          filesToAdd.push({ ...pathMixer2WorkspaceFile, path: mixerPath });
        }

        const effectDefaults = Object.fromEntries(parseUnitGraphDraft(source.content).params.map(param => [param.name, param.default]));
        const pannerDefaults = Object.fromEntries(parseUnitGraphDraft(pathPanner2WorkspaceFile.content).params.map(param => [param.name, param.default]));
        const mixerDefaults = Object.fromEntries(parseUnitGraphDraft(pathMixer2WorkspaceFile.content).params.map(param => [param.name, param.default]));
        const effectId = uniqueInstanceId(project.nodes.map(node => node.id), effectReference.id);
        const pannerId = uniqueInstanceId([...project.nodes.map(node => node.id), effectId], 'path_panner_2_unit');
        const mixerId = uniqueInstanceId([...project.nodes.map(node => node.id), effectId, pannerId], 'path_mixer_2_unit');
        const result = insertProjectParallelOnRoute(
          content,
          nextPorts,
          effectReference.id,
          effectId,
          pannerReference.id,
          pannerId,
          mixerReference.id,
          mixerId,
          routeIndex,
          effectDefaults,
          pannerDefaults,
          mixerDefaults,
        );
        pushHistory();
        setWorkspaceFiles(files => {
          const updated = files.map(file => file.path === projectWorkspaceFile.path ? { ...file, content: result.content } : file);
          return filesToAdd.reduce((current, file) => (
            current.some(item => item.path === file.path) ? current : [...current, file]
          ), updated);
        });
        const values = Object.fromEntries([
          ...Object.entries(effectDefaults).map(([key, value]) => [paramDraftKey(effectId, key), value]),
          ...Object.entries(pannerDefaults).map(([key, value]) => [paramDraftKey(pannerId, key), value]),
          ...Object.entries(mixerDefaults).map(([key, value]) => [paramDraftKey(mixerId, key), value]),
        ]);
        setParamDrafts(current => ({ ...current, ...values }));
        setParamOriginals(current => ({ ...current, ...values }));
        setSelectedId(`unit-${effectId}`);
      } catch (caught) {
        setGraphEditError(caught instanceof Error ? caught.message : 'Unable to build that parallel path.');
      }
    });
  }, [
    personalUnits,
    project.nodes,
    project.routes,
    projectDraft.units,
    projectPorts,
    projectWorkspaceFile.content,
    projectWorkspaceFile.path,
    pushHistory,
    selectedRouteIndex,
    setWorkspaceFiles,
  ]);

  const addParallelEffectAtRoute = useCallback((unitId: string, routeIndex: number) => {
    const item = simpleEffectLibrary.find(candidate => candidate.id === unitId);
    if (!item) {
      setGraphEditError(`Effect "${unitId}" is unavailable.`);
      return;
    }
    addSimpleParallelEffect(item, routeIndex);
  }, [addSimpleParallelEffect, simpleEffectLibrary]);

  const moveProjectNodeToRoute = useCallback((instanceId: string, routeIndex: number) => {
    markPerfSpan('graph.move.projectNode', () => {
      try {
        const content = moveProjectInstanceOnRoute(
          projectWorkspaceFile.content,
          projectPorts,
          instanceId,
          routeIndex,
        );
        if (content === projectWorkspaceFile.content || !updateProjectFile(() => content)) return;
        setSelectedId(`unit-${instanceId}`);
        setSelectedRouteIndex(null);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to move that unit.');
      }
    });
  }, [projectPorts, projectWorkspaceFile.content, updateProjectFile]);

  const duplicateProjectNode = useCallback((instanceId: string) => {
    markPerfSpan('graph.duplicate.projectNode', () => {
      try {
        const result = duplicateProjectInstance(projectWorkspaceFile.content, instanceId);
        const source = project.nodes.find(node => node.id === instanceId);
        if (source && !projectUnitPlacement[source.unit]?.allowed) {
          throw new Error(projectUnitPlacement[source.unit]?.reason ?? 'Only mono effects can be duplicated.');
        }
        if (!source || !updateProjectFile(() => result.content)) return;
        const values = Object.fromEntries(source.params.map(param => [paramDraftKey(result.id, param.key), param.value]));
        setParamDrafts(current => ({ ...current, ...values }));
        setParamOriginals(current => ({ ...current, ...values }));
        setSelectedId(`unit-${result.id}`);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : String(error));
      }
    });
  }, [project.nodes, projectUnitPlacement, projectWorkspaceFile.content, updateProjectFile]);

  const removeProjectNode = useCallback((instanceId: string): boolean => {
    const instance = projectDraft.nodes.find(node => node.id === instanceId);
    if (!instance) return false;
    const message = instance.routing
      ? `Remove the split/join section containing “${instanceId}”? You can undo this after removal.`
      : `Remove “${instanceId}” from the Pipeline? You can undo this after removal.`;
    if (!window.confirm(message)) return false;

    return markPerfSpan('graph.remove.projectNode', () => {
      const sectionIds = instance?.routing
        ? projectDraft.nodes.filter(node => node.routing?.section === instance.routing?.section).map(node => node.id)
        : [instanceId];
      if (!updateProjectFile(content => (
        instance?.routing
          ? removeEmptyProjectRoutingSection(content, projectPorts, instanceId).content
          : removeProjectInstanceWithTopology(content, projectPorts, instanceId).content
      ))) return false;
      setParamDrafts(values => sectionIds.reduce(withoutInstanceValues, values));
      setParamOriginals(values => sectionIds.reduce(withoutInstanceValues, values));
      setSelectedId(null);
      return true;
    });
  }, [projectDraft.nodes, projectPorts, updateProjectFile]);

  useEffect(() => {
    const removeSelectedWithDelete = (event: KeyboardEvent) => {
      if (mode !== 'simple' || event.key !== 'Delete' || event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
      if (event.target instanceof Element && event.target.closest('input, textarea, select, button, [contenteditable="true"], [role="menu"]')) return;
      if (selectedNode?.kind !== 'unit') return;
      event.preventDefault();
      removeProjectNode(selectedNode.instance.id);
    };
    window.addEventListener('keydown', removeSelectedWithDelete, true);
    return () => window.removeEventListener('keydown', removeSelectedWithDelete, true);
  }, [mode, removeProjectNode, selectedNode]);

  const copyProjectNode = useCallback((instanceId: string) => {
    try {
      setUnitClipboard(copyProjectInstance(projectWorkspaceFile.content, instanceId));
      setGraphEditError(null);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to copy unit.');
    }
  }, [projectWorkspaceFile.content]);

  const cutProjectNode = useCallback((instanceId: string) => {
    markPerfSpan('graph.cut.projectNode', () => {
      try {
        const clipboard = copyProjectInstance(projectWorkspaceFile.content, instanceId);
        const result = removeProjectInstanceWithTopology(projectWorkspaceFile.content, projectPorts, instanceId);
        if (!updateProjectFile(() => result.content)) return;
        setUnitClipboard(clipboard);
        setParamDrafts(values => withoutInstanceValues(values, instanceId));
        setParamOriginals(values => withoutInstanceValues(values, instanceId));
        setSelectedId(null);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to cut unit.');
      }
    });
  }, [projectPorts, projectWorkspaceFile.content, updateProjectFile]);

  const pasteProjectNode = useCallback(() => {
    if (!unitClipboard) return;
    markPerfSpan('graph.paste.projectNode', () => {
      try {
        if (!projectUnitPlacement[unitClipboard.unit]?.allowed) {
          throw new Error(projectUnitPlacement[unitClipboard.unit]?.reason ?? 'Only mono effects can be pasted.');
        }
        const result = pasteProjectInstance(projectWorkspaceFile.content, unitClipboard);
        if (!updateProjectFile(() => result.content)) return;
        const values = Object.fromEntries(Object.entries(unitClipboard.params).map(([key, value]) => [
          paramDraftKey(result.id, key),
          value,
        ]));
        setParamDrafts(current => ({ ...current, ...values }));
        setParamOriginals(current => ({ ...current, ...values }));
        setSelectedId(`unit-${result.id}`);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to paste unit.');
      }
    });
  }, [projectUnitPlacement, projectWorkspaceFile.content, unitClipboard, updateProjectFile]);

  const replaceProjectNode = useCallback((instanceId: string, nextUnitId: string) => {
    markPerfSpan('graph.replace.projectNode', () => {
      try {
        const reference = projectDraft.units.find(unit => unit.id === nextUnitId);
        if (!reference) throw new Error(`Project unit "${nextUnitId}" was not found.`);
        const path = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
        const file = workspaceFiles.find(item => item.path === path);
        if (file?.role !== 'unit') throw new Error(`Unit source for "${nextUnitId}" is unavailable.`);
        const current = projectDraft.nodes.find(node => node.id === instanceId);
        if (!current) throw new Error(`Project instance "${instanceId}" was not found.`);
        if (!projectPorts[current.unit]?.routing) assertUserPlaceableUnit(file.content);
        const defaults = Object.fromEntries(parseUnitGraphDraft(file.content).params.map(param => [param.name, param.default]));
        const content = replaceProjectInstance(
          projectWorkspaceFile.content,
          projectPorts,
          instanceId,
          nextUnitId,
          defaults,
        );
        if (!updateProjectFile(() => content)) return;
        const resetValues = (values: Record<string, string>) => ({
          ...withoutInstanceValues(values, instanceId),
          ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(instanceId, key), value])),
        });
        setParamDrafts(resetValues);
        setParamOriginals(resetValues);
        setSelectedId(`unit-${instanceId}`);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to replace unit.');
      }
    });
  }, [projectDraft.nodes, projectDraft.units, projectPorts, projectWorkspaceFile.content, projectWorkspaceFile.path, updateProjectFile, workspaceFiles]);

  const saveScene = useCallback((name: string) => {
    const params = Object.fromEntries(project.nodes.flatMap(node => (
      node.params.map(param => [`${node.id}.${param.key}`, param.value])
    )));
    const bypass = Object.fromEntries(project.nodes.filter(node => !node.routing).map(node => [
      node.id,
      liveBypassController?.bypassByInstance[node.id] ?? false,
    ]));
    if (updateProjectFile(content => upsertProjectScene(content, name, params, bypass))) setActiveScene(name);
  }, [liveBypassController, project.nodes, updateProjectFile]);

  const applyScene = useCallback((name: string) => {
    try {
      const result = applyProjectScene(projectWorkspaceFile.content, name);
      if (!updateProjectFile(() => result.content)) return;
      const scene = parseProjectGraphDraft(result.content).scenes.find(item => item.name === name);
      if (scene) {
        setParamDrafts(current => ({
          ...current,
          ...Object.fromEntries(Object.entries(scene.params).map(([path, value]) => [path, value])),
        }));
      }
      for (const [instanceId, bypassed] of Object.entries(result.bypass)) {
        void liveBypassController?.setBypass(instanceId, bypassed);
      }
      setActiveScene(name);
    } catch (caught) {
      setGraphEditError(caught instanceof Error ? caught.message : 'Unable to recall that scene.');
    }
  }, [liveBypassController, projectWorkspaceFile.content, updateProjectFile]);

  const renameScene = useCallback((name: string, nextName: string) => {
    if (!updateProjectFile(content => renameProjectScene(content, name, nextName))) return;
    setActiveScene(current => current === name ? nextName : current);
  }, [updateProjectFile]);

  const deleteScene = useCallback((name: string) => {
    if (!updateProjectFile(content => removeProjectScene(content, name))) return;
    setActiveScene(current => current === name ? null : current);
  }, [updateProjectFile]);

  const renameProjectNode = useCallback((instanceId: string, nextId: string) => {
    const normalizedId = nextId.trim();
    if (!normalizedId || normalizedId === instanceId) return;
    if (!updateProjectFile(content => renameProjectInstance(content, instanceId, normalizedId))) return;
    setSelectedId(`unit-${normalizedId}`);
  }, [updateProjectFile]);

  const selectedUnitName = selectedNode?.kind === 'unit'
    ? selectedUnitGraph?.name ?? selectedNode.unit.name.replace(/_unit$/, '')
    : null;
  const selectedUnitPresets = useMemo(
    () => selectedUnitName ? listPresetsForUnit(selectedUnitName, personalPresets) : [],
    [personalPresets, selectedUnitName],
  );

  const applyPreset = useCallback((preset: UnitPreset) => {
    if (!selectedNode || selectedNode.kind !== 'unit') return;
    const instanceId = selectedNode.instance.id;
    const next = Object.entries(preset.params).reduce(
      (content, [key, value]) => updateProjectInstanceParam(content, instanceId, key, value),
      projectWorkspaceFile.content,
    );
    if (!updateProjectFile(() => next)) return;
    setParamDrafts(current => ({
      ...current,
      ...Object.fromEntries(Object.entries(preset.params).map(([key, value]) => [paramDraftKey(instanceId, key), value])),
    }));
    setActiveScene(null);
  }, [projectWorkspaceFile.content, selectedNode, updateProjectFile]);

  const savePreset = useCallback((name: string) => {
    if (!selectedNode || selectedNode.kind !== 'unit' || !selectedUnitName) return;
    onSavePreset(createPersonalPreset({
      id: globalThis.crypto?.randomUUID?.() ?? `preset-${Date.now()}`,
      name,
      description: `Saved from ${selectedNode.instance.id}.`,
      unitName: selectedUnitName,
      params: Object.fromEntries(selectedNode.instance.params.map(param => [param.key, param.value])),
    }));
  }, [onSavePreset, selectedNode, selectedUnitName]);

  const saveSelectedUnitToLibrary = useCallback(() => {
    if (!selectedNode || selectedNode.kind !== 'unit' || !selectedUnitName || selectedUnitWorkspaceFile.role !== 'unit') return;
    try {
      assertUserPlaceableUnit(selectedUnitWorkspaceFile.content);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Only mono effects can be saved to the library.');
      return;
    }
    const now = new Date().toISOString();
    void persistPersonalUnit({
      schema: 'apg.personal-unit.v1',
      version: 1,
      id: globalThis.crypto?.randomUUID?.() ?? `unit-${Date.now()}`,
      name: selectedUnitName,
      title: selectedNode.unit.name.replace(/_/g, ' '),
      category: 'personal',
      description: `Saved from ${projectPackage.manifest.name}.`,
      content: selectedUnitWorkspaceFile.content,
      createdAt: now,
      updatedAt: now,
    }).catch(error => {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to save that Personal effect.');
    });
  }, [persistPersonalUnit, projectPackage.manifest.name, selectedNode, selectedUnitName, selectedUnitWorkspaceFile]);

  const createProjectRoute = useCallback((route: ProjectRouteDraft) => {
    markPerfSpan('graph.create.route', () => {
      updateProjectFile(content => addProjectRoute(content, projectPorts, route));
    });
  }, [projectPorts, updateProjectFile]);

  const updateSelectedUnitFile = useCallback((update: (content: string) => string, nextAtomId?: string | null) => {
    return markPerfSpan('graph.update.unitFile', () => {
      setGraphEditError(null);
      try {
        if (!activeContractUnit) throw new Error('Choose a Personal effect before editing its contract.');
        const content = update(activeContractUnit.content);
        assertUserPlaceableUnit(content);
        const graph = parseUnitGraphDraft(content);
        const nextUnit: PersonalUnitRecord = {
          ...activeContractUnit,
          title: graph.meta.title || activeContractUnit.title,
          category: graph.meta.category || activeContractUnit.category,
          description: graph.meta.description,
          content,
          updatedAt: new Date().toISOString(),
        };
        const path = personalUnitWorkspacePath(nextUnit);
        const matchingReferences = projectDraft.units.filter(reference => (
          resolveWorkspacePath(projectWorkspaceFile.path, reference.file) === path
        ));
        let nextProjectContent = projectWorkspaceFile.content;
        let nextProjectPorts = projectPorts;
        for (const reference of matchingReferences) {
          nextProjectContent = syncProjectUnitContract(
            nextProjectContent,
            nextProjectPorts,
            reference.id,
            activeContractUnit.content,
            content,
          );
          nextProjectPorts = { ...nextProjectPorts, [reference.id]: parseUnitPortNames(content) };
        }
        pushHistory();
        selectActiveContractUnit(nextUnit);
        setWorkspaceFiles(files => files.map(file => {
          if (matchingReferences.length > 0 && file.path === projectWorkspaceFile.path) {
            return { ...file, content: nextProjectContent };
          }
          if (file.role === 'unit' && file.path === path) return { ...file, content };
          return file;
        }));
        if (matchingReferences.length > 0) {
          const inspect = projectDraftToInspect(
            parseProjectGraphDraft(nextProjectContent),
            backendSamples.project,
            projectWorkspaceFile.path,
          );
          const values = buildParamDrafts(inspect);
          setParamDrafts(current => Object.fromEntries(Object.entries(values).map(([key, value]) => [
            key,
            current[key] ?? value,
          ])));
          setParamOriginals(current => Object.fromEntries(Object.entries(values).map(([key, value]) => [
            key,
            current[key] ?? value,
          ])));
        }
        void persistPersonalUnit(nextUnit).catch(error => {
          setGraphEditError(error instanceof Error ? error.message : 'Unable to save the Personal effect.');
        });
        if (nextAtomId !== undefined) setSelectedAtomId(nextAtomId);
        return content;
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to update unit graph.');
        return null;
      }
    });
  }, [
    activeContractUnit,
    persistPersonalUnit,
    projectDraft.units,
    projectPorts,
    projectWorkspaceFile.content,
    projectWorkspaceFile.path,
    pushHistory,
    selectActiveContractUnit,
    setWorkspaceFiles,
  ]);

  const updateSelectedAtom = useCallback((node: UnitGraphNode, originalId = node.id) => {
    markPerfSpan('contract.edit.atom', () => {
      updateSelectedUnitFile(content => serializeUnitGraphNodeUpdate(content, node, originalId), node.id);
    });
  }, [updateSelectedUnitFile]);

  const reorderUnitParam = useCallback((paramName: string, nextIndex: number) => {
    markPerfSpan('contract.reorder.param', () => {
      updateSelectedUnitFile(content => moveUnitParam(content, paramName, nextIndex));
    });
  }, [updateSelectedUnitFile]);

  const addAtom = useCallback((atomName: string, position?: UnitGraphPosition) => {
    markPerfSpan('contract.add.atom', () => {
      try {
        if (!contractWorkspaceFile) throw new Error('Choose a Personal effect before adding atoms.');
        const result = addAtomNodeToUnit(contractWorkspaceFile.content, backendSamples.atomCatalog, atomName, position);
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to add atom.');
      }
    });
  }, [contractWorkspaceFile, updateSelectedUnitFile]);

  const insertAtomOnConnection = useCallback((
    atomName: string,
    target: UnitConnectionEndpoint,
    position: UnitGraphPosition,
  ) => {
    markPerfSpan('contract.insert.atom', () => {
      try {
        if (!contractWorkspaceFile) throw new Error('Choose a Personal effect before adding atoms.');
        const result = insertAtomNodeOnConnection(
          contractWorkspaceFile.content,
          backendSamples.atomCatalog,
          atomName,
          target,
          position,
        );
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to insert atom on connection.');
      }
    });
  }, [contractWorkspaceFile, updateSelectedUnitFile]);

  const removeSelectedAtom = useCallback((nodeId?: string) => {
    const targetId = nodeId ?? selectedAtom?.id;
    if (!targetId) return;
    markPerfSpan('contract.remove.atom', () => {
      updateSelectedUnitFile(
        content => removeAtomNodeWithTopology(content, backendSamples.atomCatalog, targetId).content,
        null,
      );
    });
  }, [selectedAtom?.id, updateSelectedUnitFile]);

  const replaceSelectedAtom = useCallback((nodeId: string, nextAtomName: string, preserveId: boolean) => {
    markPerfSpan('contract.replace.atom', () => {
      try {
        if (!contractWorkspaceFile) throw new Error('Choose a Personal effect before replacing atoms.');
        const result = replaceAtomNodeInUnit(contractWorkspaceFile.content, backendSamples.atomCatalog, nodeId, nextAtomName, preserveId);
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to replace atom.');
      }
    });
  }, [contractWorkspaceFile, updateSelectedUnitFile]);

  const connectAtoms = useCallback((source: UnitConnectionEndpoint, target: UnitConnectionEndpoint) => {
    markPerfSpan('contract.connect.atom', () => {
      updateSelectedUnitFile(content => connectUnitNodes(content, backendSamples.atomCatalog, source, target));
    });
  }, [updateSelectedUnitFile]);

  const reconnectAtoms = useCallback((
    previousTarget: UnitConnectionEndpoint,
    source: UnitConnectionEndpoint,
    target: UnitConnectionEndpoint,
  ) => {
    markPerfSpan('contract.reconnect.atom', () => {
      updateSelectedUnitFile(content =>
        reconnectUnitConnection(content, backendSamples.atomCatalog, previousTarget, source, target));
    });
  }, [updateSelectedUnitFile]);

  const disconnectAtom = useCallback((target: UnitConnectionEndpoint) => {
    markPerfSpan('contract.disconnect.atom', () => {
      updateSelectedUnitFile(content => disconnectUnitInput(content, target));
    });
  }, [updateSelectedUnitFile]);

  const moveAtom = useCallback((nodeId: string, position: UnitGraphPosition) => {
    markPerfSpan('contract.move.atom', () => {
      updateSelectedUnitFile(content => setAtomNodePosition(content, nodeId, position));
    });
  }, [updateSelectedUnitFile]);

  const autoLayoutAtoms = useCallback((positions: Record<string, UnitGraphPosition>) => {
    markPerfSpan('contract.layout.graphviz', () => {
      updateSelectedUnitFile(content => setAtomNodePositions(content, positions));
    });
  }, [updateSelectedUnitFile]);

  const copySelectedAtom = useCallback((nodeId?: string) => {
    const atom = contractUnitGraph?.nodes.find(node => node.id === (nodeId ?? selectedAtom?.id));
    if (atom) setAtomClipboard(atom);
  }, [contractUnitGraph?.nodes, selectedAtom?.id]);

  const cutSelectedAtom = useCallback((nodeId?: string) => {
    const atom = contractUnitGraph?.nodes.find(node => node.id === (nodeId ?? selectedAtom?.id));
    if (!atom) return;
    markPerfSpan('contract.cut.atom', () => {
      try {
        if (!contractWorkspaceFile) throw new Error('Choose a Personal effect before cutting atoms.');
        const result = removeAtomNodeWithTopology(
          contractWorkspaceFile.content,
          backendSamples.atomCatalog,
          atom.id,
        );
        if (updateSelectedUnitFile(() => result.content, null)) setAtomClipboard(atom);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to cut atom.');
      }
    });
  }, [contractUnitGraph?.nodes, contractWorkspaceFile, selectedAtom?.id, updateSelectedUnitFile]);

  const pasteAtom = useCallback(() => {
    if (!atomClipboard) return;

    markPerfSpan('contract.paste.atom', () => {
      try {
        if (!contractWorkspaceFile) throw new Error('Choose a Personal effect before pasting atoms.');
        const result = pasteAtomNodeIntoUnit(contractWorkspaceFile.content, atomClipboard);
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to paste atom.');
      }
    });
  }, [atomClipboard, contractWorkspaceFile, updateSelectedUnitFile]);

  const selectAtom = useCallback((id: string) => {
    setSelectedAtomId(id);
    setInspectorView('atom');
  }, []);

  const openAtomInspector = useCallback((id: string) => {
    markPerfSpan('ui.openAtomInspector', () => {
      setSelectedAtomId(id);
      setInspectorView('atom');
    });
  }, []);

  const resetWorkspace = useCallback(() => {
    markPerfSpan('workspace.reset', () => {
      pushHistory();
      setWorkspaceFiles(initialWorkspace.files);
      setEntryProject(initialWorkspace.entryProject);
      setSelectedWorkspacePath(initialWorkspace.entryProject);
      setParamDrafts(buildParamDrafts(initialProjectInspect));
      setParamOriginals(buildParamOriginals(initialProjectInspect));
      setSelectedId(initialProjectInspect.nodes[0] ? `unit-${initialProjectInspect.nodes[0].id}` : null);
      setSelectedRouteIndex(null);
      setCanvasMode('project');
      navigate(PROJECT_ROUTE);
    });
  }, [initialProjectInspect, initialWorkspace, navigate, pushHistory, setWorkspaceFiles]);

  const saveWorkspace = useCallback(() => {
    try {
      markPerfSpan('workspace.save', () => {
        const payload = createWorkspacePayload(entryProject, workspaceFiles);
        if (typeof window === 'undefined') return;
        persistWorkspacePayload(WORKSPACE_STORAGE_KEY, payload, window.localStorage);
        onWorkspaceChange(payload);
        setParamOriginals(values => ({ ...values, ...paramDrafts }));
        setWorkspaceFiles(files =>
          files.map(file => (file.content === file.originalContent ? file : { ...file, originalContent: file.content })),
        );
      });
      setWorkspaceSaveError(null);
    } catch (error) {
      setWorkspaceSaveError(error instanceof Error ? error.message : 'Unable to persist the workspace.');
    }
  }, [entryProject, onWorkspaceChange, paramDrafts, setWorkspaceFiles, workspaceFiles]);

  const exportWorkspace = useCallback(() => {
    markPerfSpan('workspace.export', () => {
      const payload = createWorkspacePayload(entryProject, workspaceFiles);
      onExportProject(payload);
    });
  }, [entryProject, onExportProject, workspaceFiles]);

  const importWorkspace = useCallback((file: File) => {
    void markPerfSpan('workspace.import', async () => {
      try {
        const importedPackage = migrateApgProjectRouting(
          parseApgProjectPackage(await file.text()),
          ROUTING_MIGRATION_HELPERS,
        ).project;
        const importedFiles = hydrateWorkspaceFiles(importedPackage.workspace, initialWorkspaceFiles);
        const importedProject = importedFiles.find(item => item.path === importedPackage.workspace.entryProject);
        if (!importedProject) throw new Error('The imported package has no entry project.');
        const importedDraft = parseProjectGraphDraft(importedProject.content);
        const importedInspect = projectDraftToInspect(
          importedDraft,
          backendSamples.project,
          importedPackage.workspace.entryProject,
        );

        pushHistory();
        setWorkspaceFiles(importedFiles);
        setEntryProject(importedPackage.workspace.entryProject);
        setSelectedWorkspacePath(importedPackage.workspace.entryProject);
        setParamDrafts(buildParamDrafts(importedInspect));
        setParamOriginals(buildParamOriginals(importedInspect));
        setSelectedId(null);
        setSelectedRouteIndex(null);
        setSelectedAtomId(null);
        setCanvasMode('project');
        setInspectorView('project');
        setGraphEditError(null);
        navigate(PROJECT_ROUTE);

        const now = new Date().toISOString();
        onProjectPackageChange(current => ({
          ...importedPackage,
          manifest: {
            ...importedPackage.manifest,
            id: current.manifest.id,
            createdAt: current.manifest.createdAt,
            updatedAt: now,
            lastMode: mode,
          },
        }));
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Project import failed.');
      }
    });
  }, [mode, navigate, onProjectPackageChange, pushHistory, setWorkspaceFiles]);

  const updateReadiness = useCallback((update: Partial<ApgProjectPackage['readiness']>) => {
    onProjectPackageChange(current => {
      const diagnostics = update.diagnostics === undefined
        ? current.readiness.diagnostics
        : [
            ...current.readiness.diagnostics.filter(item => item.code?.startsWith('APG_UI_')),
            ...update.diagnostics.filter(item => !item.code?.startsWith('APG_UI_')),
          ];
      const readiness = {
        ...current.readiness,
        ...update,
        targets: update.targets ?? current.readiness.targets,
        diagnostics,
      };
      return JSON.stringify(readiness) === JSON.stringify(current.readiness)
        ? current
        : { ...current, readiness };
    });
  }, [onProjectPackageChange]);

  const updatePackagedAudio = useCallback((asset: ApgProjectPackage['audio'][number] | null) => {
    onProjectPackageChange(current => ({
      ...current,
      manifest: { ...current.manifest, updatedAt: new Date().toISOString() },
      audio: asset ? [asset] : [],
    }));
  }, [onProjectPackageChange]);

  const handleRenderProfile = useCallback((
    id: string,
    phase: 'mount' | 'render' | 'update' | string,
    actualDurationMs: number,
    baseDurationMs: number,
    startTime: number,
    commitTime: number,
  ) => {
    markRenderPerfSpan({
      id,
      phase,
      actualDurationMs,
      baseDurationMs,
      startTime,
      commitTime,
      at: Date.now(),
    });
  }, []);

  const handleRuntimeReady = useCallback(() => {
    setRuntimeReady(true);
  }, []);

  const changeEditorView = useCallback((nextMode: StudioMode) => {
    if (nextMode === 'simple') {
      selectActiveContractUnit(null);
      setUnitSettingsOpen(false);
      setSelectedAtomId(null);
      setCanvasMode('project');
      setInspectorView('project');
      onModeChange('simple');
      navigate(PROJECT_ROUTE);
      return;
    }
    onModeChange('pro');
    if (activeContractUnit) navigate(unitRoute(personalUnitWorkspacePath(activeContractUnit)));
  }, [activeContractUnit, navigate, onModeChange, selectActiveContractUnit]);

  return (
    <LiveBypassContext.Provider value={liveBypassContextValue}>
    {!runtimeReady && (
      <section className="launch-screen" aria-live="polite">
        <div className="splash-bg-glow glow-1" />
        <div className="splash-bg-glow glow-2" />
        <div className="splash-bg-glow glow-3" />
        <div className="launch-screen__content">
          <div className="launch-screen__mark splash-logo" aria-hidden="true">
            <span className="splash-logo-inner">
              <AppLogo />
            </span>
          </div>
          <h1>Audio Playground <span>v2.0</span></h1>
          <p>Preparing your live audio workspace<br />and restoring the last good sound</p>
          <div className="launch-screen__progress">
            <div><span>Initializing audio engine...</span><span>Loading</span></div>
            <i><b className="launch-screen__progress-fill" /></i>
          </div>
        </div>
      </section>
    )}
    <div className={`app app--project app--${mode}`}>
        <ProjectTopbar
          projectName={projectPackage.manifest.name}
          dirtyParamCount={dirtyParamCount + workspaceDraftCount}
          hasDirtyParamDrafts={hasDirtyDrafts}
          hasWorkspaceDrafts={hasWorkspaceDrafts}
          workspaceSaveError={workspaceSaveError}
          workspaceFileCount={workspaceFiles.length}
          onExportWorkspace={exportWorkspace}
          onImportWorkspace={importWorkspace}
          onResetWorkspace={resetWorkspace}
          onUndo={undoWorkspaceWithPerf}
          onRedo={redoWorkspaceWithPerf}
          canUndo={historyCounts.undo > 0}
          canRedo={historyCounts.redo > 0}
          entryProject={entryProject}
          workspaceFiles={workspaceFiles}
          paramOverrides={paramOverrides}
          onSaveWorkspace={saveWorkspace}
          onRuntimeReady={handleRuntimeReady}
          mode={mode}
          onModeChange={changeEditorView}
          onHome={onHome}
          onTour={() => setTourOpen(true)}
          packagedAudio={projectPackage.audio}
          readiness={projectPackage.readiness}
          onAudioAssetChange={updatePackagedAudio}
          onReadinessUpdate={updateReadiness}
          editorError={graphEditError}
          onDismissEditorError={() => setGraphEditError(null)}
        />

      <div className="layout">
        {mode === 'simple' || !activeContractUnit ? (
          <SimpleLibraryPanel
            items={simpleEffectLibrary}
            onDeletePersonal={onDeletePersonalUnit}
            onEditContract={item => { void editLibraryContract(item); }}
            onPointerDrag={setLibraryPointerDrag}
            purpose={mode === 'pro' ? 'contract' : 'pipeline'}
          />
        ) : (
          <aside className="simple-library contract-atom-library" data-testid="contract-atom-library">
            <header>
              <div><span>Atom palette</span><strong>{backendSamples.atomCatalog.atoms.length}</strong></div>
              <p>
                Click Add atom or drag an atom onto the Contract graph.
                <AtomCatalogInfo catalog={backendSamples.atomCatalog} manifest={backendSamples.atomCatalogManifest} />
              </p>
            </header>
            <AtomCatalogPanel
              catalog={backendSamples.atomCatalog}
              onAddAtom={addAtom}
              showUnitInspect={false}
              unit={backendSamples.unit}
            />
          </aside>
        )}

        {mode === 'pro' && activeContractUnit && contractWorkspaceFile && contractUnitGraph ? (
          <Profiler id="ContractGraphCanvas" onRender={handleRenderProfile}>
            <ContractGraphCanvas
              catalog={backendSamples.atomCatalog}
              selectedAtomId={selectedAtomId}
              selectedUnitLabel={activeContractUnit.title}
              workspaceFile={contractWorkspaceFile}
              onBackToProject={() => markPerfSpan('ui.returnToPipeline', () => changeEditorView('simple'))}
              onOpenSettings={openUnitSettings}
              onAddAtomAt={addAtom}
              onInsertAtomAtEdge={insertAtomOnConnection}
              onMoveAtom={moveAtom}
              onAutoLayout={autoLayoutAtoms}
              atomClipboardReady={Boolean(atomClipboard)}
              onCopyAtom={copySelectedAtom}
              onCutAtom={cutSelectedAtom}
              onPasteAtom={pasteAtom}
              onRemoveAtom={removeSelectedAtom}
              onReplaceAtom={replaceSelectedAtom}
              onConnectAtoms={connectAtoms}
              onDisconnectAtom={disconnectAtom}
              onOpenAtomInspector={openAtomInspector}
              onReconnectAtoms={reconnectAtoms}
              onSelectAtom={selectAtom}
            />
          </Profiler>
        ) : mode === 'pro' ? (
          <main className="canvas canvas-area contract-empty-state" data-testid="contract-empty-state">
            <div>
              <span>Contract</span>
              <h2>Choose a unit to edit</h2>
              <p>Click a unit in the library. Built-in units are copied to Personal before any changes are made.</p>
              <button
                className="btn btn--primary"
                onClick={() => document.querySelector<HTMLElement>('.effect-library-card')?.focus()}
                type="button"
              >Choose an effect</button>
            </div>
          </main>
        ) : (
          <Profiler id="ProjectCanvas" onRender={handleRenderProfile}>
            <ProjectCanvas
              nodes={nodes}
              edges={edges}
              selectedRouteIndex={selectedRouteIndex}
              onNodesChange={onNodesChange}
              onEdgesChange={onEdgesChange}
              onSelectNode={selectProjectNode}
              onEditUnitContract={instanceId => { void editInstanceContract(instanceId); }}
              onSelectRoute={selectRoute}
              onAddUnit={placeLibraryEffect}
              onInsertUnitAtRoute={placeLibraryEffect}
              onConnectUnits={createProjectRoute}
              onCopyUnit={copyProjectNode}
              onCutUnit={cutProjectNode}
              onPasteUnit={pasteProjectNode}
              onRemoveUnit={removeProjectNode}
              onReplaceUnit={replaceProjectNode}
              onMoveUnitToRoute={moveProjectNodeToRoute}
              onAddParallelAtRoute={addParallelEffectAtRoute}
              libraryPointerDrag={libraryPointerDrag}
              onLibraryPointerDragHandled={finishLibraryPointerDrag}
              canPasteUnit={canPasteUnit}
              parallelOptions={projectParallelOptions}
              replacementOptions={projectReplacementOptions}
            />
          </Profiler>
        )}

        {mode === 'simple' || canvasMode === 'project' ? (
          <SimpleInspector
            onApplyPreset={applyPreset}
            onDeletePreset={onDeletePreset}
            onDuplicate={duplicateProjectNode}
            onEditContract={() => {
              if (selectedNode?.kind === 'unit') void editInstanceContract(selectedNode.instance.id);
            }}
            onRemove={removeProjectNode}
            onRename={renameProjectNode}
            onSavePreset={savePreset}
            onSaveToLibrary={saveSelectedUnitToLibrary}
            presets={selectedUnitPresets}
            selectedNode={selectedNode}
          />
        ) : activeContractUnit ? (
          <AtomContextInspector
            atom={selectedAtom}
            catalog={backendSamples.atomCatalog}
            clipboardReady={Boolean(atomClipboard)}
            error={graphEditError}
            onChange={updateSelectedAtom}
            onCopy={copySelectedAtom}
            onCut={cutSelectedAtom}
            onPaste={pasteAtom}
            onRemove={removeSelectedAtom}
          />
        ) : (
          <aside className="simple-inspector contract-empty-inspector">
            <span className="simple-inspector__eyebrow">Atom inspector</span>
            <h2>No contract selected</h2>
            <p>Open a Contract to inspect only the selected atom here.</p>
          </aside>
        )}
      </div>
      {mode === 'simple' ? (
        <SceneBar
          activeScene={activeScene}
          onApply={applyScene}
          onDelete={deleteScene}
          onRename={renameScene}
          onSave={saveScene}
          scenes={project.scenes}
        />
      ) : null}
      {activeContractUnit && contractWorkspaceFile && contractUnitGraph ? (
        <UnitSettingsDrawer
          file={contractWorkspaceFile}
          onChange={content => { updateSelectedUnitFile(() => content); }}
          onClose={closeUnitSettings}
          onReorderParam={reorderUnitParam}
          open={unitSettingsOpen}
          unit={contractUnitGraph}
        />
      ) : null}
    </div>
    <GuidedTour
      onClose={() => {
        window.localStorage.setItem('apg.studio.tour.v1', 'complete');
        setTourOpen(false);
      }}
      open={tourOpen}
    />
    <LiveLatencyBadge />
    </LiveBypassContext.Provider>
  );
}
