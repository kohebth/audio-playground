import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { useEdgesState, useNodesState, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { ContractGraphCanvas } from './components/ContractGraphCanvas';
import { ProjectCanvas } from './components/ProjectCanvas';
import { ProjectInspector } from './components/ProjectInspector';
import { ProjectSidebar } from './components/ProjectSidebar';
import { ProjectTopbar } from './components/ProjectTopbar';
import { backendCommands, backendSamples, initialWorkspaceFiles, sampleSources, type WorkspaceFile } from './lib/backendSamples';
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
  duplicateProjectInstance,
  moveProjectInstance,
  moveProjectRoute,
  parseProjectGraphDraft,
  parseUnitPortNames,
  projectDraftToInspect,
  removeProjectInstance,
  removeProjectRoute,
  renameProjectInstance,
  replaceProjectRoute,
  type ProjectPortCatalog,
  type ProjectRouteDraft,
} from './lib/projectV2Graph';
import {
  addAtomNodeToUnit,
  connectUnitNodes,
  createUnitV2,
  disconnectUnitInput,
  pasteAtomNodeIntoUnit,
  parseUnitGraphDraft,
  removeAtomNodeFromUnit,
  reconnectUnitConnection,
  serializeUnitGraphNodeUpdate,
  updateProjectInstanceParam,
  type UnitGraphNode,
  type UnitConnectionEndpoint,
} from './lib/unitV2Graph';
import {
  createWorkspacePayload,
  hydrateWorkspaceFiles,
  parseWorkspacePayload,
  validateWorkspacePayload,
  WORKSPACE_FORMAT_VERSION,
  WORKSPACE_SCHEMA,
} from './lib/workspacePersistence';
import './App.css';

function findUnitNode(nodes: Node<ProjectNodeData>[], id: string | null): ProjectNodeData | null {
  if (!id) return null;
  return nodes.find(node => node.id === id)?.data ?? null;
}

type InspectorView = 'project' | 'atom' | 'contract';
type CanvasMode = 'project' | 'contract';

const WORKSPACE_STORAGE_KEY = 'apg.unit-editor.workspace.v2';
const LEGACY_WORKSPACE_STORAGE_KEY = 'apg.unit-editor.workspace.v1';

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

function loadWorkspaceState(): { entryProject: string; files: WorkspaceFile[] } {
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

export default function App() {
  const [initialWorkspace] = useState(loadWorkspaceState);
  const initialProjectInspect = useMemo(() => {
    try {
      const file = initialWorkspace.files.find(item => item.path === initialWorkspace.entryProject);
      if (!file) return backendSamples.project;
      return projectDraftToInspect(parseProjectGraphDraft(file.content), backendSamples.project, file.path);
    } catch {
      return backendSamples.project;
    }
  }, [initialWorkspace]);
  const initialGraph = useMemo(() => buildProjectGraph(initialProjectInspect), [initialProjectInspect]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initialGraph.edges);
  const [liveBypassController, setLiveBypassController] = useState<LiveBypassController | null>(null);
  const [selectedId, setSelectedId] = useState<string | null>(() =>
    initialProjectInspect.nodes[0] ? `unit-${initialProjectInspect.nodes[0].id}` : null);
  const [selectedRouteIndex, setSelectedRouteIndex] = useState<number | null>(null);
  const [inspectorView, setInspectorView] = useState<InspectorView>('project');
  const [canvasMode, setCanvasMode] = useState<CanvasMode>('project');
  const [selectedAtomId, setSelectedAtomId] = useState<string | null>(null);
  const [atomClipboard, setAtomClipboard] = useState<UnitGraphNode | null>(null);
  const [graphEditError, setGraphEditError] = useState<string | null>(null);
  const [paramDrafts, setParamDrafts] = useState(() => buildParamDrafts(initialProjectInspect));
  const [paramOriginals, setParamOriginals] = useState(() => buildParamOriginals(initialProjectInspect));
  const [entryProject, setEntryProject] = useState(initialWorkspace.entryProject);
  const [workspaceFiles, setWorkspaceFiles] = useState(initialWorkspace.files);
  const [selectedWorkspacePath, setSelectedWorkspacePath] = useState(initialWorkspace.entryProject);
  const projectWorkspaceFile = workspaceFiles.find(file => file.path === entryProject) ?? workspaceFiles[0];
  const lastValidProjectDraft = useRef(parseProjectGraphDraft(initialWorkspaceFiles[0].content));
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
  }, []);

  const graphTopologySignature = useMemo(
    () => JSON.stringify({
      nodes: project.nodes.map(node => [node.id, node.unit]),
      routes: project.routes.map(route => [route.from, route.to]),
    }),
    [project.nodes, project.routes],
  );
  const graphTopologyRef = useRef('');

  useEffect(() => {
    const topologyChanged = graphTopologyRef.current !== graphTopologySignature;
    graphTopologyRef.current = graphTopologySignature;

    if (topologyChanged) {
      const next = buildProjectGraph(project);
      setNodes(current => next.nodes.map(node => {
        const positioned = current.find(item => item.id === node.id);
        const data = node.data.kind === 'unit'
          ? { ...node.data, paramControls: projectParamControls[node.data.unit.id] ?? [], onParamChange: updateParamDraft }
          : node.data;
        return positioned ? { ...node, data, position: positioned.position } : { ...node, data };
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
  }, [graphTopologySignature, project, projectParamControls, setEdges, setNodes, updateParamDraft]);

  useEffect(() => {
    window.localStorage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(createWorkspacePayload(entryProject, workspaceFiles)));
  }, [entryProject, workspaceFiles]);

  const selectProjectNode = useCallback((id: string) => {
    setSelectedId(id);
    setSelectedRouteIndex(null);
    setSelectedAtomId(null);
    if (id.startsWith('unit-')) {
      setInspectorView('atom');
    }
  }, []);

  const openContractGraph = useCallback((id: string) => {
    const node = nodes.find(item => item.id === id)?.data;
    if (!node || node.kind !== 'unit') return;

    const path = resolveWorkspacePath(project.file, node.unit.file);
    setSelectedId(id);
    setSelectedRouteIndex(null);
    setSelectedWorkspacePath(path);
    setCanvasMode('contract');
    setInspectorView('contract');
    setSelectedAtomId(null);
  }, [nodes, project.file]);

  const selectRoute = useCallback((index: number) => {
    setSelectedRouteIndex(index);
    setSelectedId(null);
    setCanvasMode('project');
    setSelectedAtomId(null);
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
  }, [paramOriginals, project.nodes]);

  const updateProjectFile = useCallback((update: (content: string) => string) => {
    setGraphEditError(null);
    try {
      const content = update(projectWorkspaceFile.content);
      setWorkspaceFiles(files => files.map(file =>
        file.path === projectWorkspaceFile.path ? { ...file, content } : file));
      return content;
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : String(error));
      return null;
    }
  }, [projectWorkspaceFile.content, projectWorkspaceFile.path]);

  const addProjectNode = useCallback((unitId: string, instanceId: string) => {
    const reference = projectDraft.units.find(unit => unit.id === unitId);
    if (!reference) return;
    const unitPath = resolveWorkspacePath(projectWorkspaceFile.path, reference.file);
    const unitFile = workspaceFiles.find(file => file.path === unitPath);
    const defaults = unitFile?.role === 'unit'
      ? Object.fromEntries(parseUnitGraphDraft(unitFile.content).params.map(param => [param.name, param.default]))
      : {};
    const result = addProjectInstance(projectWorkspaceFile.content, unitId, instanceId, defaults);
    if (!updateProjectFile(() => result.content)) return;
    setParamDrafts(values => ({ ...values, ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value])) }));
    setParamOriginals(values => ({ ...values, ...Object.fromEntries(Object.entries(defaults).map(([key, value]) => [paramDraftKey(result.id, key), value])) }));
    setSelectedId(`unit-${result.id}`);
  }, [projectDraft.units, projectWorkspaceFile.content, projectWorkspaceFile.path, updateProjectFile, workspaceFiles]);

  const duplicateProjectNode = useCallback((instanceId: string) => {
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
  }, [project.nodes, projectWorkspaceFile.content, updateProjectFile]);

  const renameProjectNode = useCallback((instanceId: string, nextId: string) => {
    if (!updateProjectFile(content => renameProjectInstance(content, instanceId, nextId))) return;
    const migrate = (values: Record<string, string>) => Object.fromEntries(Object.entries(values).map(([key, value]) => [
      key.startsWith(`${instanceId}.`) ? `${nextId}${key.slice(instanceId.length)}` : key,
      value,
    ]));
    setParamDrafts(migrate);
    setParamOriginals(migrate);
    setSelectedId(`unit-${nextId}`);
  }, [updateProjectFile]);

  const removeProjectNode = useCallback((instanceId: string) => {
    if (!updateProjectFile(content => removeProjectInstance(content, instanceId))) return;
    const removeValues = (values: Record<string, string>) => Object.fromEntries(
      Object.entries(values).filter(([key]) => !key.startsWith(`${instanceId}.`)),
    );
    setParamDrafts(removeValues);
    setParamOriginals(removeValues);
    setSelectedId(null);
  }, [updateProjectFile]);

  const reorderProjectNode = useCallback((instanceId: string, nextIndex: number) => {
    updateProjectFile(content => moveProjectInstance(content, instanceId, nextIndex));
  }, [updateProjectFile]);

  const updateProjectRoute = useCallback((index: number, route: ProjectRouteDraft) => {
    updateProjectFile(content => replaceProjectRoute(content, projectPorts, index, route));
  }, [projectPorts, updateProjectFile]);

  const createProjectRoute = useCallback((route: ProjectRouteDraft) => {
    updateProjectFile(content => addProjectRoute(content, projectPorts, route));
  }, [projectPorts, updateProjectFile]);

  const deleteProjectRoute = useCallback((index: number) => {
    if (!updateProjectFile(content => removeProjectRoute(content, index))) return;
    setSelectedRouteIndex(null);
  }, [updateProjectFile]);

  const reorderProjectRoute = useCallback((index: number, nextIndex: number) => {
    if (!updateProjectFile(content => moveProjectRoute(content, index, nextIndex))) return;
    setSelectedRouteIndex(Math.max(0, Math.min(project.routes.length - 1, nextIndex)));
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
    setWorkspaceFiles(files => files.map(file => (file.path === path ? { ...file, content } : file)));
  }, []);

  const createUnit = useCallback((name: string) => {
    const content = createUnitV2({ name });
    const unitName = parseUnitGraphDraft(content).name;
    const path = `workspace/${unitName}.unit.v2.yaml`;
    if (workspaceFiles.some(file => file.path === path)) throw new Error(`Workspace file "${path}" already exists.`);
    setWorkspaceFiles(files => [...files, { path, role: 'unit', content, originalContent: '' }]);
    setSelectedWorkspacePath(path);
    setSelectedId(null);
    setSelectedRouteIndex(null);
    setSelectedAtomId(null);
    setInspectorView('contract');
    setCanvasMode('project');
  }, [workspaceFiles]);

  const updateSelectedUnitFile = useCallback((update: (content: string) => string, nextAtomId?: string | null) => {
    setGraphEditError(null);
    try {
      const content = update(selectedUnitWorkspaceFile.content);
      setWorkspaceFiles(files =>
        files.map(file => (file.path === selectedUnitWorkspaceFile.path ? { ...file, content } : file)),
      );
      if (nextAtomId !== undefined) setSelectedAtomId(nextAtomId);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to update unit graph.');
    }
  }, [selectedUnitWorkspaceFile.content, selectedUnitWorkspaceFile.path]);

  const updateSelectedAtom = useCallback((node: UnitGraphNode, originalId = node.id) => {
    updateSelectedUnitFile(content => serializeUnitGraphNodeUpdate(content, node, originalId), node.id);
  }, [updateSelectedUnitFile]);

  const addAtom = useCallback((atomName: string) => {
    try {
      const result = addAtomNodeToUnit(selectedUnitWorkspaceFile.content, backendSamples.atomCatalog, atomName);
      updateSelectedUnitFile(() => result.content, result.id);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to add atom.');
    }
  }, [selectedUnitWorkspaceFile.content, updateSelectedUnitFile]);

  const removeSelectedAtom = useCallback(() => {
    if (!selectedAtom) return;
    updateSelectedUnitFile(content => removeAtomNodeFromUnit(content, selectedAtom.id), null);
  }, [selectedAtom, updateSelectedUnitFile]);

  const connectAtoms = useCallback((source: UnitConnectionEndpoint, target: UnitConnectionEndpoint) => {
    updateSelectedUnitFile(content => connectUnitNodes(content, backendSamples.atomCatalog, source, target));
  }, [updateSelectedUnitFile]);

  const reconnectAtoms = useCallback((
    previousTarget: UnitConnectionEndpoint,
    source: UnitConnectionEndpoint,
    target: UnitConnectionEndpoint,
  ) => {
    updateSelectedUnitFile(content =>
      reconnectUnitConnection(content, backendSamples.atomCatalog, previousTarget, source, target));
  }, [updateSelectedUnitFile]);

  const disconnectAtom = useCallback((target: UnitConnectionEndpoint) => {
    updateSelectedUnitFile(content => disconnectUnitInput(content, target));
  }, [updateSelectedUnitFile]);

  const copySelectedAtom = useCallback(() => {
    if (selectedAtom) setAtomClipboard(selectedAtom);
  }, [selectedAtom]);

  const cutSelectedAtom = useCallback(() => {
    if (!selectedAtom) return;
    setAtomClipboard(selectedAtom);
    updateSelectedUnitFile(content => removeAtomNodeFromUnit(content, selectedAtom.id), null);
  }, [selectedAtom, updateSelectedUnitFile]);

  const pasteAtom = useCallback(() => {
    if (!atomClipboard) return;

    try {
      const result = pasteAtomNodeIntoUnit(selectedUnitWorkspaceFile.content, atomClipboard);
      updateSelectedUnitFile(() => result.content, result.id);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Unable to paste atom.');
    }
  }, [atomClipboard, selectedUnitWorkspaceFile.content, updateSelectedUnitFile]);

  const selectAtom = useCallback((id: string) => {
    setSelectedAtomId(id);
  }, []);

  const openAtomInspector = useCallback((id: string) => {
    setSelectedAtomId(id);
    setInspectorView('contract');
  }, []);

  const resetWorkspace = useCallback(() => {
    setWorkspaceFiles(initialWorkspaceFiles);
    setEntryProject(backendSamples.project.file);
    setSelectedWorkspacePath(initialWorkspaceFiles[0].path);
    setParamDrafts(buildParamDrafts(backendSamples.project));
    setParamOriginals(buildParamOriginals(backendSamples.project));
    setSelectedId('unit-drive1');
    setSelectedRouteIndex(null);
    window.localStorage.removeItem(WORKSPACE_STORAGE_KEY);
    window.localStorage.removeItem(LEGACY_WORKSPACE_STORAGE_KEY);
  }, []);

  const exportWorkspace = useCallback(() => {
    const payload = createWorkspacePayload(entryProject, workspaceFiles);
    const url = URL.createObjectURL(new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = 'audio-playground-workspace.json';
    link.click();
    URL.revokeObjectURL(url);
  }, [entryProject, workspaceFiles]);

  const importWorkspace = useCallback(async (file: File | null) => {
    if (!file) return;

    try {
      const payload = parseWorkspacePayload(await file.text());
      const imported = hydrateWorkspaceFiles(payload, initialWorkspaceFiles);
      const importedProject = imported.find(item => item.path === payload.entryProject);
      if (!importedProject) return;
      const importedDraft = parseProjectGraphDraft(importedProject.content);
      const importedInspect = projectDraftToInspect(importedDraft, backendSamples.project, payload.entryProject);
      setWorkspaceFiles(imported);
      setEntryProject(payload.entryProject);
      setSelectedWorkspacePath(payload.entryProject);
      setParamDrafts(buildParamDrafts(importedInspect));
      setParamOriginals(buildParamOriginals(importedInspect));
      setSelectedId(null);
      setSelectedRouteIndex(null);
    } catch (error) {
      setGraphEditError(error instanceof Error ? error.message : 'Workspace import failed.');
    }
  }, []);

  return (
    <LiveBypassContext.Provider value={{ controller: liveBypassController, setController: setLiveBypassController }}>
    <div className="app app--project">
        <ProjectTopbar
          project={project}
          validation={backendSamples.validation}
          dirtyParamCount={dirtyParamCount + workspaceDraftCount}
          hasDirtyParamDrafts={hasDirtyDrafts}
          hasWorkspaceDrafts={hasWorkspaceDrafts}
          workspaceFileCount={workspaceFiles.length}
          onExportWorkspace={exportWorkspace}
          onImportWorkspace={importWorkspace}
          onResetWorkspace={resetWorkspace}
        />

      <div className="layout">
        <ProjectSidebar
          project={project}
          sampleSources={sampleSources}
          workspaceFiles={workspaceFiles}
          selectedWorkspacePath={selectedWorkspacePath}
          selectedNodeId={selectedId}
          selectedRouteIndex={selectedRouteIndex}
          onSelectWorkspaceFile={setSelectedWorkspacePath}
          onCreateUnit={createUnit}
          onAddInstance={addProjectNode}
          onAddRoute={createProjectRoute}
          onSelectNode={selectProjectNode}
          onOpenContractGraph={openContractGraph}
          onSelectRoute={selectRoute}
          routeSources={routeSources}
          routeTargets={routeTargets}
        />

        {canvasMode === 'contract' && selectedNode?.kind === 'unit' ? (
          <ContractGraphCanvas
            catalog={backendSamples.atomCatalog}
            selectedAtomId={selectedAtomId}
            selectedUnitLabel={selectedNode.unit.name}
            workspaceFile={selectedUnitWorkspaceFile}
            onBackToProject={() => setCanvasMode('project')}
            onConnectAtoms={connectAtoms}
            onDisconnectAtom={disconnectAtom}
            onOpenAtomInspector={openAtomInspector}
            onReconnectAtoms={reconnectAtoms}
            onSelectAtom={selectAtom}
          />
        ) : (
          <ProjectCanvas
            nodes={nodes}
            edges={edges}
            selectedRouteIndex={selectedRouteIndex}
            onNodesChange={onNodesChange}
            onEdgesChange={onEdgesChange}
            onSelectNode={selectProjectNode}
            onOpenContractGraph={openContractGraph}
            onSelectRoute={selectRoute}
          />
        )}

        <ProjectInspector
          validation={backendSamples.validation}
          render={backendSamples.render}
          commands={backendCommands}
          project={project}
          inspectorView={inspectorView}
          onInspectorViewChange={setInspectorView}
          selectedNode={selectedNode}
          selectedRoute={selectedRoute}
          selectedRouteIndex={selectedRouteIndex}
          unit={backendSamples.unit}
          atomCatalog={backendSamples.atomCatalog}
          atomCatalogManifest={backendSamples.atomCatalogManifest}
          projectFile={project.file}
          workspaceFiles={workspaceFiles}
          hasDirtyParamDrafts={hasDirtyDrafts}
          selectedUnitFile={selectedUnitWorkspaceFile}
          selectedUnitGraph={selectedUnitGraph}
          selectedAtom={selectedAtom}
          atomClipboard={atomClipboard}
          graphEditError={graphEditError}
          paramOverrides={paramOverrides}
          onAddAtom={addAtom}
          onDuplicateInstance={duplicateProjectNode}
          onRemoveInstance={removeProjectNode}
          onRenameInstance={renameProjectNode}
          onReorderInstance={reorderProjectNode}
          onUpdateRoute={updateProjectRoute}
          onRemoveRoute={deleteProjectRoute}
          onReorderRoute={reorderProjectRoute}
          routeSources={routeSources}
          routeTargets={routeTargets}
          onCopyAtom={copySelectedAtom}
          onCutAtom={cutSelectedAtom}
          onPasteAtom={pasteAtom}
          onRemoveAtom={removeSelectedAtom}
          onResetUnitParams={resetUnitParamDrafts}
          onSelectAtom={setSelectedAtomId}
          onSelectedAtomChange={updateSelectedAtom}
          onWorkspaceFileChange={updateWorkspaceFile}
        />
      </div>
    </div>
    </LiveBypassContext.Provider>
  );
}
