import { useCallback, useMemo, useState } from 'react';
import {
  Background,
  Controls,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  useEdgesState,
  useNodesState,
  type Node,
  type NodeTypes,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import { ProjectNode } from './components/ProjectNode';
import { backendSamples, sampleSources } from './lib/backendSamples';
import { buildProjectGraph, type ProjectNodeData } from './lib/projectGraph';
import './App.css';

const nodeTypes = { projectNode: ProjectNode } satisfies NodeTypes;

function compatibilityLabel(flags: Record<string, boolean>): string {
  const enabled = Object.entries(flags)
    .filter(([, value]) => value)
    .map(([key]) => key);
  return enabled.length > 0 ? enabled.join(', ') : 'none';
}

function formatNumber(value: number): string {
  return value.toFixed(6).replace(/0+$/, '').replace(/\.$/, '');
}

function findUnitNode(nodes: Node<ProjectNodeData>[], id: string | null): ProjectNodeData | null {
  if (!id) return null;
  return nodes.find(node => node.id === id)?.data ?? null;
}

export default function App() {
  const initialGraph = useMemo(() => buildProjectGraph(backendSamples.project), []);
  const [nodes, , onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, , onEdgesChange] = useEdgesState(initialGraph.edges);
  const [selectedId, setSelectedId] = useState<string | null>('unit-drive1');
  const selectedNode = findUnitNode(nodes, selectedId);

  const selectProjectNode = useCallback((id: string) => {
    setSelectedId(id);
  }, []);

  const projectStats = backendSamples.project.compiled;
  const render = backendSamples.render.output;
  const validation = backendSamples.validation;

  return (
    <div className="app app--project">
      <header className="topbar topbar--project">
        <div className="topbar__brand">
          <span className="topbar__logo">APG</span>
          <div>
            <div className="topbar__title">Audio Playground</div>
            <div className="topbar__subtitle">v2 project workbench</div>
          </div>
        </div>

        <div className="project-summary" aria-label="Project summary">
          <div>
            <span className="project-summary__label">Project</span>
            <strong>{backendSamples.project.name}</strong>
          </div>
          <div>
            <span className="project-summary__label">Target</span>
            <strong>{backendSamples.project.targets.default}</strong>
          </div>
          <div>
            <span className="project-summary__label">Compiled</span>
            <strong>{projectStats.nodes} nodes / {projectStats.signals} signals</strong>
          </div>
        </div>

        <div className={`status-pill ${validation.ok ? 'status-pill--ok' : 'status-pill--bad'}`}>
          {validation.ok ? 'Valid' : 'Invalid'}
        </div>
      </header>

      <div className="layout">
        <aside className="project-sidebar">
          <div className="project-sidebar__header">
            <span className="project-sidebar__title">Pedalboard</span>
            <span className="project-sidebar__count">{backendSamples.project.nodes.length} units</span>
          </div>

          <div className="project-list">
            {backendSamples.project.nodes.map((instance, index) => {
              const unit = backendSamples.project.units.find(item => item.id === instance.unit);
              const nodeId = `unit-${instance.id}`;

              return (
                <button
                  key={instance.id}
                  className={`project-list__item ${selectedId === nodeId ? 'project-list__item--active' : ''}`}
                  onClick={() => selectProjectNode(nodeId)}
                  type="button"
                >
                  <span className="project-list__index">{index + 1}</span>
                  <span className="project-list__main">
                    <span className="project-list__name">{instance.id}</span>
                    <span className="project-list__unit">{unit?.name ?? instance.unit}</span>
                  </span>
                  <span className="project-list__params">{instance.params.length}</span>
                </button>
              );
            })}
          </div>

          <div className="sample-ledger">
            <div className="sample-ledger__title">Frozen Sources</div>
            {Object.entries(sampleSources).map(([key, path]) => (
              <div key={key} className="sample-ledger__row">
                <span>{key}</span>
                <code>{path}</code>
              </div>
            ))}
          </div>
        </aside>

        <main className="canvas canvas--project">
          <div className="flow-shell">
            <ReactFlowProvider>
              <ReactFlow
                nodes={nodes}
                edges={edges}
                nodeTypes={nodeTypes}
                onNodesChange={onNodesChange}
                onEdgesChange={onEdgesChange}
                onNodeClick={(_, node) => selectProjectNode(node.id)}
                fitView
                fitViewOptions={{ padding: 0.16 }}
                minZoom={0.35}
                maxZoom={1.5}
              >
                <Background color="#35312b" gap={24} />
                <Controls />
                <MiniMap
                  nodeColor={node => (node.data as ProjectNodeData).color}
                  pannable
                  zoomable
                  style={{ background: '#171512' }}
                />
              </ReactFlow>
            </ReactFlowProvider>
          </div>
        </main>

        <aside className="project-inspector">
          <section className="inspector-block">
            <div className="inspector-block__label">Validation</div>
            <div className="validation-line">
              <span className={`validation-dot ${validation.ok ? 'validation-dot--ok' : 'validation-dot--bad'}`} />
              <strong>{validation.ok ? 'Project is valid' : 'Project has errors'}</strong>
            </div>
            <div className="inspector-block__meta">
              {validation.errors.length} errors / {validation.warnings.length} warnings
            </div>
          </section>

          <section className="inspector-block">
            <div className="inspector-block__label">Render Preview</div>
            <div className="meter-grid">
              <div>
                <span>Peak</span>
                <strong>{formatNumber(render.peak)}</strong>
              </div>
              <div>
                <span>RMS</span>
                <strong>{formatNumber(render.rms)}</strong>
              </div>
              <div>
                <span>Frames</span>
                <strong>{backendSamples.render.frames}</strong>
              </div>
            </div>
            <div className="waveform" aria-label="Deterministic render samples">
              {render.samples.map((sample, index) => (
                <span
                  key={`${sample}-${index}`}
                  className="waveform__bar"
                  style={{ height: `${Math.max(8, Math.abs(sample) * 120)}px` }}
                />
              ))}
            </div>
          </section>

          {selectedNode?.kind === 'unit' ? (
            <section className="inspector-block inspector-block--selected">
              <div className="inspector-block__label">Selected Unit</div>
              <h2>{selectedNode.instance.id}</h2>
              <p>{selectedNode.unit.name}</p>

              <div className="param-list">
                {selectedNode.instance.params.map(param => (
                  <div key={param.key} className="param-list__row">
                    <span>{param.key}</span>
                    <strong>{param.value}</strong>
                  </div>
                ))}
              </div>

              <div className="compatibility">
                <span>Compatibility</span>
                <strong>{compatibilityLabel(selectedNode.unit.compatibility)}</strong>
              </div>
            </section>
          ) : (
            <section className="inspector-block inspector-block--selected">
              <div className="inspector-block__label">Selected Node</div>
              <h2>{selectedNode?.label ?? 'Nothing selected'}</h2>
              <p>{selectedNode?.detail ?? 'Select a pedalboard unit to inspect its parameters.'}</p>
            </section>
          )}
        </aside>
      </div>
    </div>
  );
}
