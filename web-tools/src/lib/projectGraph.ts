import type { Edge, Node } from '@xyflow/react';
import dagre from 'dagre';
import type { ProjectInspect, ProjectInstance, ProjectRoute, ProjectUnit } from './backendSamples';
import type { ProjectPortCatalog, ProjectRoutingRole, ProjectUnitPorts } from './projectV2Graph';

export type ProjectNodeData =
  | {
      kind: 'system';
      id: string;
      label: string;
      detail: string;
      color: string;
      visualLayout: ProjectNodeVisualLayout;
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
      routingLayout?: ProjectRoutingNodeLayout;
      visualLayout: ProjectNodeVisualLayout;
    };

export type ProjectParamControl = {
  key: string;
  label: string;
  type: string;
  min?: string;
  max?: string;
  unit?: string;
  control?: string;
};

export type ProjectRoutePoint = { x: number; y: number };

export type ProjectRouteEdgeData = {
  points: ProjectRoutePoint[];
  routeIndex: number;
  branchHintVisible?: boolean;
  branchInteractionDisabled?: boolean;
  moveTarget?: 'available' | 'current';
  onOpenBranchPicker?: (routeIndex: number, point: { x: number; y: number }) => void;
  onMoveHere?: (routeIndex: number) => void;
};

export type ProjectRouteEdge = Edge<ProjectRouteEdgeData, 'projectRoute'>;

export type ProjectRoutingNodeLayout = {
  height: number;
  inputTops: Record<string, number>;
  outputTops: Record<string, number>;
  controlTops: Record<string, number>;
};

export type ProjectNodeVisualLayout = {
  width: number;
  height: number;
  railTop: number;
};

const UNIT_NODE_COMPACT_WIDTH = 140;
const UNIT_NODE_WIDE_WIDTH = 190;
const ROUTING_PANNER_WIDTH = 104;
// Keep Dagre's card model identical to the rendered pedal. The previous smaller
// values made React Flow attach edges several pixels away from Dagre's rail.
const UNIT_NODE_EMPTY_HEIGHT = 147;
const UNIT_NODE_FIRST_KNOB_ROW_HEIGHT = 183;
const UNIT_NODE_EXTRA_KNOB_ROW_HEIGHT = 82;
const KNOBS_PER_ROW = 3;
const ROUTING_PATH_GAP = 98;
// Leaves room for the pedal header, the 40px knob, its labels, and the footer around the outer lanes.
const ROUTING_PATH_PADDING = 84;
const SYSTEM_NODE_WIDTH = 100;
const SYSTEM_NODE_HEIGHT = 109;
const UNIT_COLORS = ['#3b82f6', '#059669', '#2563eb', '#db2777', '#7c3aed', '#dc2626'];

type NodeGeometry = {
  left: number;
  right: number;
  top: number;
  bottom: number;
  inputYs: Record<string, number>;
  outputYs: Record<string, number>;
};

type RoutingNode = {
  id: string;
  instance: ProjectInstance;
  ports: ProjectUnitPorts;
};

function endpointNodeId(endpoint: string): string {
  if (endpoint === 'system.input') return 'system-input';
  if (endpoint === 'system.output') return 'system-output';
  return `unit-${endpoint.split('.')[0]}`;
}

function routeEdgeId(index: number, source: string, target: string): string {
  return `route-${index}-${source}-${target}`;
}

function createSystemNode(id: string, label: string, detail: string, color: string): Node<ProjectNodeData> {
  const visualLayout = {
    width: SYSTEM_NODE_WIDTH,
    height: SYSTEM_NODE_HEIGHT,
    railTop: SYSTEM_NODE_HEIGHT / 2,
  };
  return {
    id,
    type: 'projectNode',
    position: { x: 0, y: 0 },
    data: { kind: 'system', id, label, detail, color, visualLayout },
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

function horizontalRouteIsClear(
  source: ProjectRoutePoint,
  target: ProjectRoutePoint,
  sourceId: string,
  targetId: string,
  geometries: Map<string, NodeGeometry>,
): boolean {
  if (source.y !== target.y) return false;
  const left = Math.min(source.x, target.x);
  const right = Math.max(source.x, target.x);
  return [...geometries].every(([id, geometry]) => {
    if (id === sourceId || id === targetId) return true;
    const overlapsX = geometry.right > left && geometry.left < right;
    const overlapsY = source.y > geometry.top && source.y < geometry.bottom;
    return !overlapsX || !overlapsY;
  });
}

function createRouteEdge(
  route: ProjectRoute,
  index: number,
  graph: dagre.graphlib.Graph,
  geometries: Map<string, NodeGeometry>,
): ProjectRouteEdge {
  const source = endpointNodeId(route.from);
  const target = endpointNodeId(route.to);
  const id = routeEdgeId(index, source, target);
  const sourcePort = route.from.split('.').at(-1)!;
  const targetPort = route.to.split('.').at(-1)!;
  const sourceGeometry = geometries.get(source)!;
  const targetGeometry = geometries.get(target)!;
  const layoutPoints = graph.edge({ v: source, w: target, name: id })?.points ?? [];
  const sourcePoint = {
    x: sourceGeometry.right,
    y: sourceGeometry.outputYs[sourcePort] ?? (sourceGeometry.top + sourceGeometry.bottom) / 2,
  };
  const targetPoint = {
    x: targetGeometry.left,
    y: targetGeometry.inputYs[targetPort] ?? (targetGeometry.top + targetGeometry.bottom) / 2,
  };
  const points = horizontalRouteIsClear(sourcePoint, targetPoint, source, target, geometries)
    ? [sourcePoint, targetPoint]
    : orthogonalRoutePoints(layoutPoints, sourcePoint, targetPoint);

  return {
    id,
    type: 'projectRoute',
    source,
    target,
    sourceHandle: sourcePort,
    targetHandle: targetPort,
    data: { points, routeIndex: index },
    style: { stroke: '#64748b', strokeWidth: 1.6 },
  };
}

function routingNodeHeight(pathCount: number): number {
  return ROUTING_PATH_PADDING * 2 + Math.max(0, pathCount - 1) * ROUTING_PATH_GAP;
}

function routingNodeWidth(role: ProjectRoutingRole | undefined): number {
  return role === 'panner' ? ROUTING_PANNER_WIDTH : UNIT_NODE_COMPACT_WIDTH;
}

function unitNodeDimensions(
  paramCount: number,
  routingPathCount = 0,
  routingRole?: ProjectRoutingRole,
): { width: number; height: number } {
  if (routingPathCount > 0) {
    return { width: routingNodeWidth(routingRole), height: routingNodeHeight(routingPathCount) };
  }
  const rows = Math.ceil(paramCount / KNOBS_PER_ROW);
  return {
    width: paramCount >= KNOBS_PER_ROW ? UNIT_NODE_WIDE_WIDTH : UNIT_NODE_COMPACT_WIDTH,
    height: rows === 0
      ? UNIT_NODE_EMPTY_HEIGHT
      : UNIT_NODE_FIRST_KNOB_ROW_HEIGHT + (rows - 1) * UNIT_NODE_EXTRA_KNOB_ROW_HEIGHT,
  };
}

function routeLaneCandidate(
  graph: dagre.graphlib.Graph,
  route: ProjectRoute,
  index: number,
  side: 'source' | 'target',
): number | undefined {
  const source = endpointNodeId(route.from);
  const target = endpointNodeId(route.to);
  const points = graph.edge({ v: source, w: target, name: routeEdgeId(index, source, target) })?.points ?? [];
  if (points.length === 0) return undefined;
  const point = side === 'source' ? points[1] ?? points[0] : points.at(-2) ?? points.at(-1);
  if (!point || !Number.isFinite(point.y)) return undefined;
  return point.y;
}

function centeredPathLanes(center: number, count: number, gap = ROUTING_PATH_GAP): number[] {
  return Array.from({ length: count }, (_, index) => center + (index - (count - 1) / 2) * gap);
}

function resolvePathLanes(candidates: Array<number | undefined>, center: number): number[] {
  if (candidates.length < 2 || candidates.some(candidate => candidate === undefined)) {
    return centeredPathLanes(center, candidates.length);
  }

  const lanes = candidates as number[];
  const minimumGap = ROUTING_PATH_GAP * 0.75;
  const ordered = lanes.every((lane, index) => index === 0 || lane - lanes[index - 1] >= minimumGap);
  const candidateCenter = (lanes[0] + lanes.at(-1)!) / 2;
  if (!ordered || Math.abs(candidateCenter - center) > 8) {
    return centeredPathLanes(center, lanes.length);
  }
  return lanes;
}

function routingNodesForProject(project: ProjectInspect, ports: ProjectPortCatalog): RoutingNode[] {
  return project.nodes.flatMap(instance => {
    const unitPorts = ports[instance.unit];
    return unitPorts?.routing ? [{ id: `unit-${instance.id}`, instance, ports: unitPorts }] : [];
  });
}

function routingLanes(
  project: ProjectInspect,
  graph: dagre.graphlib.Graph,
  routingNodes: RoutingNode[],
): Map<string, number[]> {
  const nodesBySection = new Map<string, RoutingNode[]>();
  for (const node of routingNodes) {
    const section = node.instance.routing?.section;
    if (!section) continue;
    const current = nodesBySection.get(section) ?? [];
    current.push(node);
    nodesBySection.set(section, current);
  }

  const lanesByNode = new Map<string, number[]>();
  for (const node of routingNodes) {
    const contract = node.ports.routing!;
    const center = graph.node(node.id).y;
    const sectionNodes = node.instance.routing?.section
      ? nodesBySection.get(node.instance.routing.section) ?? []
      : [];
    const peer = sectionNodes.find(candidate => candidate.ports.routing?.role !== contract.role);
    const candidates = contract.paths.map(path => {
      const routeIndex = project.routes.findIndex(route => contract.role === 'panner'
        ? route.from === `${node.instance.id}.${path.port}`
        : route.to === `${node.instance.id}.${path.port}`);
      if (routeIndex < 0) return undefined;
      return routeLaneCandidate(
        graph,
        project.routes[routeIndex],
        routeIndex,
        contract.role === 'panner' ? 'source' : 'target',
      );
    });
    const resolved = resolvePathLanes(candidates, center);

    if (peer && peer.ports.routing?.paths.length === resolved.length) {
      const directPaths = contract.paths.every((path, index) => {
        const peerPath = peer.ports.routing?.paths[index];
        if (!peerPath) return false;
        return project.routes.some(route => contract.role === 'panner'
          ? route.from === `${node.instance.id}.${path.port}` && route.to === `${peer.instance.id}.${peerPath.port}`
          : route.from === `${peer.instance.id}.${peerPath.port}` && route.to === `${node.instance.id}.${path.port}`);
      });
      if (directPaths) {
        const sharedCenter = (center + graph.node(peer.id).y) / 2;
        lanesByNode.set(node.id, centeredPathLanes(sharedCenter, resolved.length));
        continue;
      }
    }
    lanesByNode.set(node.id, resolved);
  }
  return lanesByNode;
}

function requiredRoutingHeight(center: number, lanes: number[], minimumHeight: number): number {
  const furthestLane = lanes.reduce((distance, lane) => Math.max(distance, Math.abs(lane - center)), 0);
  return Math.max(minimumHeight, Math.ceil((furthestLane + ROUTING_PATH_PADDING) * 2));
}

function createRoutingLayout(
  node: RoutingNode,
  graph: dagre.graphlib.Graph,
  lanes: number[],
): ProjectRoutingNodeLayout {
  const contract = node.ports.routing!;
  const layout = graph.node(node.id);
  const top = layout.y - layout.height / 2;
  const pathTops = Object.fromEntries(contract.paths.map((path, index) => [path.port, lanes[index] - top]));
  const centerTop = layout.y - top;
  return {
    height: layout.height,
    inputTops: Object.fromEntries(node.ports.inputs.map(port => [
      port,
      contract.role === 'mixer' ? pathTops[port] ?? centerTop : centerTop,
    ])),
    outputTops: Object.fromEntries(node.ports.outputs.map(port => [
      port,
      contract.role === 'panner' ? pathTops[port] ?? centerTop : centerTop,
    ])),
    controlTops: Object.fromEntries(contract.paths.map((path, index) => [path.levelParam, lanes[index] - top])),
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
    const unitPorts = ports[instance.unit];
    const routingPathCount = unitPorts?.routing?.paths.length ?? 0;
    const dimensions = unitNodeDimensions(instance.params.length, routingPathCount, unitPorts?.routing?.role);

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
        ports: unitPorts,
        visualLayout: {
          ...dimensions,
          railTop: dimensions.height / 2,
        },
      },
    };

    nodes.push(node);
    graph.setNode(node.id, dimensions);
  });

  project.routes.forEach((route, index) => {
    const source = endpointNodeId(route.from);
    const target = endpointNodeId(route.to);
    graph.setEdge(source, target, {}, routeEdgeId(index, source, target));
  });

  const routingNodes = routingNodesForProject(project, ports);
  let lanesByNode = new Map<string, number[]>();
  let layoutChanged = false;
  // Nested sections can widen an outer section's lane span. Grow helpers and rerun Dagre until that span is covered.
  for (let iteration = 0; iteration < routingNodes.length + 1; iteration += 1) {
    dagre.layout(graph);
    lanesByNode = routingLanes(project, graph, routingNodes);
    layoutChanged = false;
    for (const routingNode of routingNodes) {
      const layout = graph.node(routingNode.id);
      const minimumHeight = routingNodeHeight(routingNode.ports.routing!.paths.length);
      const height = requiredRoutingHeight(layout.y, lanesByNode.get(routingNode.id) ?? [], minimumHeight);
      if (height <= layout.height) continue;
      graph.setNode(routingNode.id, { width: layout.width, height });
      layoutChanged = true;
    }
    if (!layoutChanged) break;
  }
  if (layoutChanged) dagre.layout(graph);
  lanesByNode = routingLanes(project, graph, routingNodes);

  const routingLayouts = new Map(routingNodes.map(node => [
    node.id,
    createRoutingLayout(node, graph, lanesByNode.get(node.id) ?? centeredPathLanes(
      graph.node(node.id).y,
      node.ports.routing!.paths.length,
    )),
  ]));

  for (const node of nodes) {
    const position = graph.node(node.id);
    const routingLayout = routingLayouts.get(node.id);
    const dimensions = node.data.kind === 'system'
      ? node.data.visualLayout
      : routingLayout
        ? { width: routingNodeWidth(node.data.ports?.routing?.role), height: routingLayout.height }
        : node.data.visualLayout;
    node.position = {
      x: position.x - dimensions.width / 2,
      y: position.y - dimensions.height / 2,
    };
    if (node.data.kind === 'unit' && routingLayout) {
      node.data.routingLayout = routingLayout;
      node.data.visualLayout = {
        width: dimensions.width,
        height: dimensions.height,
        railTop: dimensions.height / 2,
      };
    }
  }

  const geometries = new Map(nodes.map(node => {
    const position = graph.node(node.id);
    const routingLayout = node.data.kind === 'unit' ? node.data.routingLayout : undefined;
    const dimensions = node.data.visualLayout;
    const top = position.y - dimensions.height / 2;
    const inputPorts = node.data.kind === 'unit' && node.data.ports?.inputs.length
      ? node.data.ports.inputs
      : [node.id === 'system-output' ? 'output' : 'input'];
    const outputPorts = node.data.kind === 'unit' && node.data.ports?.outputs.length
      ? node.data.ports.outputs
      : [node.id === 'system-input' ? 'input' : 'output'];
    return [node.id, {
      left: position.x - dimensions.width / 2,
      right: position.x + dimensions.width / 2,
      top,
      bottom: top + dimensions.height,
      inputYs: routingLayout
        ? Object.fromEntries(Object.entries(routingLayout.inputTops).map(([port, offset]) => [port, top + offset]))
        : Object.fromEntries(inputPorts.map(port => [port, top + node.data.visualLayout.railTop])),
      outputYs: routingLayout
        ? Object.fromEntries(Object.entries(routingLayout.outputTops).map(([port, offset]) => [port, top + offset]))
        : Object.fromEntries(outputPorts.map(port => [port, top + node.data.visualLayout.railTop])),
    } satisfies NodeGeometry] as const;
  }));

  const edges = project.routes.map((route, index) => createRouteEdge(route, index, graph, geometries));
  return { nodes, edges };
}
