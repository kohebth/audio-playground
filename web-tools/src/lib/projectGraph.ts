import type { Edge, Node } from '@xyflow/react';
import dagre from 'dagre';
import type { ProjectInspect, ProjectInstance, ProjectRoute, ProjectUnit } from './backendSamples';
import type { ProjectGraphDraft } from './projectV2Graph';

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
    };

export type ProjectParamControl = {
  key: string;
  label: string;
  type: string;
  min?: string;
  max?: string;
  unit?: string;
};

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

function endpointPort(endpoint: string): string {
  return endpoint.split('.').slice(1).join('.') || endpoint;
}

function createSystemNode(id: string, label: string, detail: string, color: string): Node<ProjectNodeData> {
  return {
    id,
    type: 'projectNode',
    position: { x: 0, y: 0 },
    data: { kind: 'system', id, label, detail, color },
  };
}

function createRouteEdge(route: ProjectRoute, index: number): Edge {
  const source = endpointNodeId(route.from);
  const target = endpointNodeId(route.to);

  return {
    id: `route-${index}-${source}-${target}`,
    source,
    target,
    sourceHandle: 'out',
    targetHandle: 'in',
    label: `${endpointPort(route.from)} -> ${endpointPort(route.to)}`,
    style: { stroke: '#64748b', strokeWidth: 1.6 },
    labelStyle: { fill: '#e2e8f0', fontSize: 11, fontWeight: 500 },
    labelBgStyle: { fill: '#111827', fillOpacity: 0.9 },
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
  draft?: ProjectGraphDraft,
): { nodes: Node<ProjectNodeData>[]; edges: Edge[] } {
  const unitsById = new Map(project.units.map(unit => [unit.id, unit]));
  const positionByInstance = new Map((draft?.nodes ?? []).map(node => [node.id, node.ui?.position]));
  const graph = new dagre.graphlib.Graph();
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
      position: positionByInstance.get(instance.id) ?? { x: 0, y: 0 },
      data: {
        kind: 'unit',
        instance,
        unit,
        index,
        color: UNIT_COLORS[index % UNIT_COLORS.length],
      },
    };

    nodes.push(node);
    graph.setNode(node.id, unitNodeDimensions(instance.params.length));
  });

  const edges = project.routes.map(createRouteEdge);

  for (const edge of edges) {
    graph.setEdge(edge.source, edge.target);
  }

  dagre.layout(graph);

  for (const node of nodes) {
    if (node.data.kind === 'unit' && positionByInstance.get(node.data.instance.id)) continue;
    const position = graph.node(node.id);
    const dimensions = node.data.kind === 'system'
      ? { width: SYSTEM_NODE_WIDTH, height: SYSTEM_NODE_HEIGHT }
      : unitNodeDimensions(node.data.instance.params.length);
    node.position = {
      x: position.x - dimensions.width / 2,
      y: position.y - dimensions.height / 2,
    };
  }

  return { nodes, edges };
}
