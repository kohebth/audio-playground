import {
  memo,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type CSSProperties,
  type DragEvent,
  type KeyboardEvent as ReactKeyboardEvent,
} from 'react';
import {
  BaseEdge,
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

import type { AtomCatalog, WorkspaceFile } from '../lib/backendSamples';
import { ATOM_DRAG_TYPE } from './AtomCatalogPanel';
import { GraphContextMenu, GraphMenuButton, type ContextMenuPoint } from './GraphContextMenu';
import {
  parseUnitGraphDraft,
  parseUnitPortsDraft,
  previewAtomReplacement,
  type GraphPosition,
  type UnitConnectionEndpoint,
  type UnitGraphDraft,
  type UnitPortDraft,
  type UnitPortsDraft,
} from '../lib/unitV2Graph';
import type { GraphvizLayoutRequest, GraphvizLayoutResult } from '../lib/graphvizLayout';
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
  signalNames: string[];
  color: string;
};

type ContractAtomFlowNode = Node<ContractNodeData, 'contractNode'>;
type UnitBoundaryFlowNode = Node<UnitBoundaryNodeData, 'unitBoundaryNode'>;
type ContractFlowNode = ContractAtomFlowNode | UnitBoundaryFlowNode;
type ContractFlowEdgeData = { points?: Array<{ x: number; y: number }> };
type ContractFlowEdge = Edge<ContractFlowEdgeData, 'contractEdge'>;

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
  onAutoLayout: (positions: Record<string, GraphPosition>) => void;
  atomClipboardReady: boolean;
  onCopyAtom: (nodeId: string) => void;
  onCutAtom: (nodeId: string) => void;
  onPasteAtom: () => void;
  onRemoveAtom: (nodeId: string) => void;
  onReplaceAtom: (nodeId: string, nextAtomName: string, preserveId: boolean) => void;
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
    edges: ContractFlowEdge[];
  };
};

const NODE_WIDTH = 220;
const NODE_HEIGHT = 136;
const BOUNDARY_NODE_SIZE = 10;
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
): { nodes: ContractFlowNode[]; edges: ContractFlowEdge[] } {
  const nodes: ContractFlowNode[] = [];
  const edges: ContractFlowEdge[] = [];
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
      signalNames: outputSignalNames,
      color: OUTPUT_BOUNDARY_COLOR,
    },
    connectable: false,
    deletable: false,
    draggable: false,
    selectable: false,
  };

  nodes.push(inputBoundary);
  for (const signal of inputSignalNames) {
    signalSource.set(signal, { nodeId: INPUT_BOUNDARY_ID, handle: 'boundary-out' });
  }

  unit.nodes.forEach((graphNode, index) => {
    const atom = catalogByName.get(graphNode.atom);
    const category = atom?.category ?? 'unknown';
    const color = CATEGORY_COLORS[category] ?? '#64748b';
    const nodeId = `contract-${graphNode.id}`;

    nodes.push({
      id: nodeId,
      type: 'contractNode',
      position: graphNode.ui?.position ?? { x: index * (NODE_WIDTH + 78), y: 0 },
      data: { ...graphNode, category, color },
    });

    for (const [port, signal] of Object.entries(graphNode.out)) {
      signalSource.set(signal, { nodeId, handle: `out-${port}` });
    }
  });

  for (const graphNode of unit.nodes) {
    const target = `contract-${graphNode.id}`;
    for (const [port, signal] of Object.entries(graphNode.in)) {
      const source = signalSource.get(signal);
      if (!source) continue;
      const fromBoundary = source.nodeId === INPUT_BOUNDARY_ID;

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
  const signalSummary = data.signalNames.join(', ') || 'unbound';
  const style = { '--boundary-color': data.color } as CSSProperties;

  return (
    <div
      aria-label={`Unit ${data.direction} signal: ${signalSummary}`}
      className={`unit-boundary-node unit-boundary-node--${data.direction}`}
      data-testid={`unit-boundary-${data.direction}`}
      role="img"
      style={style}
      title={`${isInput ? 'From previous stage' : 'To next stage'}: ${signalSummary}`}
    >
      <span className="unit-boundary-node__signal">{signalSummary}</span>
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

type RoutePoint = { x: number; y: number };

function pointToward(origin: RoutePoint, target: RoutePoint, distance: number): RoutePoint {
  const length = Math.hypot(target.x - origin.x, target.y - origin.y);
  if (length === 0) return origin;
  const ratio = distance / length;
  return {
    x: origin.x + (target.x - origin.x) * ratio,
    y: origin.y + (target.y - origin.y) * ratio,
  };
}

function coordinate(value: number): number {
  return Number(value.toFixed(2));
}

function roundedPath(points: RoutePoint[], radius = 8): string {
  if (points.length === 0) return '';
  const path = [`M ${coordinate(points[0].x)} ${coordinate(points[0].y)}`];

  for (let index = 1; index < points.length - 1; index += 1) {
    const previous = points[index - 1];
    const corner = points[index];
    const next = points[index + 1];
    const cornerRadius = Math.min(
      radius,
      Math.hypot(corner.x - previous.x, corner.y - previous.y) / 2,
      Math.hypot(next.x - corner.x, next.y - corner.y) / 2,
    );
    const entry = pointToward(corner, previous, cornerRadius);
    const exit = pointToward(corner, next, cornerRadius);
    path.push(`L ${coordinate(entry.x)} ${coordinate(entry.y)}`);
    if (cornerRadius > 0) {
      path.push(`Q ${coordinate(corner.x)} ${coordinate(corner.y)} ${coordinate(exit.x)} ${coordinate(exit.y)}`);
    }
  }

  const last = points.at(-1)!;
  path.push(`L ${coordinate(last.x)} ${coordinate(last.y)}`);
  return path.join(' ');
}

function appendRoutePoint(points: RoutePoint[], point: RoutePoint): void {
  const previous = points.at(-1);
  if (previous?.x === point.x && previous.y === point.y) return;
  points.push(point);
}

function removeCollinearPoints(points: RoutePoint[]): RoutePoint[] {
  return points.filter((point, index) => {
    if (index === 0 || index === points.length - 1) return true;
    const previous = points[index - 1];
    const next = points[index + 1];
    return !((previous.x === point.x && point.x === next.x)
      || (previous.y === point.y && point.y === next.y));
  });
}

function edgePoints(planned: RoutePoint[] | undefined, source: RoutePoint, target: RoutePoint): RoutePoint[] {
  const graphvizPoints = (planned ?? []).filter((point, index, points) => (
    index === 0 || point.x !== points[index - 1].x || point.y !== points[index - 1].y
  ));
  const hints = graphvizPoints.slice(1, -1);
  if (hints.length === 0) {
    const midpointX = source.x + (target.x - source.x) / 2;
    return [source, { x: midpointX, y: source.y }, { x: midpointX, y: target.y }, target];
  }

  const points: RoutePoint[] = [source];
  for (const hint of hints) {
    const previous = points.at(-1)!;
    if (previous.x !== hint.x && previous.y !== hint.y) {
      appendRoutePoint(points, { x: hint.x, y: previous.y });
    }
    appendRoutePoint(points, hint);
  }

  const previous = points.at(-1)!;
  if (previous.x !== target.x && previous.y !== target.y) {
    appendRoutePoint(points, { x: previous.x, y: target.y });
  }
  appendRoutePoint(points, target);
  return removeCollinearPoints(points);
}

const ContractEdge = memo((props: EdgeProps<ContractFlowEdge>) => {
  useEffect(() => markComponentRender('ContractEdge', props.id));
  const points = edgePoints(
    props.data?.points,
    { x: props.sourceX, y: props.sourceY },
    { x: props.targetX, y: props.targetY },
  );
  return <BaseEdge {...props} path={roundedPath(points)} />;
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
  onAutoLayout,
  atomClipboardReady,
  onCopyAtom,
  onCutAtom,
  onPasteAtom,
  onRemoveAtom,
  onReplaceAtom,
}: Props) {
  const parsed = useMemo<ParsedContractGraph>(() => {
    try {
      const unit = parseUnitGraphDraft(workspaceFile.content);
      const ports = parseUnitPortsDraft(workspaceFile.content);
      return { unit, error: null, flow: buildContractFlow(unit, ports, catalog) };
    } catch (error) {
      return {
        unit: null,
        error: error instanceof Error ? error.message : 'Unable to read the unit source.',
        flow: { nodes: [], edges: [] },
      };
    }
  }, [catalog, workspaceFile.content]);
  const [flowNodes, setFlowNodes, onNodesChange] = useNodesState<ContractFlowNode>(parsed.flow.nodes);
  const [flowEdges, setFlowEdges, onEdgesChange] = useEdgesState<ContractFlowEdge>(parsed.flow.edges);
  const [dropState, setDropState] = useState<'idle' | 'valid' | 'reject'>('idle');
  const [connectionArmed, setConnectionArmed] = useState(false);
  const [contextMenu, setContextMenu] = useState<(ContextMenuPoint & { atomId: string }) | null>(null);
  const [replacementOpen, setReplacementOpen] = useState(false);
  const [replacementAtom, setReplacementAtom] = useState('');
  const reactFlowRef = useRef<ReactFlowInstance<ContractFlowNode, ContractFlowEdge> | null>(null);
  const dragStartAtByNode = useRef<Record<string, number>>({});
  const graphvizWorker = useRef<Worker | null>(null);
  const graphvizRequestId = useRef(0);
  const graphvizRequests = useRef(new Map<number, { mode: 'layout' | 'route'; persist: boolean }>());
  const autoLayoutTopology = useRef<string | null>(null);
  const [layoutBusy, setLayoutBusy] = useState(false);
  const [layoutError, setLayoutError] = useState<string | null>(null);
  const atomCount = parsed.unit?.nodes.length ?? 0;
  const contextAtom = contextMenu ? parsed.unit?.nodes.find(node => node.id === contextMenu.atomId) ?? null : null;
  const replacementOptions = contextAtom
    ? catalog.atoms.filter(atom => atom.visibility !== 'internal' && atom.name !== contextAtom.atom)
    : [];
  const chosenReplacement = replacementOptions.find(atom => atom.name === replacementAtom) ?? replacementOptions[0];
  const replacementPreview = contextAtom && chosenReplacement ? (() => {
    try {
      return previewAtomReplacement(workspaceFile.content, catalog, contextAtom.id, chosenReplacement.name);
    } catch {
      return null;
    }
  })() : null;
  const closeContextMenu = useCallback(() => {
    setContextMenu(null);
    setReplacementOpen(false);
  }, []);

  const applyGraphvizResult = useCallback((result: GraphvizLayoutResult) => {
    const request = graphvizRequests.current.get(result.requestId);
    graphvizRequests.current.delete(result.requestId);
    if (result.requestId !== graphvizRequestId.current) return;
    setLayoutBusy(false);
    if (result.error) {
      setLayoutError(result.error);
      return;
    }

    setLayoutError(null);
    if (request?.mode === 'layout') {
      setFlowNodes(current => current.map(node => (
        result.positions[node.id] ? { ...node, position: result.positions[node.id] } : node
      )));
      if (request.persist) {
        const atomPositions = Object.fromEntries(Object.entries(result.positions).flatMap(([id, position]) => (
          id.startsWith('contract-') && id !== INPUT_BOUNDARY_ID && id !== OUTPUT_BOUNDARY_ID
            ? [[id.replace(/^contract-/, ''), position]]
            : []
        )));
        onAutoLayout(atomPositions);
      }
      requestAnimationFrame(() => reactFlowRef.current?.fitView({ padding: 0.18 }));
    }
    setFlowEdges(current => current.map(edge => ({
      ...edge,
      data: result.routes[edge.id] ? { points: result.routes[edge.id] } : edge.data,
    })));
  }, [onAutoLayout, setFlowEdges, setFlowNodes]);

  const postGraphvizRequest = useCallback((
    mode: 'layout' | 'route',
    requestNodes: ContractFlowNode[],
    requestEdges: ContractFlowEdge[],
    persist = false,
  ) => {
    const worker = graphvizWorker.current;
    if (!worker || requestNodes.length === 0) return;
    const requestId = ++graphvizRequestId.current;
    graphvizRequests.current.set(requestId, { mode, persist });
    setLayoutBusy(true);
    setLayoutError(null);
    const request: GraphvizLayoutRequest = {
      requestId,
      mode,
      nodes: requestNodes.map(node => ({
        id: node.id,
        x: node.position.x,
        y: node.position.y,
        width: node.type === 'contractNode' ? NODE_WIDTH : BOUNDARY_NODE_SIZE,
        height: node.type === 'contractNode' ? NODE_HEIGHT : BOUNDARY_NODE_SIZE,
      })),
      edges: requestEdges.map(edge => ({ id: edge.id, source: edge.source, target: edge.target })),
    };
    worker.postMessage(request);
  }, []);

  useEffect(() => {
    const worker = new Worker(new URL('../workers/graphviz.worker.ts', import.meta.url), { type: 'module' });
    const requests = graphvizRequests.current;
    graphvizWorker.current = worker;
    const receive = (event: MessageEvent<GraphvizLayoutResult>) => applyGraphvizResult(event.data);
    worker.addEventListener('message', receive);
    return () => {
      worker.removeEventListener('message', receive);
      worker.terminate();
      graphvizWorker.current = null;
      requests.clear();
      autoLayoutTopology.current = null;
    };
  }, [applyGraphvizResult]);

  const topologySignature = useMemo(() => JSON.stringify({
    nodes: parsed.flow.nodes.map(node => node.id),
    edges: parsed.flow.edges.map(edge => [edge.id, edge.source, edge.target]),
  }), [parsed.flow.edges, parsed.flow.nodes]);

  useEffect(() => {
    if (parsed.error || autoLayoutTopology.current === topologySignature) return;
    autoLayoutTopology.current = topologySignature;
    postGraphvizRequest('layout', parsed.flow.nodes, parsed.flow.edges);
  }, [parsed.error, parsed.flow.edges, parsed.flow.nodes, postGraphvizRequest, topologySignature]);

  useEffect(() => {
    if (dropState === 'idle') return;
    const cancelDrop = (event: KeyboardEvent) => {
      if (event.key === 'Escape') setDropState('idle');
    };
    window.addEventListener('keydown', cancelDrop);
    return () => window.removeEventListener('keydown', cancelDrop);
  }, [dropState]);

  useEffect(() => {
    if (!connectionArmed) return;
    const cancelConnection = (event: KeyboardEvent) => {
      if (event.key === 'Escape') setConnectionArmed(false);
    };
    window.addEventListener('keydown', cancelConnection);
    return () => window.removeEventListener('keydown', cancelConnection);
  }, [connectionArmed]);

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
    setConnectionArmed(false);
    if (source && target) onConnectAtoms(source, target);
  };

  const openKeyboardMenu = (event: ReactKeyboardEvent<HTMLElement>) => {
    if (event.key !== 'ContextMenu' && !(event.shiftKey && event.key === 'F10')) return;
    const nodeElement = (event.target as Element).closest<HTMLElement>('.react-flow__node[data-id^="contract-"]');
    const flowNodeId = nodeElement?.dataset.id;
    if (!nodeElement || !flowNodeId || flowNodeId.startsWith('contract-unit-')) return;
    event.preventDefault();
    const atomId = flowNodeId.replace(/^contract-/, '');
    const bounds = nodeElement.getBoundingClientRect();
    onSelectAtom(atomId);
    setContextMenu({ atomId, x: bounds.left + 24, y: bounds.top + 24 });
    setReplacementOpen(false);
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
    <main className="canvas canvas--contract canvas-area" onKeyDownCapture={openKeyboardMenu}>
      <div className="canvas-modebar">
        <button className="btn btn--ghost" onClick={onBackToProject} type="button">
          Project graph
        </button>
        <div>
          <span>Atom graph</span>
          <strong>{parsed.unit?.name ?? selectedUnitLabel}</strong>
        </div>
        <code>{workspaceFile.path}</code>
        <button
          className="btn btn--ghost"
          data-testid="contract-auto-layout"
          disabled={layoutBusy}
          onClick={() => postGraphvizRequest('layout', flowNodes, flowEdges, true)}
          type="button"
        >
          {layoutBusy ? 'Arranging…' : 'Auto Layout'}
        </button>
      </div>
      {layoutError ? <p className="contract-layout-error" role="alert">Graphviz: {layoutError}</p> : null}

      {parsed.error ? (
        <div className="canvas__empty">
          <strong>Contract graph unavailable</strong>
          <p>{parsed.error}</p>
        </div>
      ) : (
        <div
          aria-busy={layoutBusy}
          className={`flow-shell flow-shell--contract flow-shell--drop-${dropState}${connectionArmed ? ' flow-shell--connecting' : ''}`}
          data-atom-count={atomCount}
          data-boundary-count="2"
          data-layout-engine="graphviz"
          data-layout-status={layoutError ? 'error' : layoutBusy ? 'busy' : 'ready'}
          data-routed-edge-count={flowEdges.filter(edge => edge.data?.points?.length).length}
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
              connectOnClick
              onConnectStart={() => setConnectionArmed(true)}
              onConnectEnd={() => setConnectionArmed(false)}
              onClickConnectStart={() => setConnectionArmed(true)}
              onClickConnectEnd={() => setConnectionArmed(false)}
              onEdgesDelete={deleteEdges}
              onEdgesChange={onEdgesChange}
              onNodeClick={(_, node) => {
                if (node.type === 'contractNode') onSelectAtom(node.data.id);
              }}
              onNodeContextMenu={(event, node) => {
                if (node.type !== 'contractNode') return;
                event.preventDefault();
                onSelectAtom(node.data.id);
                setContextMenu({ atomId: node.data.id, x: event.clientX, y: event.clientY });
                setReplacementOpen(false);
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
                  postGraphvizRequest('route', flowNodes.map(current => (
                    current.id === node.id ? { ...current, position: node.position } : current
                  )), flowEdges);
                }, startedAt ? { nodeId: node.id, durationMs: performance.now() - startedAt } : { nodeId: node.id });
              }}
              onNodesChange={onNodesChange}
              onPaneClick={() => setConnectionArmed(false)}
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
                  style={{ background: 'var(--bg-canvas)' }}
                />
              ) : null}
            </ReactFlow>
          </ReactFlowProvider>
        </div>
      )}
      {contextMenu && contextAtom ? (
        <GraphContextMenu label={`${contextAtom.id} actions`} onClose={closeContextMenu} point={contextMenu}>
          <div className="graph-context-menu__title">
            <strong>{contextAtom.id}</strong>
            <span>{contextAtom.atom}</span>
          </div>
          <GraphMenuButton
            disabled={replacementOptions.length === 0}
            icon="fa-repeat"
            onClick={() => {
              setReplacementOpen(open => !open);
              setReplacementAtom(chosenReplacement?.name ?? '');
            }}
          >Replace…</GraphMenuButton>
          {replacementOpen && chosenReplacement ? (
            <div className="graph-context-menu__replace" role="group" aria-label="Atom replacement preview">
              <label>
                <span>Replace with</span>
                <select
                  aria-label="Replacement atom"
                  onChange={event => setReplacementAtom(event.target.value)}
                  value={chosenReplacement.name}
                >
                  {replacementOptions.map(atom => (
                    <option key={atom.name} value={atom.name}>{atom.name}</option>
                  ))}
                </select>
              </label>
              {replacementPreview ? (
                <p>
                  Keeps {replacementPreview.preservedInputs.length + replacementPreview.preservedOutputs.length} bindings;
                  disconnects {replacementPreview.removedInputs.length + replacementPreview.removedOutputs.length};
                  adds {replacementPreview.addedInputs.length + replacementPreview.addedOutputs.length}.
                </p>
              ) : <p>Replacement preview is unavailable.</p>}
              <button
                className="graph-context-menu__confirm"
                disabled={!replacementPreview}
                onClick={() => {
                  onReplaceAtom(contextAtom.id, chosenReplacement.name, true);
                  closeContextMenu();
                }}
                type="button"
              >Confirm replace</button>
            </div>
          ) : null}
          <div className="graph-context-menu__separator" role="separator" />
          <GraphMenuButton icon="fa-scissors" onClick={() => { onCutAtom(contextAtom.id); closeContextMenu(); }}>Cut</GraphMenuButton>
          <GraphMenuButton icon="fa-copy" onClick={() => { onCopyAtom(contextAtom.id); closeContextMenu(); }}>Copy</GraphMenuButton>
          <GraphMenuButton disabled={!atomClipboardReady} icon="fa-paste" onClick={() => { onPasteAtom(); closeContextMenu(); }}>Paste</GraphMenuButton>
          <GraphMenuButton danger icon="fa-trash" onClick={() => { onRemoveAtom(contextAtom.id); closeContextMenu(); }}>Remove</GraphMenuButton>
        </GraphContextMenu>
      ) : null}
    </main>
  );
}
