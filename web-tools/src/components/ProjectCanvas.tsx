import {
  BaseEdge,
  Controls,
  EdgeLabelRenderer,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type EdgeMouseHandler,
  type EdgeProps,
  type EdgeTypes,
  type Connection,
  type Node,
  type NodeMouseHandler,
  type OnEdgesChange,
  type OnNodesChange,
  type NodeTypes,
} from '@xyflow/react';
import { memo, useCallback, useEffect, useState, type DragEvent, type KeyboardEvent as ReactKeyboardEvent } from 'react';

import { GraphContextMenu, GraphMenuButton, type ContextMenuPoint } from './GraphContextMenu';
import { ProjectNode } from './ProjectNode';
import { PROJECT_INSTANCE_DRAG_TYPE, UNIT_DRAG_TYPE } from '../lib/graphDragTypes';
import type {
  ProjectNodeData,
  ProjectRouteEdge,
  ProjectRouteEdgeData,
  ProjectRoutePoint,
} from '../lib/projectGraph';
import { markPerfSpan } from '../lib/perfTelemetry';
import type { ProjectRoutingContract } from '../lib/projectV2Graph';

const nodeTypes = { projectNode: ProjectNode } satisfies NodeTypes;
const ROUTE_CORNER_RADIUS = 10;
const RAIL_NODE_INSET = 12;

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

function tuckRailUnderNodes(points: ProjectRoutePoint[]): ProjectRoutePoint[] {
  if (points.length < 2) return points;
  const tucked = points.map(point => ({ ...point }));
  const first = tucked[0];
  const afterFirst = tucked[1];
  if (first.y === afterFirst.y) {
    const direction = Math.sign(afterFirst.x - first.x) || 1;
    first.x -= direction * RAIL_NODE_INSET;
  }

  const last = tucked.at(-1)!;
  const beforeLast = tucked.at(-2)!;
  if (last.y === beforeLast.y) {
    const direction = Math.sign(last.x - beforeLast.x) || 1;
    last.x += direction * RAIL_NODE_INSET;
  }
  return tucked;
}

function railActionPoint(points: ProjectRoutePoint[]): ProjectRoutePoint {
  let longestHorizontal: { length: number; point: ProjectRoutePoint } | null = null;
  let longestSegment: { length: number; point: ProjectRoutePoint } | null = null;
  for (let index = 1; index < points.length; index += 1) {
    const start = points[index - 1];
    const end = points[index];
    const length = Math.hypot(end.x - start.x, end.y - start.y);
    const point = { x: (start.x + end.x) / 2, y: (start.y + end.y) / 2 };
    if (!longestSegment || length > longestSegment.length) longestSegment = { length, point };
    if (start.y === end.y && (!longestHorizontal || length > longestHorizontal.length)) {
      longestHorizontal = { length, point };
    }
  }
  return longestHorizontal?.point ?? longestSegment?.point ?? points[0] ?? { x: 0, y: 0 };
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
}: EdgeProps<ProjectRouteEdge>) => {
  const points = tuckRailUnderNodes(
    renderPoints(data?.points, { x: sourceX, y: sourceY }, { x: targetX, y: targetY }),
  );
  const path = routePath(points);
  const actionPoint = railActionPoint(points);
  const moveTarget = data?.moveTarget;
  return (
    <>
      <BaseEdge
        className="project-route__rail"
        id={id}
        interactionWidth={interactionWidth ?? 24}
        markerEnd={markerEnd}
        markerStart={markerStart}
        path={path}
        style={style}
      />
      {data?.routeIndex !== undefined && (moveTarget || data.onOpenBranchPicker) ? (
        <EdgeLabelRenderer>
          <div
            className={`project-route__action-anchor${moveTarget ? ' project-route__action-anchor--move' : ''}`}
            data-project-route-index={data.routeIndex}
            style={{
              transform: `translate(-50%, -50%) translate(${actionPoint.x}px, ${actionPoint.y}px)`,
            }}
          >
            {moveTarget ? (
              <button
                aria-label={moveTarget === 'current' ? 'Current rail position' : `Move unit to route ${data.routeIndex + 1}`}
                className={`project-route__action project-route__action--move${moveTarget === 'current' ? ' project-route__action--current' : ''}`}
                data-testid={`project-route-move-${data.routeIndex}`}
                disabled={moveTarget === 'current'}
                onClick={event => {
                  event.preventDefault();
                  event.stopPropagation();
                  data.onMoveHere?.(data.routeIndex);
                }}
                title={moveTarget === 'current' ? 'This unit already occupies this rail position' : 'Move here'}
                type="button"
              >
                <i className={`fa-solid ${moveTarget === 'current' ? 'fa-check' : 'fa-location-dot'}`} aria-hidden="true" />
              </button>
            ) : (
              <button
                aria-disabled={data.branchInteractionDisabled || data.insertTarget || undefined}
                aria-label={data.insertTarget
                  ? `Insert effect on route ${data.routeIndex + 1}`
                  : `Add branch on route ${data.routeIndex + 1}`}
                className={`project-route__action project-route__action--branch${data.branchHintVisible ? ' project-route__action--visible' : ''}${data.insertTarget ? ' project-route__action--insert' : ''}${data.branchInteractionDisabled ? ' project-route__action--suppressed' : ''}`}
                data-testid={`project-route-branch-${data.routeIndex}`}
                onClick={event => {
                  event.preventDefault();
                  event.stopPropagation();
                  if (data.branchInteractionDisabled || data.insertTarget) return;
                  const bounds = event.currentTarget.getBoundingClientRect();
                  data.onOpenBranchPicker?.(data.routeIndex, {
                    x: bounds.left + bounds.width / 2,
                    y: bounds.top + bounds.height / 2,
                  });
                }}
                tabIndex={data.branchInteractionDisabled || data.insertTarget ? -1 : 0}
                title={data.insertTarget ? 'Drop effect here' : 'Add branch'}
                type="button"
              >
                <i className={`fa-solid ${data.insertTarget ? 'fa-plus' : 'fa-code-branch'}`} aria-hidden="true" />
              </button>
            )}
          </div>
        </EdgeLabelRenderer>
      ) : null}
    </>
  );
});

ProjectRoute.displayName = 'ProjectRoute';

const edgeTypes = { projectRoute: ProjectRoute } satisfies EdgeTypes;

type Props = {
  nodes: Node<ProjectNodeData>[];
  edges: ProjectRouteEdge[];
  selectedRouteIndex: number | null;
  onNodesChange: OnNodesChange<Node<ProjectNodeData>>;
  onEdgesChange: OnEdgesChange<ProjectRouteEdge>;
  onSelectNode: (id: string, additive?: boolean) => void;
  onEditUnitContract: (instanceId: string) => void;
  onSelectRoute: (index: number) => void;
  onAddUnit: (unitId: string) => void;
  onInsertUnitAtRoute: (unitId: string, routeIndex: number) => void;
  onConnectUnits: (route: { from: string; to: string }) => void;
  onCopyUnit: (instanceId: string) => void;
  onCutUnit: (instanceId: string) => void;
  onPasteUnit: () => void;
  onRemoveUnit: (instanceId: string) => void;
  onReplaceUnit: (instanceId: string, unitId: string) => void;
  onMoveUnitToRoute: (instanceId: string, routeIndex: number) => void;
  onAddParallelAtRoute: (unitId: string, routeIndex: number) => void;
  canPasteUnit: boolean;
  replacementOptions: ProjectReplacementOption[];
  parallelOptions: ProjectParallelOption[];
};

export type ProjectReplacementOption = {
  id: string;
  label: string;
  paramCount: number;
  routing?: ProjectRoutingContract;
};

export type ProjectParallelOption = {
  id: string;
  label: string;
  disabled?: boolean;
  reason?: string;
};

type ProjectFlowProps = Omit<Props,
  'canPasteUnit'
  | 'onEditUnitContract'
  | 'onCopyUnit'
  | 'onCutUnit'
  | 'onPasteUnit'
  | 'onRemoveUnit'
  | 'onReplaceUnit'
  | 'replacementOptions'
  | 'onAddParallelAtRoute'
  | 'parallelOptions'> & {
  displayedEdges: ProjectRouteEdge[];
  movingInstanceId: string | null;
  onCancelMove: () => void;
  onOpenBranchPicker: (routeIndex: number, point: ContextMenuPoint) => void;
  onNodeContextMenu: NodeMouseHandler<Node<ProjectNodeData>>;
  onEdgeContextMenu: EdgeMouseHandler<ProjectRouteEdge>;
};

function routeIndexFromEdge(edge: Edge): number | null {
  const routeIndex = (edge.data as ProjectRouteEdgeData | undefined)?.routeIndex;
  if (typeof routeIndex === 'number') return routeIndex;
  const match = edge.id.match(/^route-(\d+)-/);
  return match ? Number(match[1]) : null;
}

function routeIndexFromEventTarget(target: EventTarget | null, edges: ProjectRouteEdge[]): number | null {
  if (!(target instanceof Element)) return null;
  const actionAnchor = target.closest<HTMLElement>('[data-project-route-index]');
  if (actionAnchor) {
    const routeIndex = Number(actionAnchor.dataset.projectRouteIndex);
    if (Number.isInteger(routeIndex)) return routeIndex;
  }
  const edgeElement = target.closest<SVGGElement>('.react-flow__edge');
  const edge = edgeElement ? edges.find(item => item.id === edgeElement.dataset.id) : null;
  return edge ? routeIndexFromEdge(edge) : null;
}

function projectEndpoint(nodeId: string | null, handleId: string | null, direction: 'source' | 'target'): string | null {
  if (!nodeId || !handleId) return null;
  if (nodeId === 'system-input' && direction === 'source') return 'system.input';
  if (nodeId === 'system-output' && direction === 'target') return 'system.output';
  if (!nodeId.startsWith('unit-')) return null;
  return `${nodeId.slice('unit-'.length)}.${handleId}`;
}

function ProjectFlow({
  nodes,
  displayedEdges,
  selectedRouteIndex,
  onNodesChange,
  onEdgesChange,
  onSelectNode,
  onSelectRoute,
  onAddUnit,
  onInsertUnitAtRoute,
  onMoveUnitToRoute,
  onConnectUnits,
  movingInstanceId,
  onCancelMove,
  onOpenBranchPicker,
  onNodeContextMenu,
  onEdgeContextMenu,
}: ProjectFlowProps) {
  const [dropState, setDropState] = useState<'idle' | 'valid' | 'reject'>('idle');
  const [connectionArmed, setConnectionArmed] = useState(false);
  const [dragKind, setDragKind] = useState<'library' | 'instance' | null>(null);
  const [draggedInstanceId, setDraggedInstanceId] = useState<string | null>(null);
  const [hoveredRouteIndex, setHoveredRouteIndex] = useState<number | null>(null);
  const [dropRouteIndex, setDropRouteIndex] = useState<number | null>(null);

  const resetDrag = useCallback(() => {
    setDropState('idle');
    setDragKind(null);
    setDraggedInstanceId(null);
    setDropRouteIndex(null);
  }, []);

  useEffect(() => {
    const cancel = (event: KeyboardEvent) => {
      if (event.key !== 'Escape') return;
      setConnectionArmed(false);
      onCancelMove();
    };
    window.addEventListener('keydown', cancel);
    return () => window.removeEventListener('keydown', cancel);
  }, [onCancelMove]);

  useEffect(() => {
    window.addEventListener('dragend', resetDrag);
    return () => window.removeEventListener('dragend', resetDrag);
  }, [resetDrag]);

  const dragType = (event: DragEvent): 'library' | 'instance' | 'reject' => {
    if (event.dataTransfer.types.includes(PROJECT_INSTANCE_DRAG_TYPE)) return 'instance';
    if (event.dataTransfer.types.includes(UNIT_DRAG_TYPE)) return 'library';
    return 'reject';
  };

  const dragOver = (event: DragEvent) => {
    markPerfSpan('ui.dragOver.projectNode', () => {
      const kind = dragType(event);
      const routeIndex = routeIndexFromEventTarget(event.target, displayedEdges);
      const state = kind === 'library' || (kind === 'instance' && routeIndex !== null) ? 'valid' : 'reject';
      event.preventDefault();
      event.dataTransfer.dropEffect = state === 'valid' ? (kind === 'instance' ? 'move' : 'copy') : 'none';
      setDropState(state);
      setDragKind(kind === 'reject' ? null : kind);
      setDropRouteIndex(routeIndex);
    });
  };
  const drop = (event: DragEvent) => {
    markPerfSpan('ui.drop.projectNode', () => {
      event.preventDefault();
      const instanceId = event.dataTransfer.getData(PROJECT_INSTANCE_DRAG_TYPE);
      const unitId = event.dataTransfer.getData(UNIT_DRAG_TYPE);
      const routeIndex = routeIndexFromEventTarget(event.target, displayedEdges);
      resetDrag();
      if (instanceId) {
        const edge = routeIndex === null
          ? null
          : displayedEdges.find(candidate => routeIndexFromEdge(candidate) === routeIndex);
        const nodeId = `unit-${instanceId}`;
        if (edge && edge.source !== nodeId && edge.target !== nodeId) {
          onMoveUnitToRoute(instanceId, routeIndex!);
        }
        return;
      }
      if (!unitId) return;
      if (routeIndex !== null) onInsertUnitAtRoute(unitId, routeIndex);
      else onAddUnit(unitId);
    }, { valid: dragType(event) !== 'reject' });
  };

  const interactiveEdges = displayedEdges.map(edge => {
    const routeIndex = routeIndexFromEdge(edge);
    const edgeData: ProjectRouteEdgeData = edge.data ?? { points: [], routeIndex: routeIndex ?? 0 };
    const activeMovingInstanceId = movingInstanceId ?? draggedInstanceId;
    const movingNodeId = activeMovingInstanceId ? `unit-${activeMovingInstanceId}` : null;
    const currentPosition = Boolean(movingNodeId && (edge.source === movingNodeId || edge.target === movingNodeId));
    const interactionsHidden = connectionArmed || dragKind !== null || draggedInstanceId !== null;
    const dropTarget = routeIndex !== null && routeIndex === dropRouteIndex;
    return {
      ...edge,
      style: {
        ...edge.style,
        stroke: dropTarget ? 'var(--accent)' : edge.style?.stroke,
        strokeWidth: dropTarget ? 4 : edge.style?.strokeWidth,
      },
      data: {
        ...edgeData,
        routeIndex: routeIndex ?? edgeData.routeIndex,
        branchHintVisible: !movingInstanceId
          && !interactionsHidden
          && routeIndex !== null
          && (routeIndex === selectedRouteIndex || routeIndex === hoveredRouteIndex),
        branchInteractionDisabled: connectionArmed,
        insertTarget: dragKind === 'library',
        moveTarget: activeMovingInstanceId ? (currentPosition ? 'current' : 'available') : undefined,
        onOpenBranchPicker: activeMovingInstanceId ? undefined : onOpenBranchPicker,
        onMoveHere: movingInstanceId && !currentPosition
          ? index => {
            onMoveUnitToRoute(movingInstanceId, index);
            onCancelMove();
          }
          : undefined,
      },
    } satisfies ProjectRouteEdge;
  });

  return (
    <div
      className={`flow-shell flow-shell--drop-${dropState}${connectionArmed ? ' flow-shell--connecting' : ''}${movingInstanceId ? ' flow-shell--moving' : ''}${dragKind === 'instance' ? ' flow-shell--dragging-instance' : ''}`}
      data-testid="project-canvas"
      onDragEnd={resetDrag}
      onDragOver={dragOver}
      onDragStart={event => {
        const instanceId = event.dataTransfer.getData(PROJECT_INSTANCE_DRAG_TYPE);
        if (!instanceId) return;
        setDraggedInstanceId(instanceId);
        setDragKind('instance');
      }}
      onDrop={drop}
    >
      <div className="edit-plane-grid" aria-hidden="true" />
      <ReactFlow
        nodes={nodes}
        edges={interactiveEdges}
        edgeTypes={edgeTypes}
        nodeTypes={nodeTypes}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        connectOnClick
        onConnect={(connection: Connection) => {
          const from = projectEndpoint(connection.source, connection.sourceHandle, 'source');
          const to = projectEndpoint(connection.target, connection.targetHandle, 'target');
          setConnectionArmed(false);
          if (from && to) onConnectUnits({ from, to });
        }}
        onConnectStart={() => setConnectionArmed(true)}
        onConnectEnd={() => setConnectionArmed(false)}
        onClickConnectStart={() => setConnectionArmed(true)}
        onClickConnectEnd={() => setConnectionArmed(false)}
        onNodeClick={(event, node) => onSelectNode(node.id, event.shiftKey)}
        onNodeContextMenu={onNodeContextMenu}
        onEdgeContextMenu={onEdgeContextMenu}
        onEdgeMouseEnter={(_, edge) => setHoveredRouteIndex(routeIndexFromEdge(edge))}
        onEdgeMouseLeave={(_, edge) => {
          const routeIndex = routeIndexFromEdge(edge);
          setHoveredRouteIndex(current => current === routeIndex ? null : current);
        }}
        onEdgeClick={(_, edge) => {
          const routeIndex = routeIndexFromEdge(edge);
          if (routeIndex === null) return;
          if (movingInstanceId) {
            const movingNodeId = `unit-${movingInstanceId}`;
            if (edge.source !== movingNodeId && edge.target !== movingNodeId) {
              onMoveUnitToRoute(movingInstanceId, routeIndex);
              onCancelMove();
            }
            return;
          }
          onSelectRoute(routeIndex);
        }}
        onPaneClick={() => {
          setConnectionArmed(false);
          onCancelMove();
        }}
        fitView
        fitViewOptions={{ padding: 0.16 }}
        minZoom={0.2}
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
          style={{ background: 'var(--bg-canvas)' }}
        />
      </ReactFlow>
    </div>
  );
}

export function ProjectCanvas({
  edges,
  nodes,
  selectedRouteIndex,
  onCopyUnit,
  onCutUnit,
  onPasteUnit,
  onRemoveUnit,
  onReplaceUnit,
  onMoveUnitToRoute,
  onEditUnitContract,
  onAddParallelAtRoute,
  canPasteUnit,
  replacementOptions,
  parallelOptions,
  ...props
}: Props) {
  const { onSelectRoute } = props;
  const [contextMenu, setContextMenu] = useState<(ContextMenuPoint & { nodeId: string }) | null>(null);
  const [routeMenu, setRouteMenu] = useState<(ContextMenuPoint & { routeIndex: number }) | null>(null);
  const [replacementOpen, setReplacementOpen] = useState(false);
  const [replacementUnit, setReplacementUnit] = useState('');
  const [movingInstanceId, setMovingInstanceId] = useState<string | null>(null);
  const closeContextMenu = useCallback(() => {
    setContextMenu(null);
    setRouteMenu(null);
    setReplacementOpen(false);
  }, []);
  const cancelMove = useCallback(() => setMovingInstanceId(null), []);
  useEffect(() => {
    if (movingInstanceId && !nodes.some(node => (
      node.data.kind === 'unit' && node.data.instance.id === movingInstanceId
    ))) {
      setMovingInstanceId(null);
    }
  }, [movingInstanceId, nodes]);
  const contextNode = contextMenu ? nodes.find(node => node.id === contextMenu.nodeId) : null;
  const unitData = contextNode?.data.kind === 'unit' ? contextNode.data : null;
  const currentRouting = unitData?.ports?.routing;
  const availableReplacements = unitData
    ? replacementOptions.filter(option => {
      if (option.id === unitData.instance.unit) return false;
      if (!currentRouting) return !option.routing;
      return option.routing?.role === currentRouting.role
        && option.routing.paths.length === currentRouting.paths.length
        && option.routing.paths.every((path, index) => (
          path.port === currentRouting.paths[index]?.port
          && path.levelParam === currentRouting.paths[index]?.levelParam
        ));
    })
    : [];
  const chosenReplacement = availableReplacements.find(option => option.id === replacementUnit)
    ?? availableReplacements[0];
  const canReplace = Boolean(currentRouting || (unitData?.ports?.userPlaceable ?? (
    unitData?.ports?.inputs.length === 1 && unitData?.ports?.outputs.length === 1
  )));
  const incidentRoutes = contextNode
    ? edges.filter(edge => edge.source === contextNode.id || edge.target === contextNode.id).length
    : 0;

  const openKeyboardMenu = (event: ReactKeyboardEvent<HTMLElement>) => {
    if (event.key !== 'ContextMenu' && !(event.shiftKey && event.key === 'F10')) return;
    const nodeElement = (event.target as Element).closest<HTMLElement>('.react-flow__node[data-id^="unit-"]');
    const nodeId = nodeElement?.dataset.id;
    if (!nodeElement || !nodeId) return;
    event.preventDefault();
    const bounds = nodeElement.getBoundingClientRect();
    setContextMenu({ nodeId, x: bounds.left + 24, y: bounds.top + 24 });
    setRouteMenu(null);
    setReplacementOpen(false);
  };
  const displayedEdges = edges.map(edge => {
    const selected = routeIndexFromEdge(edge) === selectedRouteIndex;
    return {
      ...edge,
      animated: selected,
      style: {
        ...edge.style,
        stroke: selected ? 'var(--accent)' : 'var(--project-rail)',
        strokeWidth: selected ? 3.2 : 2.2,
      },
    };
  });
  const openBranchPicker = useCallback((routeIndex: number, point: ContextMenuPoint) => {
    onSelectRoute(routeIndex);
    setContextMenu(null);
    setRouteMenu({ routeIndex, ...point });
    setReplacementOpen(false);
  }, [onSelectRoute]);

  return (
    <main className="canvas canvas--project canvas-area" onKeyDownCapture={openKeyboardMenu}>
      <ReactFlowProvider>
        <ProjectFlow
          {...props}
          nodes={nodes}
          edges={edges}
          selectedRouteIndex={selectedRouteIndex}
          displayedEdges={displayedEdges}
          movingInstanceId={movingInstanceId}
          onCancelMove={cancelMove}
          onMoveUnitToRoute={onMoveUnitToRoute}
          onOpenBranchPicker={openBranchPicker}
          onNodeContextMenu={(event, node) => {
            if (node.data.kind !== 'unit') return;
            event.preventDefault();
            props.onSelectNode(node.id);
            setContextMenu({ nodeId: node.id, x: event.clientX, y: event.clientY });
            setRouteMenu(null);
            setReplacementOpen(false);
          }}
          onEdgeContextMenu={(event, edge) => {
            const routeIndex = routeIndexFromEdge(edge);
            if (routeIndex === null) return;
            event.preventDefault();
            props.onSelectRoute(routeIndex);
            setContextMenu(null);
            setRouteMenu({ routeIndex, x: event.clientX, y: event.clientY });
          }}
        />
      </ReactFlowProvider>
      {movingInstanceId ? (
        <div className="project-move-prompt" role="status">
          <i className="fa-solid fa-location-dot" aria-hidden="true" />
          <span>Moving <strong>{movingInstanceId}</strong> · choose a rail</span>
          <button onClick={cancelMove} type="button">Cancel</button>
        </div>
      ) : null}
      {contextMenu && unitData ? (
        <GraphContextMenu
          label={`${unitData.instance.id} actions`}
          onClose={closeContextMenu}
          point={contextMenu}
        >
          <div className="graph-context-menu__title">
            <strong>{unitData.instance.id}</strong>
            <span>{unitData.unit.name}</span>
          </div>
          <GraphMenuButton
            disabled={!unitData.bypassAvailable}
            icon="fa-power-off"
            onClick={() => {
              void unitData.onBypassChange?.(unitData.instance.id, !unitData.bypassed);
              closeContextMenu();
            }}
            title={unitData.instance.routing
              ? 'Routing helpers are always active.'
              : unitData.bypassAvailable ? undefined : 'Start audio preview to toggle this unit.'}
          >
            {unitData.bypassed ? 'Turn on' : 'Turn off'}
          </GraphMenuButton>
          <GraphMenuButton
            disabled={Boolean(unitData.instance.routing || currentRouting)}
            icon="fa-location-dot"
            onClick={() => {
              setMovingInstanceId(unitData.instance.id);
              closeContextMenu();
            }}
            title={unitData.instance.routing || currentRouting
              ? 'Panners and mixers stay fixed on their routing section.'
              : 'Choose another rail position for this unit.'}
          >Move…</GraphMenuButton>
          <GraphMenuButton
            disabled={Boolean(currentRouting)}
            icon="fa-diagram-project"
            onClick={() => {
              onEditUnitContract(unitData.instance.id);
              closeContextMenu();
            }}
            title={currentRouting ? 'Routing helpers do not expose contracts.' : undefined}
          >Edit Contract</GraphMenuButton>
          <GraphMenuButton
            disabled={!canReplace || availableReplacements.length === 0}
            icon="fa-repeat"
            onClick={() => {
              setReplacementOpen(open => !open);
              setReplacementUnit(chosenReplacement?.id ?? '');
            }}
            title={canReplace ? undefined : 'This unit does not have a replaceable contract.'}
          >Replace…</GraphMenuButton>
          {replacementOpen && chosenReplacement ? (
            <div className="graph-context-menu__replace" role="group" aria-label="Replacement preview">
              <label>
                <span>Replace with</span>
                <select
                  aria-label="Replacement unit"
                  onChange={event => setReplacementUnit(event.target.value)}
                  value={chosenReplacement.id}
                >
                  {availableReplacements.map(option => (
                    <option key={option.id} value={option.id}>{option.label}</option>
                  ))}
                </select>
              </label>
              <p>
                Keeps the instance ID and {incidentRoutes} {incidentRoutes === 1 ? 'route' : 'routes'}.
                Resets controls and scene values to {chosenReplacement.paramCount} defaults.
              </p>
              <button
                className="graph-context-menu__confirm"
                onClick={() => {
                  onReplaceUnit(unitData.instance.id, chosenReplacement.id);
                  closeContextMenu();
                }}
                type="button"
              >Confirm replace</button>
            </div>
          ) : null}
          <div className="graph-context-menu__separator" role="separator" />
          <GraphMenuButton disabled={Boolean(currentRouting)} icon="fa-scissors" onClick={() => { onCutUnit(unitData.instance.id); closeContextMenu(); }} title={currentRouting ? 'Routing helpers are removed as a complete split/join pair.' : undefined}>Cut</GraphMenuButton>
          <GraphMenuButton disabled={Boolean(currentRouting)} icon="fa-copy" onClick={() => { onCopyUnit(unitData.instance.id); closeContextMenu(); }} title={currentRouting ? 'Routing helpers are created through Add in parallel.' : undefined}>Copy</GraphMenuButton>
          <GraphMenuButton disabled={!canPasteUnit} icon="fa-paste" onClick={() => { onPasteUnit(); closeContextMenu(); }}>Paste</GraphMenuButton>
          <GraphMenuButton danger icon="fa-trash" onClick={() => { onRemoveUnit(unitData.instance.id); closeContextMenu(); }}>{currentRouting ? 'Remove split/join' : 'Remove'}</GraphMenuButton>
        </GraphContextMenu>
      ) : null}
      {routeMenu ? (
        <GraphContextMenu
          label={`Route ${routeMenu.routeIndex + 1} actions`}
          onClose={closeContextMenu}
          point={routeMenu}
        >
          <div className="graph-context-menu__title">
            <strong>Add in parallel</strong>
            <span>Pan 2 → effect → Mix 2</span>
          </div>
          {parallelOptions.length > 0 ? (
            <div className="graph-context-menu__branch-list" role="group" aria-label="Choose an effect for this branch">
              {parallelOptions.map(option => (
                <button
                  disabled={option.disabled}
                  key={option.id}
                  onClick={() => {
                    onAddParallelAtRoute(option.id, routeMenu.routeIndex);
                    closeContextMenu();
                  }}
                  title={option.disabled ? option.reason : `Create a branch with ${option.label}`}
                  type="button"
                >
                  <i className="fa-solid fa-wave-square" aria-hidden="true" />
                  <span>
                    <strong>{option.label}</strong>
                    <small>{option.disabled ? option.reason ?? 'Unavailable' : 'Add on a new two-path branch'}</small>
                  </span>
                </button>
              ))}
            </div>
          ) : <p className="graph-context-menu__empty">No effects are available.</p>}
        </GraphContextMenu>
      ) : null}
    </main>
  );
}
