import { memo, useEffect, useMemo, useRef, useState, type CSSProperties, type DragEvent } from 'react';
import {
  Controls,
  Handle,
  MiniMap,
  Position,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type Connection,
  type Node,
  type NodeProps,
  type NodeTypes,
  type ReactFlowInstance,
  useEdgesState,
  useNodesState,
} from '@xyflow/react';
import dagre from 'dagre';

import type { AtomCatalog, WorkspaceFile } from '../lib/backendSamples';
import { ATOM_DRAG_TYPE } from './AtomCatalogPanel';
import { parseUnitGraphDraft, type GraphPosition, type UnitConnectionEndpoint, type UnitGraphDraft } from '../lib/unitV2Graph';
import { markComponentRender, markPerfSpan } from '../lib/perfTelemetry';

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

type Props = {
  workspaceFile: WorkspaceFile;
  catalog: AtomCatalog;
  selectedUnitLabel: string;
  selectedAtomId: string | null;
  onBackToProject: () => void;
  onConnectAtoms: (source: UnitConnectionEndpoint, target: UnitConnectionEndpoint) => void;
  onDisconnectAtom: (target: UnitConnectionEndpoint) => void;
  onReconnectAtoms: (
    previousTarget: UnitConnectionEndpoint,
    source: UnitConnectionEndpoint,
    target: UnitConnectionEndpoint,
  ) => void;
  onSelectAtom: (id: string) => void;
  onOpenAtomInspector: (id: string) => void;
  onAddAtomAt: (atomName: string, position: GraphPosition) => void;
  onMoveAtom: (nodeId: string, position: GraphPosition) => void;
};

function endpoint(nodeId: string | null, handle: string | null, direction: 'in' | 'out'): UnitConnectionEndpoint | null {
  if (!nodeId || !handle?.startsWith(`${direction}-`)) return null;
  return { nodeId: nodeId.replace(/^contract-/, ''), field: handle.slice(direction.length + 1) };
}

type ParsedContractGraph = {
  unit: UnitGraphDraft | null;
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

function sameStringRecord(left: Record<string, string>, right: Record<string, string>): boolean {
  const leftEntries = Object.entries(left);
  return leftEntries.length === Object.keys(right).length
    && leftEntries.every(([key, value]) => right[key] === value);
}

function sameContractNodeData(left: ContractNodeData, right: ContractNodeData): boolean {
  return left.id === right.id
    && left.atom === right.atom
    && left.category === right.category
    && left.color === right.color
    && sameStringRecord(left.in, right.in)
    && sameStringRecord(left.out, right.out)
    && sameStringRecord(left.config, right.config);
}

function sameEdge(left: Edge, right: Edge): boolean {
  return left.id === right.id
    && left.source === right.source
    && left.sourceHandle === right.sourceHandle
    && left.target === right.target
    && left.targetHandle === right.targetHandle
    && left.label === right.label;
}

function buildContractFlow(
  unit: UnitGraphDraft,
  catalog: AtomCatalog,
): { nodes: ContractFlowNode[]; edges: Edge[] } {
  const needsLayout = unit.nodes.some(node => !node.ui?.position);
  const graph = needsLayout ? new dagre.graphlib.Graph() : null;
  const nodes: ContractFlowNode[] = [];
  const edges: Edge[] = [];
  const signalSource = new Map<string, { nodeId: string; handle: string }>();
  const catalogByName = new Map(catalog.atoms.map(atom => [atom.name, atom]));

  graph?.setGraph({ rankdir: 'LR', nodesep: 46, ranksep: 78, marginx: 34, marginy: 42 });
  graph?.setDefaultEdgeLabel(() => ({}));

  for (const graphNode of unit.nodes) {
    const atom = catalogByName.get(graphNode.atom);
    const category = atom?.category ?? 'unknown';
    const color = CATEGORY_COLORS[category] ?? '#64748b';
    const nodeId = `contract-${graphNode.id}`;

    graph?.setNode(nodeId, { width: NODE_WIDTH, height: NODE_HEIGHT });
    nodes.push({
      id: nodeId,
      type: 'contractNode',
      position: graphNode.ui?.position ?? { x: 0, y: 0 },
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

      graph?.setEdge(source.nodeId, target);
      edges.push({
        id: `contract-edge-${source.nodeId}-${target}-${port}`,
        source: source.nodeId,
        sourceHandle: source.handle,
        target,
        targetHandle: `in-${port}`,
        label: signal,
        style: { stroke: 'var(--text-muted)', strokeWidth: 1.5 },
        labelStyle: { fill: '#e2e8f0', fontSize: 10, fontWeight: 600 },
        labelBgStyle: { fill: '#111827', fillOpacity: 0.9 },
      });
    }
  }

  if (graph) {
    dagre.layout(graph);
    for (let index = 0; index < nodes.length; index += 1) {
      if (unit.nodes[index].ui?.position) continue;
      const position = graph.node(nodes[index].id);
      nodes[index].position = { x: position.x - NODE_WIDTH / 2, y: position.y - NODE_HEIGHT / 2 };
    }
  }

  return { nodes, edges };
}

const ContractNode = memo(({ data, selected }: NodeProps<ContractFlowNode>) => {
  useEffect(() => markComponentRender('ContractNode', data.id));
  const inputPorts = Object.keys(data.in);
  const outputPorts = Object.keys(data.out);
  const style = { '--contract-node-color': data.color } as CSSProperties;

  return (
    <div
      className={`contract-node ${selected ? 'contract-node--selected' : ''}`}
      data-testid={`contract-node-${data.id}`}
      style={style}
    >
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

export function ContractGraphCanvas({
  workspaceFile,
  catalog,
  selectedUnitLabel,
  selectedAtomId,
  onBackToProject,
  onConnectAtoms,
  onDisconnectAtom,
  onSelectAtom,
  onOpenAtomInspector,
  onReconnectAtoms,
  onAddAtomAt,
  onMoveAtom,
}: Props) {
  const parsed = useMemo<ParsedContractGraph>(() => {
    try {
      const unit = parseUnitGraphDraft(workspaceFile.content);
      return { unit, error: null, flow: buildContractFlow(unit, catalog) };
    } catch (error) {
      return {
        unit: null,
        error: error instanceof Error ? error.message : 'Unable to parse unit YAML.',
        flow: { nodes: [], edges: [] },
      };
    }
  }, [catalog, workspaceFile.content]);
  const [flowNodes, setFlowNodes, onNodesChange] = useNodesState<ContractFlowNode>(parsed.flow.nodes);
  const [flowEdges, setFlowEdges, onEdgesChange] = useEdgesState(parsed.flow.edges);
  const [dropState, setDropState] = useState<'idle' | 'valid' | 'reject'>('idle');
  const reactFlowRef = useRef<ReactFlowInstance<ContractFlowNode, Edge> | null>(null);
  const dragStartAtByNode = useRef<Record<string, number>>({});

  useEffect(() => {
    setFlowNodes(current => {
      if (current === parsed.flow.nodes) return current;
      const currentById = new Map(current.map(node => [node.id, node]));
      const storedPositionIds = new Set(
        parsed.unit?.nodes.filter(node => node.ui?.position).map(node => `contract-${node.id}`) ?? [],
      );
      const next = parsed.flow.nodes.map(node => {
        const positioned = currentById.get(node.id);
        if (!positioned) return node;
        const position = storedPositionIds.has(node.id) ? node.position : positioned.position;
        if (sameContractNodeData(positioned.data as ContractNodeData, node.data as ContractNodeData)
          && positioned.position.x === position.x
          && positioned.position.y === position.y) {
          return positioned;
        }
        return { ...positioned, data: node.data, position };
      });
      return next.length === current.length && next.every((node, index) => node === current[index]) ? current : next;
    });
  }, [parsed.flow.nodes, parsed.unit?.nodes, setFlowNodes]);

  useEffect(() => {
    setFlowEdges(current => {
      if (current === parsed.flow.edges) return current;
      const currentById = new Map(current.map(edge => [edge.id, edge]));
      const next = parsed.flow.edges.map(edge => {
        const existing = currentById.get(edge.id);
        return existing && sameEdge(existing, edge) ? existing : edge;
      });
      return next.length === current.length && next.every((edge, index) => edge === current[index]) ? current : next;
    });
  }, [parsed.flow.edges, setFlowEdges]);

  useEffect(() => {
    setFlowNodes(current => {
      let changed = false;
      const next = current.map(node => {
        const selected = (node.data as ContractNodeData).id === selectedAtomId;
        if (Boolean(node.selected) === selected) return node;
        changed = true;
        return { ...node, selected };
      });
      return changed ? next : current;
    });
  }, [selectedAtomId, setFlowNodes]);

  const connect = (connection: Connection) => {
    const source = endpoint(connection.source, connection.sourceHandle, 'out');
    const target = endpoint(connection.target, connection.targetHandle, 'in');
    if (source && target) onConnectAtoms(source, target);
  };

  const reconnect = (edge: Edge, connection: Connection) => {
    const previousTarget = endpoint(edge.target, edge.targetHandle ?? null, 'in');
    const source = endpoint(connection.source, connection.sourceHandle, 'out');
    const target = endpoint(connection.target, connection.targetHandle, 'in');
    if (previousTarget && source && target) onReconnectAtoms(previousTarget, source, target);
  };

  const deleteEdges = (edges: Edge[]) => {
    for (const edge of edges) {
      const target = endpoint(edge.target, edge.targetHandle ?? null, 'in');
      if (target) onDisconnectAtom(target);
    }
  };

  const dragState = (event: DragEvent) => event.dataTransfer.types.includes(ATOM_DRAG_TYPE) ? 'valid' : 'reject';
  const dragOver = (event: DragEvent) => {
    markPerfSpan('ui.dragOver.contractAtom', () => {
      const state = dragState(event);
      event.preventDefault();
      event.dataTransfer.dropEffect = state === 'valid' ? 'copy' : 'none';
      setDropState(state);
    });
  };
  const drop = (event: DragEvent) => {
    markPerfSpan('ui.drop.contractAtom', () => {
      event.preventDefault();
      const atomName = event.dataTransfer.getData(ATOM_DRAG_TYPE);
      setDropState('idle');
      if (!atomName || !reactFlowRef.current) return;
      onAddAtomAt(atomName, reactFlowRef.current.screenToFlowPosition({ x: event.clientX, y: event.clientY }));
    }, { atomType: event.dataTransfer.getData(ATOM_DRAG_TYPE) || 'none' });
  };

  return (
    <main className="canvas canvas--contract canvas-area">
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
        <div
          className={`flow-shell flow-shell--contract flow-shell--drop-${dropState}`}
          data-atom-count={flowNodes.length}
          data-testid="contract-canvas"
          onDragLeave={() => setDropState('idle')}
          onDragOver={dragOver}
          onDrop={drop}
        >
          <div className="edit-plane-grid" aria-hidden="true" />
          <ReactFlowProvider>
            <ReactFlow
              nodes={flowNodes}
              edges={flowEdges}
              nodeTypes={nodeTypes}
              onConnect={connect}
              onEdgesDelete={deleteEdges}
              onEdgesChange={onEdgesChange}
              onNodeClick={(_, node) => onSelectAtom((node.data as ContractNodeData).id)}
              onNodeDoubleClick={(_, node) => onOpenAtomInspector((node.data as ContractNodeData).id)}
              onNodeDragStart={(_, node) => {
                dragStartAtByNode.current[node.id] = performance.now();
                markPerfSpan('ui.drag.contractAtom.start', () => undefined, { nodeId: node.id });
              }}
              onNodeDrag={(_, node) => {
                markPerfSpan('ui.drag.contractAtom', () => undefined, { nodeId: node.id });
              }}
              onNodeDragStop={(_, node) => {
                const startedAt = dragStartAtByNode.current[node.id];
                delete dragStartAtByNode.current[node.id];
                const atomId = (node.data as ContractNodeData).id;
                markPerfSpan('ui.drag.contractAtom.stop', () => {
                  onMoveAtom(atomId, node.position);
                }, startedAt ? { nodeId: node.id, durationMs: performance.now() - startedAt } : { nodeId: node.id });
              }}
              onNodesChange={onNodesChange}
              onInit={instance => {
                reactFlowRef.current = instance;
              }}
              onReconnect={reconnect}
              deleteKeyCode={['Backspace', 'Delete']}
              edgesReconnectable
              fitView
              fitViewOptions={{ padding: 0.18 }}
              minZoom={flowNodes.length >= 500 ? 0.8 : flowNodes.length >= 100 ? 0.6 : 0.35}
              maxZoom={1.6}
              nodesDraggable
              onlyRenderVisibleElements
            >
              <Controls />
              {flowNodes.length <= 50 ? (
                <MiniMap nodeColor={node => (node.data as ContractNodeData).color} pannable zoomable style={{ background: '#111827' }} />
              ) : null}
            </ReactFlow>
          </ReactFlowProvider>
        </div>
      )}
    </main>
  );
}
