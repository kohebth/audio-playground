import { memo, useMemo, type CSSProperties } from 'react';
import {
  Background,
  Controls,
  Handle,
  MiniMap,
  Position,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type Node,
  type NodeProps,
  type NodeTypes,
} from '@xyflow/react';
import dagre from 'dagre';
import yaml from 'js-yaml';

import type { AtomCatalog, WorkspaceFile } from '../lib/backendSamples';

type ContractNodeData = {
  id: string;
  atom: string;
  category: string;
  in: Record<string, string>;
  out: Record<string, string>;
  config: Record<string, string>;
  color: string;
};

type ContractFlowNode = Node<ContractNodeData, 'contractNode'>;

type UnitGraphNode = {
  id: string;
  atom: string;
  in: Record<string, string>;
  out: Record<string, string>;
  config: Record<string, string>;
};

type UnitGraph = {
  name: string;
  signals: string[];
  nodes: UnitGraphNode[];
};

type Props = {
  workspaceFile: WorkspaceFile;
  catalog: AtomCatalog;
  selectedUnitLabel: string;
  onBackToProject: () => void;
};

type ParsedContractGraph = {
  unit: UnitGraph | null;
  error: string | null;
  flow: {
    nodes: ContractFlowNode[];
    edges: Edge[];
  };
};

const NODE_WIDTH = 220;
const NODE_HEIGHT = 136;

const CATEGORY_COLORS: Record<string, string> = {
  amplitude: '#10b981',
  delay: '#8b5cf6',
  detect: '#06b6d4',
  filter: '#3b82f6',
  generation: '#f59e0b',
  mix: '#ec4899',
  modulation: '#f97316',
  nonlinear: '#ef4444',
};

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function stringMap(value: unknown): Record<string, string> {
  if (!isObject(value)) return {};
  return Object.fromEntries(Object.entries(value).map(([key, raw]) => [key, String(raw)]));
}

function parseUnitGraph(content: string): UnitGraph {
  const doc = yaml.load(content);
  if (!isObject(doc)) throw new Error('Unit YAML must be a mapping.');

  const graph = isObject(doc.graph) ? doc.graph : {};
  const signals = Array.isArray(graph.signals) ? graph.signals.filter((item): item is string => typeof item === 'string') : [];
  const nodes = Array.isArray(graph.nodes)
    ? graph.nodes.filter(isObject).map((node, index) => ({
        id: String(node.id ?? `node-${index + 1}`),
        atom: String(node.atom ?? 'unknown'),
        in: stringMap(node.in),
        out: stringMap(node.out),
        config: stringMap(node.config),
      }))
    : [];

  return {
    name: String(doc.name ?? 'unnamed_unit'),
    signals,
    nodes,
  };
}

function buildContractFlow(unit: UnitGraph, catalog: AtomCatalog): { nodes: ContractFlowNode[]; edges: Edge[] } {
  const graph = new dagre.graphlib.Graph();
  const nodes: ContractFlowNode[] = [];
  const edges: Edge[] = [];
  const signalSource = new Map<string, { nodeId: string; handle: string }>();

  graph.setGraph({ rankdir: 'LR', nodesep: 46, ranksep: 78, marginx: 34, marginy: 42 });
  graph.setDefaultEdgeLabel(() => ({}));

  for (const graphNode of unit.nodes) {
    const atom = catalog.atoms.find(item => item.name === graphNode.atom);
    const category = atom?.category ?? 'unknown';
    const color = CATEGORY_COLORS[category] ?? '#64748b';
    const nodeId = `contract-${graphNode.id}`;

    graph.setNode(nodeId, { width: NODE_WIDTH, height: NODE_HEIGHT });
    nodes.push({
      id: nodeId,
      type: 'contractNode',
      position: { x: 0, y: 0 },
      data: { ...graphNode, category, color },
    });

    for (const [port, signal] of Object.entries(graphNode.out)) {
      signalSource.set(signal, { nodeId, handle: `out-${port}` });
    }
  }

  for (const graphNode of unit.nodes) {
    const target = `contract-${graphNode.id}`;
    for (const [port, signal] of Object.entries(graphNode.in)) {
      const source = signalSource.get(signal);
      if (!source) continue;

      graph.setEdge(source.nodeId, target);
      edges.push({
        id: `contract-edge-${source.nodeId}-${target}-${port}`,
        source: source.nodeId,
        sourceHandle: source.handle,
        target,
        targetHandle: `in-${port}`,
        label: signal,
        style: { stroke: '#94a3b8', strokeWidth: 1.5 },
        labelStyle: { fill: '#d6c6af', fontSize: 10, fontWeight: 600 },
        labelBgStyle: { fill: '#14110e', fillOpacity: 0.9 },
      });
    }
  }

  dagre.layout(graph);

  for (const node of nodes) {
    const position = graph.node(node.id);
    node.position = { x: position.x - NODE_WIDTH / 2, y: position.y - NODE_HEIGHT / 2 };
  }

  return { nodes, edges };
}

const ContractNode = memo(({ data, selected }: NodeProps<ContractFlowNode>) => {
  const inputPorts = Object.keys(data.in);
  const outputPorts = Object.keys(data.out);
  const style = { '--contract-node-color': data.color } as CSSProperties;

  return (
    <div className={`contract-node ${selected ? 'contract-node--selected' : ''}`} style={style}>
      <div className="contract-node__header">
        <span>{data.atom}</span>
        <strong>{data.category}</strong>
      </div>
      <div className="contract-node__body">
        <strong>{data.id}</strong>
        {Object.entries(data.config).map(([key, value]) => (
          <div key={key} className="contract-node__config">
            <span>{key}</span>
            <code>{value}</code>
          </div>
        ))}
      </div>

      {inputPorts.map((port, index) => (
        <Handle
          key={`in-${port}`}
          className="contract-node__handle contract-node__handle--in"
          id={`in-${port}`}
          position={Position.Left}
          style={{ top: `${((index + 1) * 100) / (inputPorts.length + 1)}%` }}
          type="target"
        />
      ))}
      {outputPorts.map((port, index) => (
        <Handle
          key={`out-${port}`}
          className="contract-node__handle contract-node__handle--out"
          id={`out-${port}`}
          position={Position.Right}
          style={{ top: `${((index + 1) * 100) / (outputPorts.length + 1)}%` }}
          type="source"
        />
      ))}
    </div>
  );
});

ContractNode.displayName = 'ContractNode';

const nodeTypes = { contractNode: ContractNode } satisfies NodeTypes;

export function ContractGraphCanvas({ workspaceFile, catalog, selectedUnitLabel, onBackToProject }: Props) {
  const parsed = useMemo<ParsedContractGraph>(() => {
    try {
      const unit = parseUnitGraph(workspaceFile.content);
      return { unit, error: null, flow: buildContractFlow(unit, catalog) };
    } catch (error) {
      return {
        unit: null,
        error: error instanceof Error ? error.message : 'Unable to parse unit YAML.',
        flow: { nodes: [], edges: [] },
      };
    }
  }, [catalog, workspaceFile.content]);

  return (
    <main className="canvas canvas--contract">
      <div className="canvas-modebar">
        <button className="btn btn--ghost" onClick={onBackToProject} type="button">
          Project graph
        </button>
        <div>
          <span>Contract graph</span>
          <strong>{parsed.unit?.name ?? selectedUnitLabel}</strong>
        </div>
        <code>{workspaceFile.path}</code>
      </div>

      {parsed.error ? (
        <div className="canvas__empty">
          <strong>Contract graph unavailable</strong>
          <p>{parsed.error}</p>
        </div>
      ) : (
        <div className="flow-shell flow-shell--contract">
          <ReactFlowProvider>
            <ReactFlow
              nodes={parsed.flow.nodes}
              edges={parsed.flow.edges}
              nodeTypes={nodeTypes}
              fitView
              fitViewOptions={{ padding: 0.18 }}
              minZoom={0.35}
              maxZoom={1.6}
              nodesDraggable
            >
              <Background color="#35312b" gap={24} />
              <Controls />
              <MiniMap nodeColor={node => (node.data as ContractNodeData).color} pannable zoomable style={{ background: '#171512' }} />
            </ReactFlow>
          </ReactFlowProvider>
        </div>
      )}
    </main>
  );
}
