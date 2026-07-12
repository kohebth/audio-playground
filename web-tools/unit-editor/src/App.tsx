import { useCallback, useEffect, useMemo, useState } from 'react';
import { useEdgesState, useNodesState, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { ContractGraphCanvas } from './components/ContractGraphCanvas';
import { ProjectCanvas } from './components/ProjectCanvas';
import { ProjectInspector } from './components/ProjectInspector';
import { ProjectSidebar } from './components/ProjectSidebar';
import { ProjectTopbar } from './components/ProjectTopbar';
import { backendCommands, backendSamples, initialWorkspaceFiles, sampleSources, type WorkspaceFile } from './lib/backendSamples';
import { buildProjectGraph, type ProjectNodeData } from './lib/projectGraph';
import { buildParamDrafts, buildParamOverrides, countDirtyParams, paramDraftKey } from './lib/projectParams';
import {
  addAtomNodeToUnit,
  pasteAtomNodeIntoUnit,
  parseUnitGraphDraft,
  removeAtomNodeFromUnit,
  serializeUnitGraphNodeUpdate,
  updateProjectInstanceParam,
  type UnitGraphNode,
} from './lib/unitV2Graph';
import './App.css';

function findUnitNode(nodes: Node<ProjectNodeData>[], id: string | null): ProjectNodeData | null {
  if (!id) return null;
  return nodes.find(node => node.id === id)?.data ?? null;
}

type InspectorView = 'project' | 'atom' | 'contract';
type CanvasMode = 'project' | 'contract';

const WORKSPACE_STORAGE_KEY = 'apg.unit-editor.workspace.v1';

function normalizeWorkspacePath(path: string): string {
  return path.replace(/^\.\.\//, '').replace(/^\.\//, '');
}

function loadWorkspaceFiles(): WorkspaceFile[] {
  if (typeof window === 'undefined') return initialWorkspaceFiles;

  const saved = window.localStorage.getItem(WORKSPACE_STORAGE_KEY);
  if (!saved) return initialWorkspaceFiles;

  try {
    const files = JSON.parse(saved) as Array<Pick<WorkspaceFile, 'path' | 'role' | 'content'>>;
    return initialWorkspaceFiles.map(file => {
      const draft = files.find(item => item.path === file.path);
      return draft ? { ...file, content: draft.content } : file;
    });
  } catch {
    return initialWorkspaceFiles;
  }
}

export default function App() {
  const initialGraph = useMemo(() => buildProjectGraph(backendSamples.project), []);
  const [nodes, , onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, , onEdgesChange] = useEdgesState(initialGraph.edges);
  const [selectedId, setSelectedId] = useState<string | null>('unit-drive1');
  const [selectedRouteIndex, setSelectedRouteIndex] = useState<number | null>(null);
  const [inspectorView, setInspectorView] = useState<InspectorView>('project');
  const [canvasMode, setCanvasMode] = useState<CanvasMode>('project');
  const [selectedAtomId, setSelectedAtomId] = useState<string | null>(null);
  const [atomClipboard, setAtomClipboard] = useState<UnitGraphNode | null>(null);
  const [graphEditError, setGraphEditError] = useState<string | null>(null);
  const [paramDrafts, setParamDrafts] = useState(() => buildParamDrafts(backendSamples.project));
  const [workspaceFiles, setWorkspaceFiles] = useState(loadWorkspaceFiles);
  const [selectedWorkspacePath, setSelectedWorkspacePath] = useState(initialWorkspaceFiles[0].path);
  const selectedNode = findUnitNode(nodes, selectedId);
  const selectedRoute = selectedRouteIndex === null ? null : backendSamples.project.routes[selectedRouteIndex] ?? null;
  const dirtyParamCount = useMemo(() => countDirtyParams(backendSamples.project, paramDrafts), [paramDrafts]);
  const workspaceDraftCount = workspaceFiles.filter(file => file.content !== file.originalContent).length;
  const hasWorkspaceDrafts = workspaceDraftCount > 0;
  const hasDirtyDrafts = dirtyParamCount > 0 || workspaceDraftCount > 0;
  const selectedWorkspaceFile = workspaceFiles.find(file => file.path === selectedWorkspacePath) ?? workspaceFiles[0];
  const paramOverrides = useMemo(() => buildParamOverrides(backendSamples.project, paramDrafts), [paramDrafts]);
  const selectedUnitWorkspaceFile = useMemo(() => {
    if (selectedNode?.kind !== 'unit') return selectedWorkspaceFile;

    const path = normalizeWorkspacePath(selectedNode.unit.file);
    return workspaceFiles.find(file => file.path === path) ?? selectedWorkspaceFile;
  }, [selectedNode, selectedWorkspaceFile, workspaceFiles]);
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
    const drafts = workspaceFiles.map(({ path, role, content }) => ({ path, role, content }));
    window.localStorage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(drafts));
  }, [workspaceFiles]);

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

    const path = normalizeWorkspacePath(node.unit.file);
    setSelectedId(id);
    setSelectedRouteIndex(null);
    setSelectedWorkspacePath(path);
    setCanvasMode('contract');
    setInspectorView('contract');
    setSelectedAtomId(null);
  }, [nodes]);

  const selectRoute = useCallback((index: number) => {
    setSelectedRouteIndex(index);
    setSelectedId(null);
    setCanvasMode('project');
    setSelectedAtomId(null);
  }, []);

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

  const resetParamDraft = updateParamDraft;

  const resetUnitParamDrafts = useCallback((instanceId: string) => {
    const instance = backendSamples.project.nodes.find(node => node.id === instanceId);
    if (!instance) return;

    setParamDrafts(drafts => {
      const next = { ...drafts };

      for (const param of instance.params) {
        next[paramDraftKey(instance.id, param.key)] = param.value;
      }

      return next;
    });
    setWorkspaceFiles(files =>
      files.map(file => {
        if (file.role !== 'project') return file;
        const content = instance.params.reduce(
          (draft, param) => updateProjectInstanceParam(draft, instance.id, param.key, param.value),
          file.content,
        );
        return { ...file, content };
      }),
    );
  }, []);

  const updateWorkspaceFile = useCallback((path: string, content: string) => {
    setWorkspaceFiles(files => files.map(file => (file.path === path ? { ...file, content } : file)));
  }, []);

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
    setSelectedWorkspacePath(initialWorkspaceFiles[0].path);
    window.localStorage.removeItem(WORKSPACE_STORAGE_KEY);
  }, []);

  const exportWorkspace = useCallback(() => {
    const payload = {
      schema: 'apg.ui.workspace.v1',
      files: workspaceFiles.map(({ path, role, content }) => ({ path, role, content })),
    };
    const url = URL.createObjectURL(new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = 'audio-playground-workspace.json';
    link.click();
    URL.revokeObjectURL(url);
  }, [workspaceFiles]);

  const importWorkspace = useCallback(async (file: File | null) => {
    if (!file) return;

    try {
      const payload = JSON.parse(await file.text()) as { files?: Array<Pick<WorkspaceFile, 'path' | 'role' | 'content'>> };
      if (!Array.isArray(payload.files)) return;

      setWorkspaceFiles(
        initialWorkspaceFiles.map(workspaceFile => {
          const draft = payload.files?.find(item => item.path === workspaceFile.path);
          return draft ? { ...workspaceFile, content: draft.content } : workspaceFile;
        }),
      );
    } catch {
      return;
    }
  }, []);

  return (
    <div className="app app--project">
        <ProjectTopbar
          project={backendSamples.project}
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
          project={backendSamples.project}
          sampleSources={sampleSources}
          workspaceFiles={workspaceFiles}
          selectedWorkspacePath={selectedWorkspacePath}
          selectedNodeId={selectedId}
          selectedRouteIndex={selectedRouteIndex}
          onSelectWorkspaceFile={setSelectedWorkspacePath}
          onSelectNode={selectProjectNode}
          onOpenContractGraph={openContractGraph}
          onSelectRoute={selectRoute}
        />

        {canvasMode === 'contract' && selectedNode?.kind === 'unit' ? (
          <ContractGraphCanvas
            catalog={backendSamples.atomCatalog}
            selectedAtomId={selectedAtomId}
            selectedUnitLabel={selectedNode.unit.name}
            workspaceFile={selectedUnitWorkspaceFile}
            onBackToProject={() => setCanvasMode('project')}
            onOpenAtomInspector={openAtomInspector}
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
          project={backendSamples.project}
          inspectorView={inspectorView}
          onInspectorViewChange={setInspectorView}
          selectedNode={selectedNode}
          selectedRoute={selectedRoute}
          unit={backendSamples.unit}
          atomCatalog={backendSamples.atomCatalog}
          atomCatalogManifest={backendSamples.atomCatalogManifest}
          projectFile={backendSamples.project.file}
          workspaceFiles={workspaceFiles}
          hasDirtyParamDrafts={hasDirtyDrafts}
          selectedUnitFile={selectedUnitWorkspaceFile}
          selectedUnitGraph={selectedUnitGraph}
          selectedAtom={selectedAtom}
          atomClipboard={atomClipboard}
          graphEditError={graphEditError}
          paramDrafts={paramDrafts}
          paramOverrides={paramOverrides}
          onAddAtom={addAtom}
          onCopyAtom={copySelectedAtom}
          onCutAtom={cutSelectedAtom}
          onPasteAtom={pasteAtom}
          onParamChange={updateParamDraft}
          onParamReset={resetParamDraft}
          onRemoveAtom={removeSelectedAtom}
          onResetUnitParams={resetUnitParamDrafts}
          onSelectAtom={setSelectedAtomId}
          onSelectedAtomChange={updateSelectedAtom}
          onWorkspaceFileChange={updateWorkspaceFile}
        />
      </div>
    </div>
  );
}
