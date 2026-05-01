import type { Node, Edge } from '@xyflow/react';
import type { UnitConfig, Stage } from '../types';
import { ATOM_MAP, CATEGORY_COLORS } from '../atoms/atomCatalog';

const COLUMN_WIDTH = 400;
const ROW_HEIGHT = 300;
const COLS = 2;

export type StageNodeData = {
  stage: Stage;
  signals: string[];
  pipelineIndex: number;
};

export function buildGraph(unit: UnitConfig): { nodes: Node[]; edges: Edge[] } {
  const nodes: Node[] = [];
  const edges: Edge[] = [];

  // Signal output map: signal name → { nodeId, portId }
  const signalSource = new Map<string, { nodeId: string; portId: string }>();

  unit.pipeline.forEach((stage, idx) => {
    const col = idx % COLS;
    const row = Math.floor(idx / COLS);
    const atomDef = ATOM_MAP.get(stage.fn);
    const category = atomDef?.category ?? 'filter';
    const color = CATEGORY_COLORS[category] ?? '#6b7280';

    const nodeId = `stage-${stage.id}`;

    nodes.push({
      id: nodeId,
      type: 'atomNode',
      position: { x: col * COLUMN_WIDTH + 40, y: row * ROW_HEIGHT + 40 },
      data: { stage, signals: unit.signals, pipelineIndex: idx, color } as StageNodeData & { color: string },
    });

    // Register output signals produced by this node
    for (const kv of stage.out) {
      signalSource.set(kv.value, { nodeId, portId: `out-${kv.key}` });
    }
  });

  // Create edges for each input binding that matches a known signal source
  unit.pipeline.forEach((stage) => {
    const nodeId = `stage-${stage.id}`;
    for (const kv of stage.in) {
      const src = signalSource.get(kv.value);
      if (src) {
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

  return { nodes, edges };
}
