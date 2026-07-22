/// <reference lib="webworker" />

import { instance } from '@viz-js/viz';
import type {
  GraphvizLayoutNode,
  GraphvizLayoutRequest,
  GraphvizLayoutResult,
} from '../lib/graphvizLayout';

type JsonNode = { name?: string; pos?: string };
type JsonEdge = { id?: string; _draw_?: Array<{ op?: string; points?: Array<[number, number]> }> };
type JsonGraph = { objects?: JsonNode[]; edges?: JsonEdge[] };

const vizPromise = instance();
type Viz = Awaited<typeof vizPromise>;
type Position = { x: number; y: number };
type LayoutPass = {
  positions: Record<string, Position>;
  routes: Record<string, Position[]>;
};

function parsePosition(value: string | undefined): { x: number; y: number } | null {
  if (!value) return null;
  const [x, y] = value.split(',').map(Number);
  return Number.isFinite(x) && Number.isFinite(y) ? { x, y } : null;
}

function renderGraph(
  viz: Viz,
  request: GraphvizLayoutRequest,
  nodes: GraphvizLayoutNode[],
  fixed: boolean,
): LayoutPass {
  const graph = {
    name: 'AtomGraph',
    directed: true,
    graphAttributes: {
      rankdir: 'LR',
      nodesep: '0.65',
      ranksep: '1.0',
      margin: '0.35',
      pad: '0.2',
      splines: 'ortho',
      outputorder: 'edgesfirst',
    },
    nodeAttributes: { shape: 'box', fixedsize: 'true' },
    nodes: nodes.map(node => ({
      name: node.id,
      attributes: {
        width: String(node.width / 72),
        height: String(node.height / 72),
        ...(fixed ? { pos: `${node.x + node.width / 2},${node.y + node.height / 2}!`, pin: 'true' } : {}),
      },
    })),
    edges: request.edges.map(edge => ({
      tail: edge.source,
      head: edge.target,
      attributes: { id: edge.id },
    })),
  };
  const output = viz.renderJSON(graph, { engine: fixed ? 'nop2' : 'dot', yInvert: true }) as JsonGraph;
  const positions = Object.fromEntries((output.objects ?? []).flatMap(node => {
    const source = nodes.find(candidate => candidate.id === node.name);
    const position = parsePosition(node.pos);
    return source && node.name && position ? [[node.name, {
      x: position.x - source.width / 2,
      y: position.y - source.height / 2,
    }]] : [];
  }));
  const routes = Object.fromEntries((output.edges ?? []).flatMap(edge => {
    const draw = edge._draw_?.find(command => command.op === 'b' && command.points);
    return edge.id && draw?.points ? [[edge.id, draw.points.map(([x, y]) => ({ x, y }))]] : [];
  }));
  return { positions, routes };
}

function nodesAtPositions(
  nodes: GraphvizLayoutNode[],
  positions: Record<string, Position>,
): GraphvizLayoutNode[] {
  return nodes.map(node => ({ ...node, ...(positions[node.id] ?? {}) }));
}

function alignBoundaryNodes(
  request: GraphvizLayoutRequest,
  positions: Record<string, Position>,
): GraphvizLayoutNode[] {
  const constraint = request.boundaryConstraint;
  if (!constraint) return nodesAtPositions(request.nodes, positions);
  const input = request.nodes.find(node => node.id === constraint.inputId);
  const output = request.nodes.find(node => node.id === constraint.outputId);
  if (!input || !output) return nodesAtPositions(request.nodes, positions);

  const positioned = nodesAtPositions(request.nodes, positions);
  const atoms = positioned.filter(node => node.id !== input.id && node.id !== output.id);
  if (atoms.length === 0) {
    const baseline = (
      (positions[input.id]?.y ?? input.y) + input.height / 2
      + (positions[output.id]?.y ?? output.y) + output.height / 2
    ) / 2;
    return positioned.map(node => (
      node.id === input.id || node.id === output.id
        ? { ...node, y: baseline - node.height / 2 }
        : node
    ));
  }

  const minLeft = Math.min(...atoms.map(node => node.x));
  const maxRight = Math.max(...atoms.map(node => node.x + node.width));
  const minTop = Math.min(...atoms.map(node => node.y));
  const maxBottom = Math.max(...atoms.map(node => node.y + node.height));
  const baseline = (minTop + maxBottom) / 2;
  return positioned.map(node => {
    if (node.id === input.id) {
      return {
        ...node,
        x: minLeft - constraint.gap - node.width,
        y: baseline - node.height / 2,
      };
    }
    if (node.id === output.id) {
      return {
        ...node,
        x: maxRight + constraint.gap,
        y: baseline - node.height / 2,
      };
    }
    return node;
  });
}

function segmentCrossesNode(start: Position, end: Position, node: GraphvizLayoutNode): boolean {
  const inset = 1;
  const left = node.x + inset;
  const right = node.x + node.width - inset;
  const top = node.y + inset;
  const bottom = node.y + node.height - inset;
  const segmentLeft = Math.min(start.x, end.x);
  const segmentRight = Math.max(start.x, end.x);
  const segmentTop = Math.min(start.y, end.y);
  const segmentBottom = Math.max(start.y, end.y);
  return segmentRight > left && segmentLeft < right && segmentBottom > top && segmentTop < bottom;
}

function orthogonalRoutePoints(planned: Position[]): Position[] {
  const graphvizPoints = planned.filter((point, index, points) => (
    index === 0 || point.x !== points[index - 1].x || point.y !== points[index - 1].y
  ));
  if (graphvizPoints.length < 2) return graphvizPoints;
  const points: Position[] = [graphvizPoints[0]];
  for (const hint of graphvizPoints.slice(1, -1)) {
    const previous = points.at(-1)!;
    if (previous.x !== hint.x && previous.y !== hint.y) {
      points.push({ x: hint.x, y: previous.y });
    }
    points.push(hint);
  }
  const target = graphvizPoints.at(-1)!;
  const previous = points.at(-1)!;
  if (previous.x !== target.x && previous.y !== target.y) {
    points.push({ x: previous.x, y: target.y });
  }
  points.push(target);
  return points;
}

function routeCrossesAnotherNode(
  points: Position[],
  sourceId: string,
  targetId: string,
  nodes: GraphvizLayoutNode[],
): boolean {
  const obstacles = nodes.filter(node => node.id !== sourceId && node.id !== targetId);
  const routed = orthogonalRoutePoints(points);
  return routed.slice(1).some((point, index) => (
    obstacles.some(node => segmentCrossesNode(routed[index], point, node))
  ));
}

function outerLaneRoute(
  index: number,
  source: GraphvizLayoutNode,
  target: GraphvizLayoutNode,
  nodes: GraphvizLayoutNode[],
  planned: Position[],
): Position[] {
  const forward = source.x + source.width / 2 <= target.x + target.width / 2;
  const sourcePoint = planned[0] ?? {
    x: forward ? source.x + source.width : source.x,
    y: source.y + source.height / 2,
  };
  const targetPoint = planned.at(-1) ?? {
    x: forward ? target.x : target.x + target.width,
    y: target.y + target.height / 2,
  };
  const sourceEscapeX = forward ? source.x + source.width + 24 : source.x - 24;
  const targetEscapeX = forward ? target.x - 24 : target.x + target.width + 24;
  const laneOffset = 48 + Math.floor(index / 2) * 18;
  const laneY = index % 2 === 0
    ? Math.min(...nodes.map(node => node.y)) - laneOffset
    : Math.max(...nodes.map(node => node.y + node.height)) + laneOffset;
  return [
    sourcePoint,
    { x: sourceEscapeX, y: sourcePoint.y },
    { x: sourceEscapeX, y: laneY },
    { x: targetEscapeX, y: laneY },
    { x: targetEscapeX, y: targetPoint.y },
    targetPoint,
  ];
}

function routesOutsideNodes(
  request: GraphvizLayoutRequest,
  nodes: GraphvizLayoutNode[],
  routes: Record<string, Position[]>,
): Record<string, Position[]> {
  return Object.fromEntries(request.edges.map((edge, index) => {
    const route = routes[edge.id] ?? [];
    const source = nodes.find(node => node.id === edge.source);
    const target = nodes.find(node => node.id === edge.target);
    if (!source || !target || route.length < 2) {
      return [edge.id, route];
    }
    const boundaryIds = request.boundaryConstraint
      ? new Set([request.boundaryConstraint.inputId, request.boundaryConstraint.outputId])
      : new Set<string>();
    const corridorLeft = Math.min(source.x + source.width, target.x + target.width);
    const corridorRight = Math.max(source.x, target.x);
    const boundarySkipsNode = (boundaryIds.has(source.id) || boundaryIds.has(target.id))
      && nodes.some(node => (
        node.id !== source.id
        && node.id !== target.id
        && node.x + node.width > corridorLeft
        && node.x < corridorRight
      ));
    if (!boundarySkipsNode && !routeCrossesAnotherNode(route, edge.source, edge.target, nodes)) {
      return [edge.id, route];
    }
    return [edge.id, outerLaneRoute(index, source, target, nodes, route)];
  }));
}

self.addEventListener('message', event => {
  const request = event.data as GraphvizLayoutRequest;
  void vizPromise.then(viz => {
    const initial = renderGraph(viz, request, request.nodes, request.mode === 'route');
    const pass = request.mode === 'layout' && request.boundaryConstraint
      ? renderGraph(viz, request, alignBoundaryNodes(request, initial.positions), true)
      : initial;
    const positionedNodes = nodesAtPositions(request.nodes, pass.positions);
    const routes = routesOutsideNodes(request, positionedNodes, pass.routes);
    const result: GraphvizLayoutResult = { requestId: request.requestId, positions: pass.positions, routes };
    self.postMessage(result);
  }).catch(error => {
    const result: GraphvizLayoutResult = {
      requestId: request.requestId,
      positions: {},
      routes: {},
      error: error instanceof Error ? error.message : 'Graphviz layout failed.',
    };
    self.postMessage(result);
  });
});
