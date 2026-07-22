import {
  BaseEdge,
  Controls,
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
import { UNIT_DRAG_TYPE } from './ProjectSidebar';
import type {
  ProjectNodeData,
  ProjectRouteEdge,
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
  const path = routePath(tuckRailUnderNodes(
    renderPoints(data?.points, { x: sourceX, y: sourceY }, { x: targetX, y: targetY }),
  ));
  return (
    <BaseEdge
      className="project-route__rail"
      id={id}
      interactionWidth={interactionWidth ?? 24}
      markerEnd={markerEnd}
      markerStart={markerStart}
      path={path}
      style={style}
    />
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
  onNodeContextMenu: NodeMouseHandler<Node<ProjectNodeData>>;
  onEdgeContextMenu: EdgeMouseHandler<ProjectRouteEdge>;
};

function routeIndexFromEdge(edge: Edge): number | null {
  const match = edge.id.match(/^route-(\d+)-/);
  return match ? Number(match[1]) : null;
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
  onNodesChange,
  onEdgesChange,
  onSelectNode,
  onSelectRoute,
  onAddUnit,
  onInsertUnitAtRoute,
  onConnectUnits,
  onNodeContextMenu,
  onEdgeContextMenu,
}: ProjectFlowProps) {
  const [dropState, setDropState] = useState<'idle' | 'valid' | 'reject'>('idle');
  const [connectionArmed, setConnectionArmed] = useState(false);

  useEffect(() => {
    if (!connectionArmed) return;
    const cancel = (event: KeyboardEvent) => {
      if (event.key === 'Escape') setConnectionArmed(false);
    };
    window.addEventListener('keydown', cancel);
    return () => window.removeEventListener('keydown', cancel);
  }, [connectionArmed]);

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
      className={`flow-shell flow-shell--drop-${dropState}${connectionArmed ? ' flow-shell--connecting' : ''}`}
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
        onEdgeClick={(_, edge) => {
          const routeIndex = routeIndexFromEdge(edge);
          if (routeIndex !== null) onSelectRoute(routeIndex);
        }}
        onPaneClick={() => setConnectionArmed(false)}
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
  onEditUnitContract,
  onAddParallelAtRoute,
  canPasteUnit,
  replacementOptions,
  parallelOptions,
  ...props
}: Props) {
  const [contextMenu, setContextMenu] = useState<(ContextMenuPoint & { nodeId: string }) | null>(null);
  const [routeMenu, setRouteMenu] = useState<(ContextMenuPoint & { routeIndex: number }) | null>(null);
  const [replacementOpen, setReplacementOpen] = useState(false);
  const [replacementUnit, setReplacementUnit] = useState('');
  const [parallelUnit, setParallelUnit] = useState('');
  const closeContextMenu = useCallback(() => {
    setContextMenu(null);
    setRouteMenu(null);
    setReplacementOpen(false);
  }, []);
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
  const enabledParallelOptions = parallelOptions.filter(option => !option.disabled);
  const chosenParallel = enabledParallelOptions.find(option => option.id === parallelUnit) ?? enabledParallelOptions[0];

  return (
    <main className="canvas canvas--project canvas-area" onKeyDownCapture={openKeyboardMenu}>
      <ReactFlowProvider>
        <ProjectFlow
          {...props}
          nodes={nodes}
          edges={edges}
          selectedRouteIndex={selectedRouteIndex}
          displayedEdges={displayedEdges}
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
            setParallelUnit(chosenParallel?.id ?? '');
          }}
        />
      </ReactFlowProvider>
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
            <div className="graph-context-menu__replace" role="group" aria-label="Parallel effect">
              <label>
                <span>Effect</span>
                <select
                  aria-label="Parallel effect"
                  onChange={event => setParallelUnit(event.target.value)}
                  value={chosenParallel?.id ?? ''}
                >
                  {parallelOptions.map(option => (
                    <option disabled={option.disabled} key={option.id} value={option.id}>
                      {option.label}{option.disabled ? ' — unavailable' : ''}
                    </option>
                  ))}
                </select>
              </label>
              {chosenParallel ? (
                <button
                  className="graph-context-menu__confirm"
                  onClick={() => {
                    onAddParallelAtRoute(chosenParallel.id, routeMenu.routeIndex);
                    closeContextMenu();
                  }}
                  type="button"
                >Create parallel path</button>
              ) : <p>No mono effects are available for this route.</p>}
            </div>
          ) : <p className="graph-context-menu__empty">No effects are available.</p>}
        </GraphContextMenu>
      ) : null}
    </main>
  );
}
