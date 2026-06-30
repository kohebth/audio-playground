import { useCallback, useMemo, useState } from 'react';
import { useEdgesState, useNodesState, type Node } from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { ProjectCanvas } from './components/ProjectCanvas';
import { ProjectInspector } from './components/ProjectInspector';
import { ProjectSidebar } from './components/ProjectSidebar';
import { ProjectTopbar } from './components/ProjectTopbar';
import { backendSamples, sampleSources } from './lib/backendSamples';
import { buildProjectGraph, type ProjectNodeData } from './lib/projectGraph';
import { buildParamDrafts, countDirtyParams, paramDraftKey } from './lib/projectParams';
import './App.css';

function findUnitNode(nodes: Node<ProjectNodeData>[], id: string | null): ProjectNodeData | null {
  if (!id) return null;
  return nodes.find(node => node.id === id)?.data ?? null;
}

export default function App() {
  const initialGraph = useMemo(() => buildProjectGraph(backendSamples.project), []);
  const [nodes, , onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, , onEdgesChange] = useEdgesState(initialGraph.edges);
  const [selectedId, setSelectedId] = useState<string | null>('unit-drive1');
  const [selectedRouteIndex, setSelectedRouteIndex] = useState<number | null>(null);
  const [paramDrafts, setParamDrafts] = useState(() => buildParamDrafts(backendSamples.project));
  const selectedNode = findUnitNode(nodes, selectedId);
  const selectedRoute = selectedRouteIndex === null ? null : backendSamples.project.routes[selectedRouteIndex] ?? null;
  const dirtyParamCount = useMemo(() => countDirtyParams(backendSamples.project, paramDrafts), [paramDrafts]);

  const selectProjectNode = useCallback((id: string) => {
    setSelectedId(id);
    setSelectedRouteIndex(null);
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

  return (
    <div className="app app--project">
      <ProjectTopbar
        project={backendSamples.project}
        validation={backendSamples.validation}
        dirtyParamCount={dirtyParamCount}
      />

      <div className="layout">
        <ProjectSidebar
          project={backendSamples.project}
          sampleSources={sampleSources}
          selectedNodeId={selectedId}
          selectedRouteIndex={selectedRouteIndex}
          onSelectNode={selectProjectNode}
          onSelectRoute={selectRoute}
        />

        <ProjectCanvas
          nodes={nodes}
          edges={edges}
          selectedRouteIndex={selectedRouteIndex}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          onSelectNode={selectProjectNode}
          onSelectRoute={selectRoute}
        />

        <ProjectInspector
          validation={backendSamples.validation}
          render={backendSamples.render}
          selectedNode={selectedNode}
          selectedRoute={selectedRoute}
          unit={backendSamples.unit}
          atomCatalog={backendSamples.atomCatalog}
          paramDrafts={paramDrafts}
          onParamChange={updateParamDraft}
          onParamReset={resetParamDraft}
          onResetUnitParams={resetUnitParamDrafts}
        />
      </div>
    </div>
  );
}
