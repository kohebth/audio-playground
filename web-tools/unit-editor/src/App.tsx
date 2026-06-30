import { useCallback, useMemo, useState } from 'react';
import {
  Background,
  Controls,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  useEdgesState,
  useNodesState,
  type Edge,
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

function routeIndexFromEdge(edge: Edge): number | null {
  const match = edge.id.match(/^route-(\d+)-/);
  return match ? Number(match[1]) : null;
}

export default function App() {
  const initialGraph = useMemo(() => buildProjectGraph(backendSamples.project), []);
  const [nodes, , onNodesChange] = useNodesState<Node<ProjectNodeData>>(initialGraph.nodes);
  const [edges, , onEdgesChange] = useEdgesState(initialGraph.edges);
  const [selectedId, setSelectedId] = useState<string | null>('unit-drive1');
  const [selectedRouteIndex, setSelectedRouteIndex] = useState<number | null>(null);
  const selectedNode = findUnitNode(nodes, selectedId);
  const selectedRoute = selectedRouteIndex === null ? null : backendSamples.project.routes[selectedRouteIndex] ?? null;

  const selectProjectNode = useCallback((id: string) => {
    setSelectedId(id);
    setSelectedRouteIndex(null);
  }, []);

  const selectRoute = useCallback((index: number) => {
    setSelectedRouteIndex(index);
    setSelectedId(null);
  }, []);

  const displayedEdges = useMemo(() => edges.map(edge => {
    const selected = routeIndexFromEdge(edge) === selectedRouteIndex;
    return {
      ...edge,
      animated: selected,
      style: {
        ...edge.style,
        stroke: selected ? '#fbbf24' : '#94a3b8',
        strokeWidth: selected ? 2.6 : 1.6,
      },
    };
  }), [edges, selectedRouteIndex]);

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
          <section className="project-card">
            <span className="project-card__label">Loaded Project</span>
            <strong>{backendSamples.project.file}</strong>
            <span>{backendSamples.project.schema}</span>
          </section>

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

          <div className="route-list">
            <div className="route-list__header">
              <span>Routes</span>
              <strong>{backendSamples.project.routes.length}</strong>
            </div>
            {backendSamples.project.routes.map((route, index) => (
              <button
                key={`${index}-${route.from}-${route.to}`}
                className={`route-list__item ${selectedRouteIndex === index ? 'route-list__item--active' : ''}`}
                onClick={() => selectRoute(index)}
                type="button"
              >
                <span>{route.from}</span>
                <strong>{route.to}</strong>
              </button>
            ))}
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
                edges={displayedEdges}
                nodeTypes={nodeTypes}
                onNodesChange={onNodesChange}
                onEdgesChange={onEdgesChange}
                onNodeClick={(_, node) => selectProjectNode(node.id)}
                onEdgeClick={(_, edge) => {
                  const routeIndex = routeIndexFromEdge(edge);
                  if (routeIndex !== null) selectRoute(routeIndex);
                }}
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
            {validation.errors.length === 0 && validation.warnings.length === 0 ? (
              <div className="diagnostic-empty">No diagnostics in the frozen validation sample.</div>
            ) : (
              <div className="diagnostic-list">
                {[...validation.errors, ...validation.warnings].map((diagnostic, index) => (
                  <div key={`${diagnostic.code ?? 'diagnostic'}-${index}`} className="diagnostic-list__item">
                    <strong>{diagnostic.code ?? 'diagnostic'}</strong>
                    <span>{diagnostic.path ?? diagnostic.file ?? 'project'}</span>
                    <p>{diagnostic.message ?? 'No message'}</p>
                  </div>
                ))}
              </div>
            )}
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

          {selectedRoute ? (
            <section className="inspector-block inspector-block--selected">
              <div className="inspector-block__label">Selected Route</div>
              <h2>{selectedRoute.from}</h2>
              <p>{selectedRoute.to}</p>
            </section>
          ) : selectedNode?.kind === 'unit' ? (
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

              <div className="compatibility">
                <span>Unit Reference</span>
                <strong>{selectedNode.unit.file}</strong>
              </div>
            </section>
          ) : (
            <section className="inspector-block inspector-block--selected">
              <div className="inspector-block__label">Selected Node</div>
              <h2>{selectedNode?.label ?? 'Nothing selected'}</h2>
              <p>{selectedNode?.detail ?? 'Select a pedalboard unit to inspect its parameters.'}</p>
            </section>
          )}

          <section className="inspector-block">
            <div className="inspector-block__label">Backend Contract</div>
            <div className="contract-list">
              <div>
                <span>Unit sample</span>
                <strong>{backendSamples.unit.name}</strong>
              </div>
              <div>
                <span>Atom catalog bytes</span>
                <strong>{backendSamples.atomCatalog.bytes}</strong>
              </div>
              <div>
                <span>Atom catalog fnv1a64</span>
                <strong>{backendSamples.atomCatalog.fnv1a64}</strong>
              </div>
            </div>
          </section>
        </aside>
      </div>
    </div>
  );
}
