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
  useReactFlow,
} from '@xyflow/react';
import { useState, type DragEvent } from 'react';

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
  onNodesChange: OnNodesChange<Node<ProjectNodeData>>;
  onEdgesChange: OnEdgesChange;
  onSelectNode: (id: string) => void;
  onOpenContractGraph: (id: string) => void;
  onSelectRoute: (index: number) => void;
  onAddUnitAt: (unitId: string, position: GraphPosition) => void;
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
  onMoveUnit,
}: ProjectFlowProps) {
  const reactFlow = useReactFlow();
  const [dropState, setDropState] = useState<'idle' | 'valid' | 'reject'>('idle');

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
      onAddUnitAt(unitId, reactFlow.screenToFlowPosition({ x: event.clientX, y: event.clientY }));
    }, { valid: event.dataTransfer.types.includes(UNIT_DRAG_TYPE) });
  };

  return (
    <div
      className={`flow-shell flow-shell--drop-${dropState}`}
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
        onNodeDragStop={(_, node) => {
          const data = node.data as ProjectNodeData;
          if (data.kind === 'unit') onMoveUnit(data.instance.id, node.position);
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
