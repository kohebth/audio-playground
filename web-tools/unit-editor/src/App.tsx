import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  ReactFlow,
  Background,
  Controls,
  MiniMap,
  useNodesState,
  useEdgesState,
  addEdge,
  type Connection,
  type Node,
  type Edge,
  type NodeTypes,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import type { UnitConfig } from './types';
import { parseYaml } from './lib/yamlParser';
import { serializeYaml } from './lib/yamlSerializer';
import { buildGraph } from './lib/graphBuilder';
import { AtomNode } from './components/AtomNode';
import { Inspector } from './components/Inspector';
import { PipelineSidebar } from './components/PipelineSidebar';
import { UnitHeader } from './components/UnitHeader';
import { YamlPreview } from './components/YamlPreview';
import './App.css';

const nodeTypes: NodeTypes = { atomNode: AtomNode as any };

const EMPTY_UNIT: UnitConfig = {
  name: 'new_unit',
  version: '1.0.0',
  params: [],
  signals: ['input', 'output'],
  pipeline: [],
};

export default function App() {
  const [unit, setUnit] = useState<UnitConfig>(EMPTY_UNIT);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node>([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState<Edge>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [showYaml, setShowYaml] = useState(false);
  const [isDragOver, setIsDragOver] = useState(false);
  const fileRef = useRef<HTMLInputElement>(null);

  // Rebuild graph whenever unit changes
  useEffect(() => {
    const { nodes: n, edges: e } = buildGraph(unit);
    setNodes(n);
    setEdges(e);
  }, [unit]);

  const onConnect = useCallback((params: Connection) => setEdges(eds => addEdge(params, eds)), []);

  const onNodeClick = useCallback((_: React.MouseEvent, node: any) => {
    setSelectedId(node.data.stage.id);
  }, []);

  const loadFile = (text: string) => {
    try {
      setUnit(parseYaml(text));
      setSelectedId(null);
    } catch (e) {
      alert('Failed to parse YAML: ' + (e as Error).message);
    }
  };

  const handleFileInput = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = ev => loadFile(ev.target?.result as string);
    reader.readAsText(file);
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragOver(false);
    const file = e.dataTransfer.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = ev => loadFile(ev.target?.result as string);
    reader.readAsText(file);
  };

  const exportYaml = () => {
    const blob = new Blob([serializeYaml(unit)], { type: 'text/yaml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${unit.name}.unit.yaml`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const yamlText = useMemo(() => serializeYaml(unit), [unit]);

  return (
    <div
      className={`app ${isDragOver ? 'app--drag-over' : ''}`}
      onDragOver={e => { e.preventDefault(); setIsDragOver(true); }}
      onDragLeave={() => setIsDragOver(false)}
      onDrop={handleDrop}
    >
      {/* ── Top Bar ── */}
      <div className="topbar">
        <div className="topbar__brand">
          <span className="topbar__logo">◈</span>
          <span className="topbar__title">DSP Unit Editor</span>
        </div>
        <UnitHeader unit={unit} onChange={setUnit} />
        <div className="topbar__actions">
          <button className="btn btn--ghost" onClick={() => fileRef.current?.click()}>
            Open YAML
          </button>
          <input ref={fileRef} type="file" accept=".yaml,.yml" hidden onChange={handleFileInput} />
          <button className="btn btn--ghost" onClick={() => setShowYaml(v => !v)}>
            {showYaml ? 'Hide' : 'YAML'} Preview
          </button>
          <button className="btn btn--primary" onClick={exportYaml}>
            Export YAML
          </button>
        </div>
      </div>

      {/* ── Main Layout ── */}
      <div className="layout">
        {/* Left: Pipeline sidebar */}
        <PipelineSidebar
          unit={unit}
          selectedId={selectedId}
          onSelectId={setSelectedId}
          onChange={setUnit}
        />

        {/* Center: Node graph */}
        <main className="canvas">
          {unit.pipeline.length === 0 ? (
            <div className="canvas__empty">
              <div className="canvas__empty-icon">⊕</div>
              <p>Drop a <code>.unit.yaml</code> file here to get started</p>
              <p>or use the <strong>Open YAML</strong> button above</p>
            </div>
          ) : (
            <ReactFlow
              nodes={nodes}
              edges={edges}
              nodeTypes={nodeTypes}
              onNodesChange={onNodesChange}
              onEdgesChange={onEdgesChange}
              onConnect={onConnect}
              onNodeClick={onNodeClick}
              fitView
              fitViewOptions={{ padding: 0.1 }}
              minZoom={0.1}
              maxZoom={2}
            >
              <Background color="#1e293b" gap={20} />
              <Controls />
              <MiniMap
                nodeColor={(n: any) => n.data?.color ?? '#374151'}
                style={{ background: '#0f172a' }}
              />
            </ReactFlow>
          )}
        </main>

        {/* Right: Inspector */}
        <Inspector unit={unit} selectedId={selectedId} onChange={setUnit} />
      </div>

      {/* YAML Preview drawer */}
      {showYaml && (
        <div className="yaml-drawer">
          <YamlPreview yaml={yamlText} />
        </div>
      )}
    </div>
  );
}
