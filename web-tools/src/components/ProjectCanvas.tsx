import {
  BaseEdge,
  Controls,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type EdgeProps,
  type EdgeTypes,
  type Node,
  type OnEdgesChange,
  type OnNodesChange,
  type NodeTypes,
} from '@xyflow/react';
import { memo, useState, type DragEvent } from 'react';

import { ProjectNode } from './ProjectNode';
import { UNIT_DRAG_TYPE } from './ProjectSidebar';
import type {
  ProjectNodeData,
  ProjectRouteEdge,
  ProjectRoutePoint,
} from '../lib/projectGraph';
import { markPerfSpan } from '../lib/perfTelemetry';

const nodeTypes = { projectNode: ProjectNode } satisfies NodeTypes;
const ROUTE_CORNER_RADIUS = 10;

function pointToward(origin: ProjectRoutePoint, target: ProjectRoutePoint, distance: number): ProjectRoutePoint {
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

function routePath(points: ProjectRoutePoint[]): string {
  if (points.length === 0) return '';
  const path = [`M ${coordinate(points[0].x)} ${coordinate(points[0].y)}`];

  for (let index = 1; index < points.length - 1; index += 1) {
    const previous = points[index - 1];
    const corner = points[index];
    const next = points[index + 1];
    const radius = Math.min(
      ROUTE_CORNER_RADIUS,
      Math.hypot(corner.x - previous.x, corner.y - previous.y) / 2,
      Math.hypot(next.x - corner.x, next.y - corner.y) / 2,
    );
    const entry = pointToward(corner, previous, radius);
    const exit = pointToward(corner, next, radius);
    path.push(`L ${coordinate(entry.x)} ${coordinate(entry.y)}`);
    if (radius > 0) {
      path.push(
        `Q ${coordinate(corner.x)} ${coordinate(corner.y)} ${coordinate(exit.x)} ${coordinate(exit.y)}`,
      );
    }
  }

  const last = points.at(-1)!;
  path.push(`L ${coordinate(last.x)} ${coordinate(last.y)}`);
  return path.join(' ');
}

function renderPoints(
  planned: ProjectRoutePoint[] | undefined,
  source: ProjectRoutePoint,
  target: ProjectRoutePoint,
): ProjectRoutePoint[] {
  if (!planned || planned.length < 2) return [source, target];
  const points = planned.map(point => ({ ...point }));
  const plannedSource = points[0];
  const plannedTarget = points.at(-1)!;
  points[0] = source;
  points[points.length - 1] = target;

  if (points.length > 2) {
    if (plannedSource.y === points[1].y) points[1].y = source.y;
    else if (plannedSource.x === points[1].x) points[1].x = source.x;
    const beforeTarget = points[points.length - 2];
    if (beforeTarget.y === plannedTarget.y) beforeTarget.y = target.y;
    else if (beforeTarget.x === plannedTarget.x) beforeTarget.x = target.x;
  }

  return points.filter((point, index) => (
    index === 0 || point.x !== points[index - 1].x || point.y !== points[index - 1].y
  ));
}

const ProjectRoute = memo(({
  data,
  id,
  interactionWidth,
  markerEnd,
  markerStart,
  sourceX,
  sourceY,
  style,
  targetX,
  targetY,
}: EdgeProps<ProjectRouteEdge>) => (
  <BaseEdge
    id={id}
    interactionWidth={interactionWidth ?? 24}
    markerEnd={markerEnd}
    markerStart={markerStart}
    path={routePath(renderPoints(data?.points, { x: sourceX, y: sourceY }, { x: targetX, y: targetY }))}
    style={style}
  />
));

ProjectRoute.displayName = 'ProjectRoute';

const edgeTypes = { projectRoute: ProjectRoute } satisfies EdgeTypes;

type Props = {
  nodes: Node<ProjectNodeData>[];
  edges: ProjectRouteEdge[];
  selectedRouteIndex: number | null;
  onNodesChange: OnNodesChange<Node<ProjectNodeData>>;
  onEdgesChange: OnEdgesChange<ProjectRouteEdge>;
  onSelectNode: (id: string, additive?: boolean) => void;
  onOpenContractGraph: (id: string) => void;
  onSelectRoute: (index: number) => void;
  onAddUnit: (unitId: string) => void;
  onInsertUnitAtRoute: (unitId: string, routeIndex: number) => void;
};

type ProjectFlowProps = Props & {
  displayedEdges: ProjectRouteEdge[];
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
  onAddUnit,
  onInsertUnitAtRoute,
}: ProjectFlowProps) {
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
      const edgeElement = event.target instanceof Element ? event.target.closest<SVGGElement>('.react-flow__edge') : null;
      const edge = edgeElement ? displayedEdges.find(item => item.id === edgeElement.dataset.id) : null;
      const routeIndex = edge ? routeIndexFromEdge(edge) : null;
      if (routeIndex !== null) onInsertUnitAtRoute(unitId, routeIndex);
      else onAddUnit(unitId);
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
        edgeTypes={edgeTypes}
        nodeTypes={nodeTypes}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onNodeClick={(event, node) => onSelectNode(node.id, event.shiftKey)}
        onNodeDoubleClick={(_, node) => onOpenContractGraph(node.id)}
        onEdgeClick={(_, edge) => {
          const routeIndex = routeIndexFromEdge(edge);
          if (routeIndex !== null) onSelectRoute(routeIndex);
        }}
        fitView
        fitViewOptions={{ padding: 0.16 }}
        minZoom={0.35}
        maxZoom={1.5}
        multiSelectionKeyCode="Shift"
        nodesDraggable={false}
        selectionOnDrag
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
