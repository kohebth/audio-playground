import { Profiler, useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';
import { useEdgesState, useNodesState, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { AppLogo } from './components/AppLogo';
import { ContractGraphCanvas } from './components/ContractGraphCanvas';
import { BatchActionBar } from './components/BatchActionBar';
import { GuidedTour } from './components/GuidedTour';
import { LiveLatencyBadge } from './components/LiveLatencyBadge';
import { ProjectCanvas } from './components/ProjectCanvas';
import { ProjectInspector } from './components/ProjectInspector';
import { ProjectSidebar } from './components/ProjectSidebar';
import { ProjectTopbar } from './components/ProjectTopbar';
import { SceneBar } from './components/SceneBar';
import { SimpleInspector } from './components/SimpleInspector';
import { SimpleLibraryPanel, type EffectLibraryItem } from './components/SimpleLibraryPanel';
import {
  backendCommands,
  backendSamples,
  initialWorkspaceFiles,
  wetDryMixWorkspaceFile,
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
  duplicateProjectInstance,
  insertProjectParallelOnRoute,
  insertProjectInstanceOnRoute,
  moveProjectInstance,
  moveProjectRoute,
  parseProjectGraphDraft,
  parseUnitPortNames,
  projectDraftToInspect,
  removeProjectInstance,
  removeProjectRoute,
  removeProjectScene,
  renameProjectInstance,
  renameProjectScene,
  replaceProjectRoute,
  setProjectInstancePosition,
  upsertProjectScene,
  type GraphPosition as ProjectGraphPosition,
  type ProjectPortCatalog,
  type ProjectRouteDraft,
} from './lib/projectV2Graph';
import type { ApgProjectPackage, StudioMode } from './lib/projectPackage';
import {
  createPersonalPreset,
  listPresetsForUnit,
  type PersonalUnitRecord,
  type UnitPreset,
} from './lib/presetLibrary';
import {
  addAtomNodeToUnit,
  connectUnitNodes,
  createUnitV2,
  disconnectUnitInput,
  insertAtomNodeOnConnection,
  moveUnitParam,
  pasteAtomNodeIntoUnit,
  parseUnitGraphDraft,
  removeAtomNodeFromUnit,
  replaceAtomNodeInUnit,
  reconnectUnitConnection,
  serializeUnitGraphNodeUpdate,
  setAtomNodePosition,
  updateProjectInstanceParam,
  type GraphPosition as UnitGraphPosition,
  type UnitGraphNode,
  type UnitConnectionEndpoint,
} from './lib/unitV2Graph';
import {
  createWorkspacePayload,
  hydrateWorkspaceFiles,
  parseWorkspacePayload,
  persistSerializedWorkspace,
  persistWorkspacePayload,
  validateWorkspacePayload,
  WORKSPACE_FORMAT_VERSION,
  WORKSPACE_SCHEMA,
  type WorkspacePayload,
} from './lib/workspacePersistence';
import {
  PERFORMANCE_DEBOUNCE_MS,
  incrementPerfCounter,
  markPerfSpan,
  markRenderPerfSpan,
  readPerfRenderSpans,
  readPerfSpans,
} from './lib/perfTelemetry';
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

type InspectorView = 'project' | 'atom' | 'contract';
type CanvasMode = 'project' | 'contract';
type WorkspaceHistoryEntry = {
  entryProject: string;
  files: WorkspaceFile[];
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

function workspacePathFromRoute(pathname: string, files: WorkspaceFile[]): string | null {
  const match = /^\/unit\/([^/]+)\/?$/.exec(pathname);
  if (!match) return null;
  let routeId: string;
  try {
    routeId = decodeURIComponent(match[1]);
  } catch {
    return null;
  }
  return files.find(file => file.role === 'unit' && unitRouteId(file.path) === routeId)?.path ?? null;
}

function loadWorkspaceState(projectPackage?: ApgProjectPackage): { entryProject: string; files: WorkspaceFile[] } {
  if (projectPackage) {
    return {
      entryProject: projectPackage.workspace.entryProject,
      files: hydrateWorkspaceFiles(projectPackage.workspace, initialWorkspaceFiles),
    };
  }
  const fallback = { entryProject: backendSamples.project.file, files: initialWorkspaceFiles };
  if (typeof window === 'undefined') return fallback;
  try {
    const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
    if (saved) {
      const payload = parseWorkspacePayload(saved);
      return { entryProject: payload.entryProject, files: hydrateWorkspaceFiles(payload, initialWorkspaceFiles) };
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
      return { entryProject: payload.entryProject, files: hydrateWorkspaceFiles(payload, initialWorkspaceFiles) };
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
  onImportProject: (file: File) => void;
  onProjectPackageChange: (update: (project: ApgProjectPackage) => ApgProjectPackage) => void;
  onSavePersonalUnit: (unit: PersonalUnitRecord) => void;
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
  onImportProject,
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
  const simpleEffectLibrary = useMemo<EffectLibraryItem[]>(() => [
    ...builtInSimpleEffectLibrary,
    ...personalUnits.map(unit => ({
      id: unit.name,
      title: unit.title,
      category: unit.category,
      description: unit.description,
      scope: 'personal' as const,
      recordId: unit.id,
    })),
  ], [personalUnits]);
  const initialRouteWorkspacePath = workspacePathFromRoute(location.pathname, initialWorkspace.files);
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
  const initialGraph = useMemo(() => buildProjectGraph(initialProjectInspect, initialProjectDraft), [initialProjectDraft, initialProjectInspect]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initialGraph.edges);
  const [liveBypassController, setLiveBypassController] = useState<LiveBypassController | null>(null);
  const liveBypassContextValue = useMemo(
    () => ({ controller: liveBypassController, setController: setLiveBypassController }),
    [liveBypassController],
  );
  const initialRouteUnit = initialRouteWorkspacePath
    ? initialProjectInspect.units.find(unit => (
        resolveWorkspacePath(initialProjectInspect.file, unit.file) === initialRouteWorkspacePath
      ))
    : null;
  const initialRouteNode = initialRouteUnit
    ? initialProjectInspect.nodes.find(node => node.unit === initialRouteUnit.id)
    : null;
  const [selectedId, setSelectedId] = useState<string | null>(() => initialRouteNode
    ? `unit-${initialRouteNode.id}`
    : initialProjectInspect.nodes[0] ? `unit-${initialProjectInspect.nodes[0].id}` : null);
  const [selectedInstanceIds, setSelectedInstanceIds] = useState<string[]>([]);
  const [activeScene, setActiveScene] = useState<string | null>(null);
  const [selectedRouteIndex, setSelectedRouteIndex] = useState<number | null>(null);
  const [canvasFitRevision, setCanvasFitRevision] = useState(0);
  const [inspectorView, setInspectorView] = useState<InspectorView>(initialRouteWorkspacePath ? 'contract' : 'project');
  const [canvasMode, setCanvasMode] = useState<CanvasMode>(initialRouteNode ? 'contract' : 'project');
  const [selectedAtomId, setSelectedAtomId] = useState<string | null>(null);
  const [atomClipboard, setAtomClipboard] = useState<UnitGraphNode | null>(null);
  const [graphEditError, setGraphEditError] = useState<string | null>(null);
  const [paramDrafts, setParamDrafts] = useState(() => buildParamDrafts(initialProjectInspect));
  const [paramOriginals, setParamOriginals] = useState(() => buildParamOriginals(initialProjectInspect));
  const [entryProject, setEntryProject] = useState(initialWorkspace.entryProject);
  const [workspaceFiles, setWorkspaceFilesState] = useState(initialWorkspace.files);
  const setWorkspaceFiles = useCallback((update: WorkspaceFile[] | ((files: WorkspaceFile[]) => WorkspaceFile[])) => {
    incrementPerfCounter('state.workspace.dispatches');
    setWorkspaceFilesState(update);
  }, []);
  const [selectedWorkspacePath, setSelectedWorkspacePath] = useState(
    initialRouteWorkspacePath ?? initialWorkspace.entryProject,
  );
  const appliedRoutePath = useRef<string | null>(null);
  const undoStack = useRef<WorkspaceHistoryEntry[]>([]);
  const redoStack = useRef<WorkspaceHistoryEntry[]>([]);
  const autosaveTimeout = useRef<number | null>(null);
  const lastSavedWorkspace = useRef<string>(JSON.stringify(createWorkspacePayload(initialWorkspace.entryProject, initialWorkspace.files)));
  const [workspaceSaveError, setWorkspaceSaveError] = useState<string | null>(null);
  const [historyCounts, setHistoryCounts] = useState({ undo: 0, redo: 0 });
  const currentHistoryEntry = useCallback((): WorkspaceHistoryEntry => ({
    entryProject,
    files: workspaceFiles,
    selectedWorkspacePath,
    selectedId,
    selectedRouteIndex,
    selectedAtomId,
    canvasMode,
    inspectorView,
  }), [canvasMode, entryProject, inspectorView, selectedAtomId, selectedId, selectedRouteIndex, selectedWorkspacePath, workspaceFiles]);
  const restoreHistoryEntry = useCallback((entry: WorkspaceHistoryEntry) => {
    setEntryProject(entry.entryProject);
    setWorkspaceFiles(entry.files);
    setSelectedWorkspacePath(entry.selectedWorkspacePath);
    setSelectedId(entry.selectedId);
    setSelectedRouteIndex(entry.selectedRouteIndex);
    setSelectedAtomId(entry.selectedAtomId);
    setCanvasMode(entry.canvasMode);
    setInspectorView(entry.inspectorView);
    navigate(entry.files.find(file => file.path === entry.selectedWorkspacePath)?.role === 'unit'
      ? unitRoute(entry.selectedWorkspacePath)
      : PROJECT_ROUTE);
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
  }, [navigate, setWorkspaceFiles]);
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
  const project = useMemo(
    () => projectDraftToInspect(projectDraft, backendSamples.project, projectWorkspaceFile.path),
    [projectDraft, projectWorkspaceFile.path],
  );
  const selectedNode = findUnitNode(nodes, selectedId);
  const selectedRoute = selectedRouteIndex === null ? null : project.routes[selectedRouteIndex] ?? null;
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
  const selectedAtom =
    selectedUnitGraph?.nodes.find(node => node.id === selectedAtomId) ?? selectedUnitGraph?.nodes[0] ?? null;

  useEffect(() => {
    if (appliedRoutePath.current === location.pathname) return;
    appliedRoutePath.current = location.pathname;

    if (location.pathname === '/' || location.pathname === '') {
      navigate(PROJECT_ROUTE, { replace: true });
      return;
    }

    if (location.pathname === PROJECT_ROUTE) {
      setSelectedWorkspacePath(entryProject);
      setCanvasMode('project');
      setInspectorView('project');
      return;
    }

    const path = workspacePathFromRoute(location.pathname, workspaceFiles);
    if (!path) {
      navigate(PROJECT_ROUTE, { replace: true });
      return;
    }

    const unit = project.units.find(item => resolveWorkspacePath(project.file, item.file) === path);
    const node = unit ? project.nodes.find(item => item.unit === unit.id) : null;
    setSelectedWorkspacePath(path);
    setSelectedRouteIndex(null);
    setSelectedAtomId(null);
    setSelectedId(node ? `unit-${node.id}` : null);
    setCanvasMode(node ? 'contract' : 'project');
    setInspectorView('contract');
  }, [entryProject, location.pathname, navigate, project.file, project.nodes, project.units, workspaceFiles]);

  const selectWorkspaceFile = useCallback((path: string) => {
    const file = workspaceFiles.find(item => item.path === path);
    navigate(file?.role === 'unit' ? unitRoute(path) : PROJECT_ROUTE);
  }, [navigate, workspaceFiles]);

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
        }))];
      } catch {
        return [reference.id, []];
      }
    }),
  ), [projectDraft.units, projectWorkspaceFile.path, workspaceFiles]);

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
      nodes: projectDraft.nodes.map(node => [node.id, node.unit, node.ui?.position?.x ?? null, node.ui?.position?.y ?? null]),
      routes: project.routes.map(route => [route.from, route.to]),
    }),
    [project.routes, projectDraft.nodes],
  );
  const graphTopologyRef = useRef('');
  const graphLayoutRevisionRef = useRef(0);

  useEffect(() => {
    markPerfSpan('graph.sync.project', () => {
      const topologyChanged = graphTopologyRef.current !== graphTopologySignature;
      const replaceWorkspaceLayout = graphLayoutRevisionRef.current !== canvasFitRevision;
      graphTopologyRef.current = graphTopologySignature;
      graphLayoutRevisionRef.current = canvasFitRevision;

      if (topologyChanged || replaceWorkspaceLayout) {
        const next = buildProjectGraph(project, projectDraft);
        setNodes(current => next.nodes.map(node => {
          const existing = current.find(item => item.id === node.id);
          let data = node.data;
          let storedPosition: ProjectGraphPosition | undefined;
          if (node.data.kind === 'unit') {
            const unitData = node.data;
            data = { ...unitData, paramControls: projectParamControls[unitData.unit.id] ?? [], onParamChange: updateParamDraft };
            storedPosition = projectDraft.nodes.find(item => item.id === unitData.instance.id)?.ui?.position;
          }
          const position = storedPosition ?? (replaceWorkspaceLayout ? undefined : existing?.position) ?? node.position;
          if (existing
            && existing.position.x === position.x
            && existing.position.y === position.y
            && JSON.stringify(existing.data) === JSON.stringify(data)) {
            return existing;
          }
          return { ...node, data, position };
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
            paramControls: projectParamControls[unit.id] ?? [],
            onParamChange: updateParamDraft,
          };
          const previous = JSON.stringify({
            instance: node.data.instance,
            unit: node.data.unit,
            controls: node.data.paramControls,
          });
          const candidate = JSON.stringify({ instance, unit, controls: data.paramControls });
          if (previous === candidate) return node;
          changed = true;
          return { ...node, data };
        });
        return changed ? next : current;
      });
    });
  }, [canvasFitRevision, graphTopologySignature, project, projectDraft, projectParamControls, setEdges, setNodes, updateParamDraft]);

  useEffect(() => {
    if (typeof window === 'undefined') return;

    if (autosaveTimeout.current) {
      window.clearTimeout(autosaveTimeout.current);
    }

    autosaveTimeout.current = window.setTimeout(() => {
      try {
        markPerfSpan('workspace.autosave.persist', () => {
          const payload = createWorkspacePayload(entryProject, workspaceFiles);
          const serialized = JSON.stringify(payload);
          if (serialized !== lastSavedWorkspace.current) {
            lastSavedWorkspace.current = persistSerializedWorkspace(WORKSPACE_STORAGE_KEY, serialized, window.localStorage);
            onWorkspaceChange(payload);
          }
        });
        setWorkspaceSaveError(null);
      } catch (error) {
        setWorkspaceSaveError(error instanceof Error ? error.message : 'Unable to persist the workspace.');
      } finally {
        autosaveTimeout.current = null;
      }
    }, PERFORMANCE_DEBOUNCE_MS);

    return () => {
      if (autosaveTimeout.current) {
        window.clearTimeout(autosaveTimeout.current);
        autosaveTimeout.current = null;
      }
    };
  }, [entryProject, onWorkspaceChange, workspaceFiles]);

  const selectProjectNode = useCallback((id: string, additive = false) => {
    markPerfSpan('ui.select.projectNode', () => {
      setSelectedId(id);
      if (id.startsWith('unit-')) {
        const instanceId = id.slice('unit-'.length);
        setSelectedInstanceIds(current => additive
          ? current.includes(instanceId) ? current.filter(item => item !== instanceId) : [...current, instanceId]
          : [instanceId]);
      } else {
        setSelectedInstanceIds([]);
      }
      setSelectedRouteIndex(null);
      setSelectedAtomId(null);
      if (id.startsWith('unit-')) {
        setInspectorView('atom');
      }
    });
  }, []);

  const openContractGraph = useCallback((id: string) => {
    const node = nodes.find(item => item.id === id)?.data;
    if (!node || node.kind !== 'unit') return;

    const path = resolveWorkspacePath(project.file, node.unit.file);
    markPerfSpan('ui.openContractGraph', () => {
      setSelectedId(id);
      setSelectedRouteIndex(null);
      setSelectedWorkspacePath(path);
      setCanvasMode('contract');
      setInspectorView('contract');
      setSelectedAtomId(null);
      navigate(unitRoute(path));
    });
  }, [navigate, nodes, project.file]);

  const selectRoute = useCallback((index: number) => {
    markPerfSpan('ui.select.route', () => {
      setSelectedRouteIndex(index);
      setSelectedId(null);
      setCanvasMode('project');
      setSelectedAtomId(null);
    });
  }, []);

  const resetUnitParamDrafts = useCallback((instanceId: string) => {
    const instance = project.nodes.find(node => node.id === instanceId);
    if (!instance) return;

    setParamDrafts(drafts => {
      const next = { ...drafts };

      for (const param of instance.params) {
        const key = paramDraftKey(instance.id, param.key);
        next[key] = paramOriginals[key] ?? param.value;
      }

      return next;
    });
    setWorkspaceFiles(files =>
      files.map(file => {
        if (file.role !== 'project') return file;
        const content = instance.params.reduce(
          (draft, param) => {
            const key = paramDraftKey(instance.id, param.key);
            return updateProjectInstanceParam(draft, instance.id, param.key, paramOriginals[key] ?? param.value);
          },
          file.content,
        );
        return { ...file, content };
      }),
    );
  }, [paramOriginals, project.nodes, setWorkspaceFiles]);

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

  const addProjectNode = useCallback((unitId: string, instanceId: string, position?: ProjectGraphPosition) => {
    markPerfSpan('graph.add.projectNode', () => {
      const reference = projectDraft.units.find(unit => unit.id === unitId);
      if (!reference) return;
      const unitPath = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
      const unitFile = workspaceFiles.find(file => file.path === unitPath);
      const defaults = unitFile?.role === 'unit'
        ? Object.fromEntries(parseUnitGraphDraft(unitFile.content).params.map(param => [param.name, param.default]))
        : {};
      const result = addProjectInstance(projectWorkspaceFile.content, unitId, instanceId, defaults, position);
      if (!updateProjectFile(() => result.content)) return;
      setParamDrafts(values => ({ ...values, ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value])) }));
      setParamOriginals(values => ({ ...values, ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value])) }));
      setSelectedId(`unit-${result.id}`);
    });
  }, [projectDraft.units, projectWorkspaceFile.content, projectWorkspaceFile.path, updateProjectFile, workspaceFiles]);

  const addProjectNodeFromLibrary = useCallback((unitId: string, position?: ProjectGraphPosition) => {
    markPerfSpan('graph.add.projectNodeFromLibrary', () => {
      addProjectNode(unitId, uniqueInstanceId(project.nodes.map(node => node.id), unitId), position);
    });
  }, [addProjectNode, project.nodes]);

  const insertProjectNodeOnRoute = useCallback((
    unitId: string,
    routeIndex: number,
    position: ProjectGraphPosition,
  ) => {
    markPerfSpan('graph.insert.projectNode', () => {
      const reference = projectDraft.units.find(unit => unit.id === unitId);
      if (!reference) return;
      const unitPath = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
      const unitFile = workspaceFiles.find(file => file.path === unitPath);
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
        position,
      );
      if (!updateProjectFile(() => result.content)) return;
      const values = Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value]));
      setParamDrafts(current => ({ ...current, ...values }));
      setParamOriginals(current => ({ ...current, ...values }));
      setSelectedId(`unit-${result.id}`);
    });
  }, [project.nodes, projectDraft.units, projectPorts, projectWorkspaceFile.content, projectWorkspaceFile.path, updateProjectFile, workspaceFiles]);

  const addSimpleEffect = useCallback((item: EffectLibraryItem) => {
    markPerfSpan('graph.add.simpleEffect', () => {
      try {
        const source = libraryWorkspaceSource(item, personalUnits);
        if (!source) throw new Error(`Effect "${item.title}" is unavailable.`);
        const outputRouteIndex = project.routes.findIndex(route => route.to === 'system.output');
        if (outputRouteIndex < 0) throw new Error('Connect the board to Output before adding another effect.');
        const existing = projectDraft.units.find(reference => reference.id === `${item.id}_unit`
          || reference.file.endsWith(`/${item.id}.unit.v2.yaml`));
        const position = { x: 180 + project.nodes.length * 260, y: 220 };
        if (existing) {
          insertProjectNodeOnRoute(existing.id, outputRouteIndex, position);
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
          outputRouteIndex,
          defaults,
          position,
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
        setCanvasFitRevision(revision => revision + 1);
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

  const addSimpleParallelEffect = useCallback((item: EffectLibraryItem) => {
    markPerfSpan('graph.add.simpleParallelEffect', () => {
      try {
        const source = libraryWorkspaceSource(item, personalUnits);
        if (!source) throw new Error(`Effect "${item.title}" is unavailable.`);
        const outputRouteIndex = project.routes.findIndex(route => route.to === 'system.output');
        if (outputRouteIndex < 0) throw new Error('Connect the board to Output before adding a parallel effect.');

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

        let mixerReference = parseProjectGraphDraft(content).units.find(reference => reference.id === 'wet_dry_mix_unit');
        if (!mixerReference) {
          const mixerPath = 'units/wet_dry_mix.unit.v2.yaml';
          content = addProjectUnitReference(
            content,
            'wet_dry_mix_unit',
            relativeWorkspaceReference(projectWorkspaceFile.path, mixerPath),
          );
          mixerReference = { id: 'wet_dry_mix_unit', file: relativeWorkspaceReference(projectWorkspaceFile.path, mixerPath) };
          nextPorts.wet_dry_mix_unit = parseUnitPortNames(wetDryMixWorkspaceFile.content);
          filesToAdd.push({ ...wetDryMixWorkspaceFile, path: mixerPath });
        }

        const effectDefaults = Object.fromEntries(parseUnitGraphDraft(source.content).params.map(param => [param.name, param.default]));
        const mixerDefaults = Object.fromEntries(parseUnitGraphDraft(wetDryMixWorkspaceFile.content).params.map(param => [param.name, param.default]));
        const effectId = uniqueInstanceId(project.nodes.map(node => node.id), effectReference.id);
        const mixerId = uniqueInstanceId([...project.nodes.map(node => node.id), effectId], 'wet_dry_mix_unit');
        const positionX = 180 + project.nodes.length * 260;
        const result = insertProjectParallelOnRoute(
          content,
          nextPorts,
          effectReference.id,
          effectId,
          mixerReference.id,
          mixerId,
          outputRouteIndex,
          effectDefaults,
          mixerDefaults,
          { effect: { x: positionX, y: 100 }, mixer: { x: positionX + 230, y: 260 } },
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
          ...Object.entries(mixerDefaults).map(([key, value]) => [paramDraftKey(mixerId, key), value]),
        ]);
        setParamDrafts(current => ({ ...current, ...values }));
        setParamOriginals(current => ({ ...current, ...values }));
        setSelectedId(`unit-${effectId}`);
        setCanvasFitRevision(revision => revision + 1);
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
    setWorkspaceFiles,
  ]);

  const duplicateProjectNode = useCallback((instanceId: string) => {
    markPerfSpan('graph.duplicate.projectNode', () => {
      try {
        const result = duplicateProjectInstance(projectWorkspaceFile.content, instanceId);
        const source = project.nodes.find(node => node.id === instanceId);
        if (!source || !updateProjectFile(() => result.content)) return;
        const values = Object.fromEntries(source.params.map(param => [paramDraftKey(result.id, param.key), param.value]));
        setParamDrafts(current => ({ ...current, ...values }));
        setParamOriginals(current => ({ ...current, ...values }));
        setSelectedId(`unit-${result.id}`);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : String(error));
      }
    });
  }, [project.nodes, projectWorkspaceFile.content, updateProjectFile]);

  const renameProjectNode = useCallback((instanceId: string, nextId: string) => {
    markPerfSpan('graph.rename.projectNode', () => {
      if (!updateProjectFile(content => renameProjectInstance(content, instanceId, nextId))) return;
      const migrate = (values: Record<string, string>) => Object.fromEntries(Object.entries(values).map(([key, value]) => [
        key.startsWith(`${instanceId}.`) ? `${nextId}${key.slice(instanceId.length)}` : key,
        value,
      ]));
      setParamDrafts(migrate);
      setParamOriginals(migrate);
      setSelectedId(`unit-${nextId}`);
    });
  }, [updateProjectFile]);

  const removeProjectNode = useCallback((instanceId: string) => {
    markPerfSpan('graph.remove.projectNode', () => {
      if (!updateProjectFile(content => removeProjectInstance(content, instanceId))) return;
      const removeValues = (values: Record<string, string>) => Object.fromEntries(
        Object.entries(values).filter(([key]) => !key.startsWith(`${instanceId}.`)),
      );
      setParamDrafts(removeValues);
      setParamOriginals(removeValues);
      setSelectedId(null);
    });
  }, [updateProjectFile]);

  const saveScene = useCallback((name: string) => {
    const params = Object.fromEntries(project.nodes.flatMap(node => (
      node.params.map(param => [`${node.id}.${param.key}`, param.value])
    )));
    const bypass = Object.fromEntries(project.nodes.map(node => [
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
    const now = new Date().toISOString();
    onSavePersonalUnit({
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
    });
  }, [onSavePersonalUnit, projectPackage.manifest.name, selectedNode, selectedUnitName, selectedUnitWorkspaceFile]);

  const batchBypass = useCallback((enabled: boolean) => {
    for (const instanceId of selectedInstanceIds) void liveBypassController?.setBypass(instanceId, enabled);
  }, [liveBypassController, selectedInstanceIds]);

  const removeSelectedInstances = useCallback(() => {
    if (selectedInstanceIds.length < 2) return;
    const removed = new Set(selectedInstanceIds);
    if (!updateProjectFile(content => selectedInstanceIds.reduce(
      (current, instanceId) => removeProjectInstance(current, instanceId),
      content,
    ))) return;
    const removeValues = (values: Record<string, string>) => Object.fromEntries(
      Object.entries(values).filter(([key]) => !removed.has(key.split('.')[0])),
    );
    setParamDrafts(removeValues);
    setParamOriginals(removeValues);
    setSelectedInstanceIds([]);
    setSelectedId(null);
  }, [selectedInstanceIds, updateProjectFile]);

  const reorderProjectNode = useCallback((instanceId: string, nextIndex: number) => {
    markPerfSpan('graph.reorder.projectNode', () => {
      updateProjectFile(content => moveProjectInstance(content, instanceId, nextIndex));
    });
  }, [updateProjectFile]);

  const moveProjectNode = useCallback((instanceId: string, position: ProjectGraphPosition) => {
    markPerfSpan('graph.move.projectNode', () => {
      updateProjectFile(content => setProjectInstancePosition(content, instanceId, position));
    });
  }, [updateProjectFile]);

  const updateProjectRoute = useCallback((index: number, route: ProjectRouteDraft) => {
    markPerfSpan('graph.update.route', () => {
      updateProjectFile(content => replaceProjectRoute(content, projectPorts, index, route));
    });
  }, [projectPorts, updateProjectFile]);

  const createProjectRoute = useCallback((route: ProjectRouteDraft) => {
    markPerfSpan('graph.create.route', () => {
      updateProjectFile(content => addProjectRoute(content, projectPorts, route));
    });
  }, [projectPorts, updateProjectFile]);

  const deleteProjectRoute = useCallback((index: number) => {
    markPerfSpan('graph.delete.route', () => {
      if (!updateProjectFile(content => removeProjectRoute(content, index))) return;
      setSelectedRouteIndex(null);
    });
  }, [updateProjectFile]);

  const reorderProjectRoute = useCallback((index: number, nextIndex: number) => {
    markPerfSpan('graph.reorder.route', () => {
      if (!updateProjectFile(content => moveProjectRoute(content, index, nextIndex))) return;
      setSelectedRouteIndex(Math.max(0, Math.min(project.routes.length - 1, nextIndex)));
    });
  }, [project.routes.length, updateProjectFile]);

  const routeSources = useMemo(() => [
    'system.input',
    ...project.nodes.flatMap(node => (projectPorts[node.unit]?.outputs ?? []).map(port => `${node.id}.${port}`)),
  ], [project.nodes, projectPorts]);
  const routeTargets = useMemo(() => [
    ...project.nodes.flatMap(node => (projectPorts[node.unit]?.inputs ?? []).map(port => `${node.id}.${port}`)),
    'system.output',
  ], [project.nodes, projectPorts]);

  const updateWorkspaceFile = useCallback((path: string, content: string) => {
    markPerfSpan('workspace.update.raw', () => {
      pushHistory();
      setWorkspaceFiles(files => files.map(file => (file.path === path ? { ...file, content } : file)));
    });
  }, [pushHistory, setWorkspaceFiles]);

  const createUnit = useCallback((name: string) => {
    markPerfSpan('project.create.unitFile', () => {
      const content = createUnitV2({ name });
      const unitName = parseUnitGraphDraft(content).name;
      const path = `workspace/${unitName}.unit.v2.yaml`;
      if (workspaceFiles.some(file => file.path === path)) throw new Error(`Workspace file "${path}" already exists.`);
      pushHistory();
      setWorkspaceFiles(files => [...files, { path, role: 'unit', content, originalContent: '' }]);
      setSelectedWorkspacePath(path);
      setSelectedId(null);
      setSelectedRouteIndex(null);
      setSelectedAtomId(null);
      setInspectorView('contract');
      setCanvasMode('project');
      navigate(unitRoute(path));
    });
  }, [navigate, pushHistory, setWorkspaceFiles, workspaceFiles]);

  const updateSelectedUnitFile = useCallback((update: (content: string) => string, nextAtomId?: string | null) => {
    markPerfSpan('graph.update.unitFile', () => {
      setGraphEditError(null);
      try {
        const content = update(selectedUnitWorkspaceFile.content);
        pushHistory();
        setWorkspaceFiles(files =>
          files.map(file => (file.path === selectedUnitWorkspaceFile.path ? { ...file, content } : file)),
        );
        if (nextAtomId !== undefined) setSelectedAtomId(nextAtomId);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to update unit graph.');
      }
    });
  }, [pushHistory, selectedUnitWorkspaceFile.content, selectedUnitWorkspaceFile.path, setWorkspaceFiles]);

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
        const result = addAtomNodeToUnit(selectedUnitWorkspaceFile.content, backendSamples.atomCatalog, atomName, position);
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to add atom.');
      }
    });
  }, [selectedUnitWorkspaceFile.content, updateSelectedUnitFile]);

  const insertAtomOnConnection = useCallback((
    atomName: string,
    target: UnitConnectionEndpoint,
    position: UnitGraphPosition,
  ) => {
    markPerfSpan('contract.insert.atom', () => {
      try {
        const result = insertAtomNodeOnConnection(
          selectedUnitWorkspaceFile.content,
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
  }, [selectedUnitWorkspaceFile.content, updateSelectedUnitFile]);

  const removeSelectedAtom = useCallback(() => {
    if (!selectedAtom) return;
    markPerfSpan('contract.remove.atom', () => {
      updateSelectedUnitFile(content => removeAtomNodeFromUnit(content, selectedAtom.id), null);
    });
  }, [selectedAtom, updateSelectedUnitFile]);

  const replaceSelectedAtom = useCallback((nodeId: string, nextAtomName: string, preserveId: boolean) => {
    markPerfSpan('contract.replace.atom', () => {
      try {
        const result = replaceAtomNodeInUnit(selectedUnitWorkspaceFile.content, backendSamples.atomCatalog, nodeId, nextAtomName, preserveId);
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to replace atom.');
      }
    });
  }, [selectedUnitWorkspaceFile.content, updateSelectedUnitFile]);

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

  const copySelectedAtom = useCallback(() => {
    if (selectedAtom) setAtomClipboard(selectedAtom);
  }, [selectedAtom]);

  const cutSelectedAtom = useCallback(() => {
    if (!selectedAtom) return;
    markPerfSpan('contract.cut.atom', () => {
      setAtomClipboard(selectedAtom);
      updateSelectedUnitFile(content => removeAtomNodeFromUnit(content, selectedAtom.id), null);
    });
  }, [selectedAtom, updateSelectedUnitFile]);

  const pasteAtom = useCallback(() => {
    if (!atomClipboard) return;

    markPerfSpan('contract.paste.atom', () => {
      try {
        const result = pasteAtomNodeIntoUnit(selectedUnitWorkspaceFile.content, atomClipboard);
        updateSelectedUnitFile(() => result.content, result.id);
      } catch (error) {
        setGraphEditError(error instanceof Error ? error.message : 'Unable to paste atom.');
      }
    });
  }, [atomClipboard, selectedUnitWorkspaceFile.content, updateSelectedUnitFile]);

  const selectAtom = useCallback((id: string) => {
    setSelectedAtomId(id);
  }, []);

  const openAtomInspector = useCallback((id: string) => {
    markPerfSpan('ui.openAtomInspector', () => {
      setSelectedAtomId(id);
      setInspectorView('contract');
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
      setCanvasFitRevision(revision => revision + 1);
      navigate(PROJECT_ROUTE);
    });
  }, [initialProjectInspect, initialWorkspace, navigate, pushHistory, setWorkspaceFiles]);

  const saveWorkspace = useCallback(() => {
    try {
      markPerfSpan('workspace.save', () => {
        const payload = createWorkspacePayload(entryProject, workspaceFiles);
        if (typeof window === 'undefined') return;
        lastSavedWorkspace.current = persistWorkspacePayload(WORKSPACE_STORAGE_KEY, payload, window.localStorage);
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
    markPerfSpan('workspace.import', () => onImportProject(file));
  }, [onImportProject]);

  const updateReadiness = useCallback((update: Partial<ApgProjectPackage['readiness']>) => {
    onProjectPackageChange(current => {
      const readiness = {
        ...current.readiness,
        ...update,
        targets: update.targets ?? current.readiness.targets,
        diagnostics: update.diagnostics ?? current.readiness.diagnostics,
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

  const perfSpans = readPerfSpans(20);
  const renderPerfSpans = readPerfRenderSpans(20);

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
          project={project}
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
          onModeChange={onModeChange}
          onHome={onHome}
          onTour={() => setTourOpen(true)}
          packagedAudio={projectPackage.audio}
          readiness={projectPackage.readiness}
          onAudioAssetChange={updatePackagedAudio}
          onReadinessUpdate={updateReadiness}
        />

      <div className="layout">
        {mode === 'simple' ? (
          <SimpleLibraryPanel
            items={simpleEffectLibrary}
            onAdd={addSimpleEffect}
            onAddParallel={addSimpleParallelEffect}
            onDeletePersonal={onDeletePersonalUnit}
          />
        ) : (
          <ProjectSidebar
          project={project}
          workspaceFiles={workspaceFiles}
          selectedWorkspacePath={selectedWorkspacePath}
          selectedNodeId={selectedId}
          selectedRouteIndex={selectedRouteIndex}
          onSelectWorkspaceFile={selectWorkspaceFile}
          onCreateUnit={createUnit}
          onAddInstance={addProjectNode}
          onAddUnitFromLibrary={addProjectNodeFromLibrary}
          onAddRoute={createProjectRoute}
          onSelectNode={selectProjectNode}
          onOpenContractGraph={openContractGraph}
          onSelectRoute={selectRoute}
          routeSources={routeSources}
          routeTargets={routeTargets}
          selectedInstanceIds={selectedInstanceIds}
          onToggleBatchInstance={instanceId => setSelectedInstanceIds(current => (
            current.includes(instanceId) ? current.filter(id => id !== instanceId) : [...current, instanceId]
          ))}
          />
        )}

        {mode === 'pro' && canvasMode === 'contract' && selectedNode?.kind === 'unit' ? (
          <Profiler id="ContractGraphCanvas" onRender={handleRenderProfile}>
            <ContractGraphCanvas
              catalog={backendSamples.atomCatalog}
              selectedAtomId={selectedAtomId}
              selectedUnitLabel={selectedNode.unit.name}
              workspaceFile={selectedUnitWorkspaceFile}
              onBackToProject={() => markPerfSpan('ui.returnToProject', () => navigate(PROJECT_ROUTE))}
              onAddAtomAt={addAtom}
              onInsertAtomAtEdge={insertAtomOnConnection}
              onMoveAtom={moveAtom}
              onConnectAtoms={connectAtoms}
              onDisconnectAtom={disconnectAtom}
              onOpenAtomInspector={openAtomInspector}
              onReconnectAtoms={reconnectAtoms}
              onSelectAtom={selectAtom}
            />
          </Profiler>
        ) : (
          <Profiler id="ProjectCanvas" onRender={handleRenderProfile}>
            <ProjectCanvas
              nodes={nodes}
              edges={edges}
              fitViewRevision={canvasFitRevision}
              selectedRouteIndex={selectedRouteIndex}
              onNodesChange={onNodesChange}
              onEdgesChange={onEdgesChange}
              onSelectNode={selectProjectNode}
              onOpenContractGraph={openContractGraph}
              onSelectRoute={selectRoute}
              onAddUnitAt={addProjectNodeFromLibrary}
              onInsertUnitAtRoute={insertProjectNodeOnRoute}
              onMoveUnit={moveProjectNode}
            />
          </Profiler>
        )}

        {mode === 'simple' ? (
          <SimpleInspector
            onApplyPreset={applyPreset}
            onDeletePreset={onDeletePreset}
            onDuplicate={duplicateProjectNode}
            onOpenPro={() => {
              onModeChange('pro');
              if (selectedId) openContractGraph(selectedId);
            }}
            onRemove={removeProjectNode}
            onSavePreset={savePreset}
            onSaveToLibrary={saveSelectedUnitToLibrary}
            presets={selectedUnitPresets}
            selectedNode={selectedNode}
          />
        ) : (
          <Profiler id="ProjectInspector" onRender={handleRenderProfile}>
            <ProjectInspector
            validation={backendSamples.validation}
            render={backendSamples.render}
            commands={backendCommands}
            project={project}
            inspectorView={inspectorView}
            onInspectorViewChange={value => {
              markPerfSpan('ui.change.inspectorView', () => setInspectorView(value));
            }}
            selectedNode={selectedNode}
            selectedRoute={selectedRoute}
            selectedRouteIndex={selectedRouteIndex}
            unit={backendSamples.unit}
            atomCatalog={backendSamples.atomCatalog}
            atomCatalogManifest={backendSamples.atomCatalogManifest}
            projectFile={project.file}
            hasDirtyParamDrafts={hasDirtyDrafts}
            selectedUnitFile={selectedUnitWorkspaceFile}
            selectedUnitGraph={selectedUnitGraph}
            selectedAtom={selectedAtom}
            atomClipboard={atomClipboard}
            graphEditError={graphEditError}
            paramOverrides={paramOverrides}
            perfSpans={perfSpans}
            renderPerfSpans={renderPerfSpans}
            onAddAtom={addAtom}
            onDuplicateInstance={duplicateProjectNode}
            onRemoveInstance={removeProjectNode}
            onRenameInstance={renameProjectNode}
            onReorderInstance={reorderProjectNode}
            onReorderUnitParam={reorderUnitParam}
            onUpdateRoute={updateProjectRoute}
            onRemoveRoute={deleteProjectRoute}
            onReorderRoute={reorderProjectRoute}
            routeSources={routeSources}
            routeTargets={routeTargets}
            onCopyAtom={copySelectedAtom}
            onCutAtom={cutSelectedAtom}
            onPasteAtom={pasteAtom}
            onRemoveAtom={removeSelectedAtom}
            onReplaceAtom={replaceSelectedAtom}
            onResetUnitParams={resetUnitParamDrafts}
            onSelectAtom={setSelectedAtomId}
            onSelectedAtomChange={updateSelectedAtom}
            onWorkspaceFileChange={updateWorkspaceFile}
            onSaveToLibrary={saveSelectedUnitToLibrary}
            />
          </Profiler>
        )}
      </div>
      <BatchActionBar
        count={mode === 'pro' ? selectedInstanceIds.length : 0}
        liveReady={Boolean(liveBypassController)}
        onBypass={batchBypass}
        onClear={() => setSelectedInstanceIds([])}
        onRemove={removeSelectedInstances}
      />
      <SceneBar
        activeScene={activeScene}
        onApply={applyScene}
        onDelete={deleteScene}
        onRename={renameScene}
        onSave={saveScene}
        scenes={project.scenes}
      />
      {mode === 'simple' && graphEditError ? <p className="simple-edit-error" role="alert">{graphEditError}</p> : null}
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
