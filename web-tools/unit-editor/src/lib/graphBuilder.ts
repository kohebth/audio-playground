import type { Node, Edge } from '@xyflow/react';
import dagre from 'dagre';
import type { UnitConfig, Stage } from '../types';
import { ATOM_MAP, CATEGORY_COLORS } from '../atoms/atomCatalog';

const NODE_WIDTH = 240;
const NODE_HEIGHT = 160;

export type StageNodeData = {
  stage: Stage;
  signals: string[];
  pipelineIndex: number;
};

export function buildGraph(unit: UnitConfig): { nodes: Node[]; edges: Edge[] } {
  const nodes: Node[] = [];
  const edges: Edge[] = [];

  // 1. Create a Dagre graph to handle the layout
  const g = new dagre.graphlib.Graph();
  g.setGraph({ rankdir: 'TB', nodesep: 50, ranksep: 100 });
  g.setDefaultEdgeLabel(() => ({}));

  // Signal output map: signal name → { nodeId, portId }
  const signalSource = new Map<string, { nodeId: string; portId: string }>();

  // 2. Define nodes in Dagre
  unit.pipeline.forEach((stage, idx) => {
    const atomDef = ATOM_MAP.get(stage.fn);
    const category = atomDef?.category ?? 'filter';
    const color = CATEGORY_COLORS[category] ?? '#6b7280';
    const nodeId = `stage-${stage.id}`;

    // Add to Dagre
    g.setNode(nodeId, { width: NODE_WIDTH, height: NODE_HEIGHT });

    nodes.push({
      id: nodeId,
      type: 'atomNode',
      position: { x: 0, y: 0 }, // Will be set by Dagre
      data: { stage, signals: unit.signals, pipelineIndex: idx, color } as StageNodeData & { color: string },
    });

    // Register output signals
    for (const kv of stage.out) {
      if (kv.value) {
        signalSource.set(kv.value, { nodeId, portId: `out-${kv.key}` });
      }
    }
  });

  // 3. Define edges in Dagre and React Flow
  unit.pipeline.forEach((stage) => {
    const nodeId = `stage-${stage.id}`;
    for (const kv of stage.in) {
      if (!kv.value) continue;
      
      const src = signalSource.get(kv.value);
      if (src) {
        // Add to Dagre for layout calculation
        g.setEdge(src.nodeId, nodeId);

        // Add to React Flow
        edges.push({
          id: `edge-${src.nodeId}-${nodeId}-${kv.key}`,
          source: src.nodeId,
          sourceHandle: src.portId,
          target: nodeId,
          targetHandle: `in-${kv.key}`,
          animated: false,
          style: { stroke: '#6b7280', strokeWidth: 1.5 },
        });
      }
    }
  });

  // 4. Calculate layout
  dagre.layout(g);

  // 5. Apply calculated positions to React Flow nodes
  nodes.forEach((node) => {
    const dagreNode = g.node(node.id);
    node.position = {
      x: dagreNode.x - NODE_WIDTH / 2,
      y: dagreNode.y - NODE_HEIGHT / 2,
    };
  });

  return { nodes, edges };
}
