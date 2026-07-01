import { useCallback, useEffect, useMemo, useState } from 'react';
import { useEdgesState, useNodesState, type Node } from '@xyflow/react';
import yaml from 'js-yaml';
import '@xyflow/react/dist/style.css';

import { ProjectCanvas } from './components/ProjectCanvas';
import { ProjectInspector } from './components/ProjectInspector';
import { ProjectSidebar } from './components/ProjectSidebar';
import { ProjectTopbar } from './components/ProjectTopbar';
import {
  backendCommands,
  backendSamples,
  initialWorkspaceFiles,
  sampleSources,
  type UnitInspect,
  type WorkspaceFile,
} from './lib/backendSamples';
import { buildProjectGraph, type ProjectNodeData } from './lib/projectGraph';
import { buildParamDrafts, buildParamOverrides, countDirtyParams, paramDraftKey } from './lib/projectParams';
import './App.css';

function findUnitNode(nodes: Node<ProjectNodeData>[], id: string | null): ProjectNodeData | null {
  if (!id) return null;
  return nodes.find(node => node.id === id)?.data ?? null;
}

type InspectorView = 'project' | 'atom' | 'contract' | 'graph';

function normalizeWorkspacePath(path: string): string {
  return path.replace(/^\.\.\//, '').replace(/^\.\//, '');
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function parseUnitPorts(raw: unknown): Array<{ name: string; type: string; channels?: string }> {
  if (!Array.isArray(raw)) return [];

  return raw
    .filter(isObject)
    .map(port => ({
      name: String(port.name ?? 'signal'),
      type: String(port.type ?? 'audio'),
      channels: typeof port.channels === 'string' ? port.channels : undefined,
    }));
}

function parseWorkspaceUnitInspect(file: WorkspaceFile): UnitInspect | null {
  try {
    const doc = yaml.load(file.content) as Record<string, unknown>;
    if (!isObject(doc)) return null;

    const params = isObject(doc.params)
      ? Object.entries(doc.params).map(([name, raw]) => {
          const spec = isObject(raw) ? raw : {};

          return {
            name,
            type: String(spec.type ?? 'float'),
            default: String(spec.default ?? 0),
            min: spec.min === undefined ? undefined : String(spec.min),
            max: spec.max === undefined ? undefined : String(spec.max),
            smoothing_ms: spec.smoothing_ms === undefined ? undefined : String(spec.smoothing_ms),
          };
        })
      : [];

    const ports = isObject(doc.ports)
      ? {
          inputs: parseUnitPorts(doc.ports.inputs),
          outputs: parseUnitPorts(doc.ports.outputs),
        }
      : { inputs: [], outputs: [] };

    const graphRaw = isObject(doc.graph) ? doc.graph : {};
    const graphSignals = Array.isArray(graphRaw.signals)
      ? graphRaw.signals.filter((signal): signal is string => typeof signal === 'string')
      : [];

    const graphNodes = Array.isArray(graphRaw.nodes)
      ? graphRaw.nodes
          .filter(isObject)
          .map((node, index) => {
            const bindings = Object.fromEntries([
              ...Object.entries(isObject(node.in) ? node.in : {}).map(([k, v]) => [k, String(v)]),
              ...Object.entries(isObject(node.out) ? node.out : {}).map(([k, v]) => [k, String(v)]),
            ]);

            return {
              id: String(node.id ?? `node-${index}`),
              atom: String(node.atom ?? 'unknown'),
              bindings,
            };
          })
      : [];

    const compatibility = isObject(doc.compatibility)
      ? Object.fromEntries(
          Object.entries(doc.compatibility).map(([key, value]) => [key, Boolean(value)]),
        )
      : undefined;

    return {
      schema: String(doc.schema ?? 'apg.unit.v2'),
      file: file.path,
      name: String(doc.name ?? 'unit'),
      version: String(doc.version ?? '2.0.0'),
      meta: isObject(doc.meta) ? (doc.meta as Record<string, string>) : undefined,
      compatibility,
      params,
      ports,
      graph: { signals: graphSignals, nodes: graphNodes },
    };
  } catch {
    return null;
  }
}

const unitWorkspacePathByInstance = new Map<string, string>();

for (const unit of backendSamples.project.units) {
  const unitInstanceIds = backendSamples.project.nodes
    .filter(instance => instance.unit === unit.id)
    .map(instance => instance.id);

  for (const instanceId of unitInstanceIds) {
    unitWorkspacePathByInstance.set(instanceId, normalizeWorkspacePath(unit.file));
  }
}

const WORKSPACE_STORAGE_KEY = 'apg.unit-editor.workspace.v1';

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
  const selectedUnitForGraph =
    selectedNode?.kind === 'unit' && selectedWorkspaceFile.role === 'unit'
      ? parseWorkspaceUnitInspect(selectedWorkspaceFile)
      : null;
  const paramOverrides = useMemo(() => buildParamOverrides(backendSamples.project, paramDrafts), [paramDrafts]);

  useEffect(() => {
    const drafts = workspaceFiles.map(({ path, role, content }) => ({ path, role, content }));
    window.localStorage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(drafts));
  }, [workspaceFiles]);

  const selectProjectNode = useCallback((id: string) => {
    setSelectedId(id);
    setSelectedRouteIndex(null);
  }, []);

  const selectProjectNodeForGraph = useCallback((id: string) => {
    if (!id.startsWith('unit-')) {
      setSelectedId(id);
      return;
    }

    const instanceId = id.replace(/^unit-/, '');
    const workspacePath = unitWorkspacePathByInstance.get(instanceId);
    if (workspacePath) {
      setSelectedWorkspacePath(workspacePath);
    }

    setSelectedId(id);
    setSelectedRouteIndex(null);
    setInspectorView('graph');
  }, []);

  const selectRoute = useCallback((index: number) => {
    setSelectedRouteIndex(index);
    setSelectedId(null);
  }, []);

  const updateParamDraft = useCallback((instanceId: string, paramKey: string, value: string) => {
    setParamDrafts(drafts => ({ ...drafts, [paramDraftKey(instanceId, paramKey)]: value }));
  }, []);

  const resetParamDraft = useCallback((instanceId: string, paramKey: string, value: string) => {
    setParamDrafts(drafts => ({ ...drafts, [paramDraftKey(instanceId, paramKey)]: value }));
  }, []);

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
  }, []);

  const updateWorkspaceFile = useCallback((path: string, content: string) => {
    setWorkspaceFiles(files => files.map(file => (file.path === path ? { ...file, content } : file)));
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
          onDoubleSelectNode={selectProjectNodeForGraph}
          onSelectRoute={selectRoute}
        />

        <ProjectCanvas
          nodes={nodes}
          edges={edges}
          selectedRouteIndex={selectedRouteIndex}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          onSelectNode={selectProjectNode}
          onDoubleSelectNode={selectProjectNodeForGraph}
          onSelectRoute={selectRoute}
        />

        <ProjectInspector
          validation={backendSamples.validation}
          render={backendSamples.render}
          commands={backendCommands}
          project={backendSamples.project}
          inspectorView={inspectorView}
          onInspectorViewChange={setInspectorView}
          selectedNode={selectedNode}
          selectedRoute={selectedRoute}
          unit={selectedUnitForGraph ?? backendSamples.unit}
          atomCatalog={backendSamples.atomCatalog}
          atomCatalogManifest={backendSamples.atomCatalogManifest}
          projectFile={backendSamples.project.file}
          hasDirtyParamDrafts={hasDirtyDrafts}
          selectedWorkspaceFile={selectedWorkspaceFile}
          paramDrafts={paramDrafts}
          paramOverrides={paramOverrides}
          onParamChange={updateParamDraft}
          onParamReset={resetParamDraft}
          onResetUnitParams={resetUnitParamDrafts}
          onWorkspaceFileChange={updateWorkspaceFile}
        />
      </div>
    </div>
  );
}
