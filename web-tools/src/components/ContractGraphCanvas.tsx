import { memo, useEffect, useMemo, useRef, useState, type CSSProperties, type DragEvent } from 'react';
import {
  BezierEdge,
  Controls,
  Handle,
  MiniMap,
  Position,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type EdgeProps,
  type EdgeTypes,
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
import {
  parseUnitGraphDraft,
  parseUnitPortsDraft,
  type GraphPosition,
  type UnitConnectionEndpoint,
  type UnitGraphDraft,
  type UnitPortDraft,
  type UnitPortsDraft,
} from '../lib/unitV2Graph';
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

type UnitBoundaryNodeData = {
  direction: 'input' | 'output';
  portNames: string[];
  signalNames: string[];
  color: string;
};

type ContractAtomFlowNode = Node<ContractNodeData, 'contractNode'>;
type UnitBoundaryFlowNode = Node<UnitBoundaryNodeData, 'unitBoundaryNode'>;
type ContractFlowNode = ContractAtomFlowNode | UnitBoundaryFlowNode;
type ContractFlowEdge = Edge<Record<string, never>, 'contractEdge'>;

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
  onInsertAtomAtEdge: (atomName: string, target: UnitConnectionEndpoint, position: GraphPosition) => void;
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
const BOUNDARY_NODE_SIZE = 88;
const BOUNDARY_NODE_GAP = 96;
const INPUT_BOUNDARY_ID = 'contract-unit-input';
const OUTPUT_BOUNDARY_ID = 'contract-unit-output';
const INPUT_BOUNDARY_COLOR = '#38bdf8';
const OUTPUT_BOUNDARY_COLOR = '#f59e0b';

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

function sameStringArray(left: string[], right: string[]): boolean {
  return left.length === right.length && left.every((value, index) => value === right[index]);
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

function sameBoundaryNodeData(left: UnitBoundaryNodeData, right: UnitBoundaryNodeData): boolean {
  return left.direction === right.direction
    && left.color === right.color
    && sameStringArray(left.portNames, right.portNames)
    && sameStringArray(left.signalNames, right.signalNames);
}

function sameFlowNodeData(left: ContractFlowNode, right: ContractFlowNode): boolean {
  if (left.type !== right.type) return false;
  if (left.type === 'contractNode' && right.type === 'contractNode') {
    return sameContractNodeData(left.data, right.data);
  }
  if (left.type === 'unitBoundaryNode' && right.type === 'unitBoundaryNode') {
    return sameBoundaryNodeData(left.data, right.data);
  }
  return false;
}

function mergeFlowNode(
  current: ContractFlowNode,
  next: ContractFlowNode,
  position: ContractFlowNode['position'],
): ContractFlowNode {
  if (current.type === 'contractNode' && next.type === 'contractNode') {
    return { ...current, data: next.data, position };
  }
  if (current.type === 'unitBoundaryNode' && next.type === 'unitBoundaryNode') {
    return { ...current, data: next.data, position };
  }
  return next;
}

function sameEdge(left: Edge, right: Edge): boolean {
  return left.id === right.id
    && left.type === right.type
    && left.source === right.source
    && left.sourceHandle === right.sourceHandle
    && left.target === right.target
    && left.targetHandle === right.targetHandle
    && left.label === right.label;
}

function portSignals(port: UnitPortDraft): string[] {
  if (port.type !== 'audio') return [];
  return port.signals.length > 0 ? port.signals : [port.name];
}

function signalPorts(ports: UnitPortDraft[]): UnitPortDraft[] {
  return ports.filter(port => portSignals(port).length > 0);
}

function buildContractFlow(
  unit: UnitGraphDraft,
  ports: UnitPortsDraft,
  catalog: AtomCatalog,
): { nodes: ContractFlowNode[]; edges: Edge[] } {
  const needsLayout = unit.nodes.some(node => !node.ui?.position);
  const graph = needsLayout ? new dagre.graphlib.Graph() : null;
  const nodes: ContractFlowNode[] = [];
  const edges: Edge[] = [];
  const signalSource = new Map<string, { nodeId: string; handle: string }>();
  const catalogByName = new Map(catalog.atoms.map(atom => [atom.name, atom]));
  const inputPorts = signalPorts(ports.inputs);
  const outputPorts = signalPorts(ports.outputs);
  const inputSignalNames = [...new Set(inputPorts.flatMap(portSignals))];
  const outputSignalNames = [...new Set(outputPorts.flatMap(portSignals))];
  const inputBoundary: UnitBoundaryFlowNode = {
    id: INPUT_BOUNDARY_ID,
    type: 'unitBoundaryNode',
    position: { x: 0, y: 0 },
    data: {
      direction: 'input',
      portNames: inputPorts.map(port => port.name),
      signalNames: inputSignalNames,
      color: INPUT_BOUNDARY_COLOR,
    },
    connectable: false,
    deletable: false,
    draggable: false,
    selectable: false,
  };
  const outputBoundary: UnitBoundaryFlowNode = {
    id: OUTPUT_BOUNDARY_ID,
    type: 'unitBoundaryNode',
    position: { x: 0, y: 0 },
    data: {
      direction: 'output',
      portNames: outputPorts.map(port => port.name),
      signalNames: outputSignalNames,
      color: OUTPUT_BOUNDARY_COLOR,
    },
    connectable: false,
    deletable: false,
    draggable: false,
    selectable: false,
  };

  graph?.setGraph({ rankdir: 'LR', nodesep: 46, ranksep: 78, marginx: 34, marginy: 42 });
  graph?.setDefaultEdgeLabel(() => ({}));
  nodes.push(inputBoundary);
  for (const signal of inputSignalNames) {
    signalSource.set(signal, { nodeId: INPUT_BOUNDARY_ID, handle: 'boundary-out' });
  }

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
      const fromBoundary = source.nodeId === INPUT_BOUNDARY_ID;

      if (!fromBoundary) graph?.setEdge(source.nodeId, target);
      edges.push({
        id: fromBoundary
          ? `contract-boundary-input-${signal}-${target}-${port}`
          : `contract-edge-${source.nodeId}-${target}-${port}`,
        type: 'contractEdge',
        source: source.nodeId,
        sourceHandle: source.handle,
        target,
        targetHandle: `in-${port}`,
        label: signal,
        className: fromBoundary ? 'contract-edge--boundary contract-edge--boundary-input' : undefined,
        deletable: fromBoundary ? false : undefined,
        reconnectable: fromBoundary ? false : undefined,
        selectable: fromBoundary ? false : undefined,
        style: { stroke: fromBoundary ? INPUT_BOUNDARY_COLOR : 'var(--text-muted)', strokeWidth: 1.5 },
        labelStyle: { fill: '#e2e8f0', fontSize: 10, fontWeight: 600 },
        labelBgStyle: { fill: '#111827', fillOpacity: 0.9 },
      });
    }
  }

  if (graph) {
    dagre.layout(graph);
    const storedPositionIds = new Set(unit.nodes.filter(node => node.ui?.position).map(node => `contract-${node.id}`));
    for (const node of nodes) {
      if (node.type !== 'contractNode' || storedPositionIds.has(node.id)) continue;
      const position = graph.node(node.id);
      node.position = { x: position.x - NODE_WIDTH / 2, y: position.y - NODE_HEIGHT / 2 };
    }
  }

  nodes.push(outputBoundary);
  for (const signal of outputSignalNames) {
    const source = signalSource.get(signal);
    if (!source) continue;
    edges.push({
      id: `contract-boundary-output-${signal}-${source.nodeId}`,
      type: 'contractEdge',
      source: source.nodeId,
      sourceHandle: source.handle,
      target: OUTPUT_BOUNDARY_ID,
      targetHandle: 'boundary-in',
      label: signal,
      className: 'contract-edge--boundary contract-edge--boundary-output',
      deletable: false,
      reconnectable: false,
      selectable: false,
      style: { stroke: OUTPUT_BOUNDARY_COLOR, strokeWidth: 1.5 },
      labelStyle: { fill: '#e2e8f0', fontSize: 10, fontWeight: 600 },
      labelBgStyle: { fill: '#111827', fillOpacity: 0.9 },
    });
  }

  const atomNodes = nodes.filter((node): node is ContractAtomFlowNode => node.type === 'contractNode');
  if (atomNodes.length > 0) {
    const minX = Math.min(...atomNodes.map(node => node.position.x));
    const maxX = Math.max(...atomNodes.map(node => node.position.x + NODE_WIDTH));
    const minY = Math.min(...atomNodes.map(node => node.position.y));
    const maxY = Math.max(...atomNodes.map(node => node.position.y + NODE_HEIGHT));
    const boundaryY = (minY + maxY - BOUNDARY_NODE_SIZE) / 2;
    inputBoundary.position = { x: minX - BOUNDARY_NODE_GAP - BOUNDARY_NODE_SIZE, y: boundaryY };
    outputBoundary.position = { x: maxX + BOUNDARY_NODE_GAP, y: boundaryY };
  } else {
    inputBoundary.position = { x: 0, y: 0 };
    outputBoundary.position = { x: BOUNDARY_NODE_SIZE + BOUNDARY_NODE_GAP * 2, y: 0 };
  }

  return { nodes, edges };
}

const ContractNode = memo(({ data, selected }: NodeProps<ContractAtomFlowNode>) => {
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

const UnitBoundaryNode = memo(({ data }: NodeProps<UnitBoundaryFlowNode>) => {
  useEffect(() => markComponentRender('UnitBoundaryNode', data.direction));
  const isInput = data.direction === 'input';
  const portSummary = data.portNames.length === 0
    ? 'No audio port'
    : data.portNames.length === 1
      ? data.portNames[0]
      : `${data.portNames.length} ports`;
  const signalSummary = data.signalNames.join(', ') || 'No graph signal';
  const style = { '--boundary-color': data.color } as CSSProperties;

  return (
    <div
      aria-label={`Unit ${data.direction} boundary, ${portSummary}: ${signalSummary}`}
      className={`unit-boundary-node unit-boundary-node--${data.direction}`}
      data-testid={`unit-boundary-${data.direction}`}
      role="img"
      style={style}
      title={`${isInput ? 'From previous stage' : 'To next stage'} · ${signalSummary}`}
    >
      <span className="unit-boundary-node__context">{isInput ? 'Previous' : 'Next'}</span>
      <strong>{isInput ? 'IN' : 'OUT'}</strong>
      <span className="unit-boundary-node__ports">{portSummary}</span>
      <Handle
        className="unit-boundary-node__handle"
        id={isInput ? 'boundary-out' : 'boundary-in'}
        isConnectable={false}
        position={isInput ? Position.Right : Position.Left}
        type={isInput ? 'source' : 'target'}
      />
    </div>
  );
});

UnitBoundaryNode.displayName = 'UnitBoundaryNode';

const nodeTypes = {
  contractNode: ContractNode,
  unitBoundaryNode: UnitBoundaryNode,
} satisfies NodeTypes;

const ContractEdge = memo((props: EdgeProps<ContractFlowEdge>) => {
  useEffect(() => markComponentRender('ContractEdge', props.id));
  return <BezierEdge {...props} />;
});

ContractEdge.displayName = 'ContractEdge';

const edgeTypes = { contractEdge: ContractEdge } satisfies EdgeTypes;

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
  onInsertAtomAtEdge,
  onMoveAtom,
}: Props) {
  const parsed = useMemo<ParsedContractGraph>(() => {
    try {
      const unit = parseUnitGraphDraft(workspaceFile.content);
      const ports = parseUnitPortsDraft(workspaceFile.content);
      return { unit, error: null, flow: buildContractFlow(unit, ports, catalog) };
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
  const atomCount = parsed.unit?.nodes.length ?? 0;

  useEffect(() => {
    if (dropState === 'idle') return;
    const cancelDrop = (event: KeyboardEvent) => {
      if (event.key === 'Escape') setDropState('idle');
    };
    window.addEventListener('keydown', cancelDrop);
    return () => window.removeEventListener('keydown', cancelDrop);
  }, [dropState]);

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
        const position = node.type === 'unitBoundaryNode' || storedPositionIds.has(node.id)
          ? node.position
          : positioned.position;
        if (sameFlowNodeData(positioned, node)
          && positioned.position.x === position.x
          && positioned.position.y === position.y) {
          return positioned;
        }
        return mergeFlowNode(positioned, node, position);
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
        const selected = node.type === 'contractNode' && node.data.id === selectedAtomId;
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
      const position = reactFlowRef.current.screenToFlowPosition({ x: event.clientX, y: event.clientY });
      const edgeElement = event.target instanceof Element ? event.target.closest<SVGGElement>('.react-flow__edge') : null;
      const edge = edgeElement ? flowEdges.find(item => item.id === edgeElement.dataset.id) : null;
      const target = edge && !edge.id.startsWith('contract-boundary-')
        ? endpoint(edge.target, edge.targetHandle ?? null, 'in')
        : null;
      if (target) onInsertAtomAtEdge(atomName, target, position);
      else onAddAtomAt(atomName, position);
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
          data-atom-count={atomCount}
          data-boundary-count="2"
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
              edgeTypes={edgeTypes}
              nodeTypes={nodeTypes}
              onConnect={connect}
              onEdgesDelete={deleteEdges}
              onEdgesChange={onEdgesChange}
              onNodeClick={(_, node) => {
                if (node.type === 'contractNode') onSelectAtom(node.data.id);
              }}
              onNodeDoubleClick={(_, node) => {
                if (node.type === 'contractNode') onOpenAtomInspector(node.data.id);
              }}
              onNodeDragStart={(_, node) => {
                if (node.type !== 'contractNode') return;
                dragStartAtByNode.current[node.id] = performance.now();
                markPerfSpan('ui.drag.contractAtom.start', () => undefined, { nodeId: node.id });
              }}
              onNodeDrag={(_, node) => {
                if (node.type !== 'contractNode') return;
                markPerfSpan('ui.drag.contractAtom', () => undefined, { nodeId: node.id });
              }}
              onNodeDragStop={(_, node) => {
                if (node.type !== 'contractNode') return;
                const startedAt = dragStartAtByNode.current[node.id];
                delete dragStartAtByNode.current[node.id];
                markPerfSpan('ui.drag.contractAtom.stop', () => {
                  onMoveAtom(node.data.id, node.position);
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
              minZoom={atomCount >= 500 ? 0.8 : atomCount >= 100 ? 0.6 : 0.35}
              maxZoom={1.6}
              nodesDraggable
              onlyRenderVisibleElements
            >
              <Controls />
              {atomCount <= 50 ? (
                <MiniMap
                  nodeColor={node => (node.data as ContractNodeData | UnitBoundaryNodeData).color}
                  pannable
                  zoomable
                  style={{ background: '#111827' }}
                />
              ) : null}
            </ReactFlow>
          </ReactFlowProvider>
        </div>
      )}
    </main>
  );
}
