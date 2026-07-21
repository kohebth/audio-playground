import type { Edge, Node } from '@xyflow/react';
import dagre from 'dagre';
import type { ProjectInspect, ProjectInstance, ProjectRoute, ProjectUnit } from './backendSamples';
import type { ProjectPortCatalog, ProjectUnitPorts } from './projectV2Graph';

export type ProjectNodeData =
  | {
      kind: 'system';
      id: string;
      label: string;
      detail: string;
      color: string;
    }
  | {
      kind: 'unit';
      instance: ProjectInstance;
      unit: ProjectUnit;
      index: number;
      color: string;
      paramControls?: ProjectParamControl[];
      onParamChange?: (instanceId: string, paramKey: string, value: string) => void;
      bypassed?: boolean;
      bypassAvailable?: boolean;
      onBypassChange?: (instanceId: string, enabled: boolean) => Promise<void>;
      ports?: ProjectUnitPorts;
    };

export type ProjectParamControl = {
  key: string;
  label: string;
  type: string;
  min?: string;
  max?: string;
  unit?: string;
};

export type ProjectRoutePoint = { x: number; y: number };

export type ProjectRouteEdgeData = {
  points: ProjectRoutePoint[];
};

export type ProjectRouteEdge = Edge<ProjectRouteEdgeData, 'projectRoute'>;

const UNIT_NODE_COMPACT_WIDTH = 140;
const UNIT_NODE_WIDE_WIDTH = 190;
const UNIT_NODE_EMPTY_HEIGHT = 132;
const UNIT_NODE_FIRST_KNOB_ROW_HEIGHT = 166;
const UNIT_NODE_EXTRA_KNOB_ROW_HEIGHT = 77;
const KNOBS_PER_ROW = 3;
const SYSTEM_NODE_WIDTH = 100;
const SYSTEM_NODE_HEIGHT = 118;
const UNIT_COLORS = ['#3b82f6', '#059669', '#2563eb', '#db2777', '#7c3aed', '#dc2626'];

function endpointNodeId(endpoint: string): string {
  if (endpoint === 'system.input') return 'system-input';
  if (endpoint === 'system.output') return 'system-output';
  return `unit-${endpoint.split('.')[0]}`;
}

function routeEdgeId(index: number, source: string, target: string): string {
  return `route-${index}-${source}-${target}`;
}

function createSystemNode(id: string, label: string, detail: string, color: string): Node<ProjectNodeData> {
  return {
    id,
    type: 'projectNode',
    position: { x: 0, y: 0 },
    data: { kind: 'system', id, label, detail, color },
  };
}

function appendPoint(points: ProjectRoutePoint[], point: ProjectRoutePoint): void {
  const previous = points.at(-1);
  if (previous?.x === point.x && previous.y === point.y) return;
  points.push(point);
}

function removeCollinearPoints(points: ProjectRoutePoint[]): ProjectRoutePoint[] {
  return points.filter((point, index) => {
    if (index === 0 || index === points.length - 1) return true;
    const previous = points[index - 1];
    const next = points[index + 1];
    return !((previous.x === point.x && point.x === next.x)
      || (previous.y === point.y && point.y === next.y));
  });
}

function orthogonalRoutePoints(
  layoutPoints: ProjectRoutePoint[],
  source: ProjectRoutePoint,
  target: ProjectRoutePoint,
): ProjectRoutePoint[] {
  const points: ProjectRoutePoint[] = [source];
  const hints = layoutPoints.slice(1, -1);

  if (hints.length === 0 && source.x !== target.x && source.y !== target.y) {
    const midpointX = source.x + (target.x - source.x) / 2;
    hints.push({ x: midpointX, y: source.y }, { x: midpointX, y: target.y });
  }

  for (const hint of hints) {
    const previous = points.at(-1)!;
    if (previous.x !== hint.x && previous.y !== hint.y) {
      appendPoint(points, { x: hint.x, y: previous.y });
    }
    appendPoint(points, hint);
  }

  const previous = points.at(-1)!;
  if (previous.x !== target.x && previous.y !== target.y) {
    appendPoint(points, { x: previous.x, y: target.y });
  }
  appendPoint(points, target);
  return removeCollinearPoints(points);
}

function createRouteEdge(route: ProjectRoute, index: number, graph: dagre.graphlib.Graph): ProjectRouteEdge {
  const source = endpointNodeId(route.from);
  const target = endpointNodeId(route.to);
  const id = routeEdgeId(index, source, target);
  const sourceLayout = graph.node(source);
  const targetLayout = graph.node(target);
  const layoutPoints = graph.edge({ v: source, w: target, name: id })?.points ?? [];
  const points = orthogonalRoutePoints(
    layoutPoints,
    { x: sourceLayout.x + sourceLayout.width / 2, y: sourceLayout.y },
    { x: targetLayout.x - targetLayout.width / 2, y: targetLayout.y },
  );

  return {
    id,
    type: 'projectRoute',
    source,
    target,
    sourceHandle: route.from.split('.').at(-1),
    targetHandle: route.to.split('.').at(-1),
    data: { points },
    style: { stroke: '#64748b', strokeWidth: 1.6 },
  };
}

function unitNodeDimensions(paramCount: number): { width: number; height: number } {
  const rows = Math.ceil(paramCount / KNOBS_PER_ROW);
  return {
    width: paramCount >= KNOBS_PER_ROW ? UNIT_NODE_WIDE_WIDTH : UNIT_NODE_COMPACT_WIDTH,
    height: rows === 0
      ? UNIT_NODE_EMPTY_HEIGHT
      : UNIT_NODE_FIRST_KNOB_ROW_HEIGHT + (rows - 1) * UNIT_NODE_EXTRA_KNOB_ROW_HEIGHT,
  };
}

export function buildProjectGraph(
  project: ProjectInspect,
  ports: ProjectPortCatalog = {},
): { nodes: Node<ProjectNodeData>[]; edges: ProjectRouteEdge[] } {
  const unitsById = new Map(project.units.map(unit => [unit.id, unit]));
  const graph = new dagre.graphlib.Graph({ multigraph: true });
  const nodes: Node<ProjectNodeData>[] = [
    createSystemNode('system-input', 'Input', 'system.input', '#0891b2'),
    createSystemNode('system-output', 'Output', 'system.output', '#65a30d'),
  ];

  graph.setGraph({ rankdir: 'LR', nodesep: 44, ranksep: 72, marginx: 32, marginy: 32 });
  graph.setDefaultEdgeLabel(() => ({}));

  for (const node of nodes) {
    graph.setNode(node.id, { width: SYSTEM_NODE_WIDTH, height: SYSTEM_NODE_HEIGHT });
  }

  project.nodes.forEach((instance, index) => {
    const unit = unitsById.get(instance.unit);
    if (!unit) return;

    const node: Node<ProjectNodeData> = {
      id: `unit-${instance.id}`,
      type: 'projectNode',
      position: { x: 0, y: 0 },
      data: {
        kind: 'unit',
        instance,
        unit,
        index,
        color: UNIT_COLORS[index % UNIT_COLORS.length],
        ports: ports[instance.unit],
      },
    };

    nodes.push(node);
    graph.setNode(node.id, unitNodeDimensions(instance.params.length));
  });

  project.routes.forEach((route, index) => {
    const source = endpointNodeId(route.from);
    const target = endpointNodeId(route.to);
    graph.setEdge(source, target, {}, routeEdgeId(index, source, target));
  });

  dagre.layout(graph);

  for (const node of nodes) {
    const position = graph.node(node.id);
    const dimensions = node.data.kind === 'system'
      ? { width: SYSTEM_NODE_WIDTH, height: SYSTEM_NODE_HEIGHT }
      : unitNodeDimensions(node.data.instance.params.length);
    node.position = {
      x: position.x - dimensions.width / 2,
      y: position.y - dimensions.height / 2,
    };
  }

  const edges = project.routes.map((route, index) => createRouteEdge(route, index, graph));
  return { nodes, edges };
}
