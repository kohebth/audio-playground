import {
  Controls,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type Node,
  type OnEdgesChange,
  type OnNodesChange,
  type NodeTypes,
  useNodesInitialized,
  useReactFlow,
} from '@xyflow/react';
import { useEffect, useRef, useState, type DragEvent } from 'react';

import { ProjectNode } from './ProjectNode';
import { UNIT_DRAG_TYPE } from './ProjectSidebar';
import type { ProjectNodeData } from '../lib/projectGraph';
import type { GraphPosition } from '../lib/projectV2Graph';
import { markPerfSpan } from '../lib/perfTelemetry';

const nodeTypes = { projectNode: ProjectNode } satisfies NodeTypes;

type Props = {
  nodes: Node<ProjectNodeData>[];
  edges: Edge[];
  selectedRouteIndex: number | null;
  fitViewRevision: number;
  onNodesChange: OnNodesChange<Node<ProjectNodeData>>;
  onEdgesChange: OnEdgesChange;
  onSelectNode: (id: string) => void;
  onOpenContractGraph: (id: string) => void;
  onSelectRoute: (index: number) => void;
  onAddUnitAt: (unitId: string, position: GraphPosition) => void;
  onInsertUnitAtRoute: (unitId: string, routeIndex: number, position: GraphPosition) => void;
  onMoveUnit: (instanceId: string, position: GraphPosition) => void;
};

type ProjectFlowProps = Props & {
  displayedEdges: Edge[];
};

function routeIndexFromEdge(edge: Edge): number | null {
  const match = edge.id.match(/^route-(\d+)-/);
  return match ? Number(match[1]) : null;
}

function ProjectFlow({
  nodes,
  displayedEdges,
  onNodesChange,
  onEdgesChange,
  onSelectNode,
  onOpenContractGraph,
  onSelectRoute,
  onAddUnitAt,
  onInsertUnitAtRoute,
  onMoveUnit,
  fitViewRevision,
}: ProjectFlowProps) {
  const reactFlow = useReactFlow();
  const nodesInitialized = useNodesInitialized();
  const [dropState, setDropState] = useState<'idle' | 'valid' | 'reject'>('idle');
  const dragStartAtByNode = useRef<Record<string, number>>({});

  useEffect(() => {
    if (fitViewRevision === 0 || !nodesInitialized) return;
    const frame = requestAnimationFrame(() => void reactFlow.fitView({ padding: 0.16 }));
    return () => cancelAnimationFrame(frame);
  }, [fitViewRevision, nodesInitialized, reactFlow]);

  const dragState = (event: DragEvent) => event.dataTransfer.types.includes(UNIT_DRAG_TYPE) ? 'valid' : 'reject';
  const dragOver = (event: DragEvent) => {
    markPerfSpan('ui.dragOver.projectNode', () => {
      const state = dragState(event);
      event.preventDefault();
      event.dataTransfer.dropEffect = state === 'valid' ? 'copy' : 'none';
      setDropState(state);
    });
  };
  const drop = (event: DragEvent) => {
    markPerfSpan('ui.drop.projectNode', () => {
      event.preventDefault();
      const unitId = event.dataTransfer.getData(UNIT_DRAG_TYPE);
      setDropState('idle');
      if (!unitId) return;
      const position = reactFlow.screenToFlowPosition({ x: event.clientX, y: event.clientY });
      const edgeElement = event.target instanceof Element ? event.target.closest<SVGGElement>('.react-flow__edge') : null;
      const edge = edgeElement ? displayedEdges.find(item => item.id === edgeElement.dataset.id) : null;
      const routeIndex = edge ? routeIndexFromEdge(edge) : null;
      if (routeIndex !== null) onInsertUnitAtRoute(unitId, routeIndex, position);
      else onAddUnitAt(unitId, position);
    }, { valid: event.dataTransfer.types.includes(UNIT_DRAG_TYPE) });
  };

  return (
    <div
      className={`flow-shell flow-shell--drop-${dropState}`}
      data-testid="project-canvas"
      onDragLeave={() => setDropState('idle')}
      onDragOver={dragOver}
      onDrop={drop}
    >
      <div className="edit-plane-grid" aria-hidden="true" />
      <ReactFlow
        nodes={nodes}
        edges={displayedEdges}
        nodeTypes={nodeTypes}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onNodeClick={(_, node) => onSelectNode(node.id)}
        onNodeDoubleClick={(_, node) => onOpenContractGraph(node.id)}
        onNodeDragStart={(_, node) => {
          dragStartAtByNode.current[node.id] = performance.now();
          markPerfSpan('ui.drag.projectNode.start', () => undefined, { nodeId: node.id });
        }}
        onNodeDrag={(_, node) => {
          markPerfSpan('ui.drag.projectNode', () => undefined, { nodeId: node.id });
        }}
        onNodeDragStop={(_, node) => {
          const startedAt = dragStartAtByNode.current[node.id];
          delete dragStartAtByNode.current[node.id];
          const data = node.data as ProjectNodeData;
          if (data.kind === 'unit') {
            const durationMs = startedAt ? performance.now() - startedAt : 0;
            const meta = { nodeId: data.instance.id, durationMs };
            markPerfSpan('ui.drag.projectNode.stop', () => undefined, meta);
            onMoveUnit(data.instance.id, node.position);
          }
        }}
        onEdgeClick={(_, edge) => {
          const routeIndex = routeIndexFromEdge(edge);
          if (routeIndex !== null) onSelectRoute(routeIndex);
        }}
        fitView
        fitViewOptions={{ padding: 0.16 }}
        minZoom={0.35}
        maxZoom={1.5}
      >
        <Controls />
        <MiniMap
          nodeColor={node => (node.data as ProjectNodeData).color}
          pannable
          zoomable
          style={{ background: '#0c1220' }}
        />
      </ReactFlow>
    </div>
  );
}

export function ProjectCanvas({
  edges,
  selectedRouteIndex,
  ...props
}: Props) {
  const displayedEdges = edges.map(edge => {
    const selected = routeIndexFromEdge(edge) === selectedRouteIndex;
    return {
      ...edge,
      animated: selected,
      style: {
        ...edge.style,
        stroke: selected ? 'var(--accent)' : 'var(--text-muted)',
        strokeWidth: selected ? 2.6 : 1.6,
      },
    };
  });

  return (
    <main className="canvas canvas--project canvas-area">
      <ReactFlowProvider>
        <ProjectFlow {...props} edges={edges} selectedRouteIndex={selectedRouteIndex} displayedEdges={displayedEdges} />
      </ReactFlowProvider>
    </main>
  );
}
