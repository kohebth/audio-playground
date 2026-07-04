import {
  Background,
  Controls,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  type Edge,
  type Node,
  type OnEdgesChange,
  type OnNodesChange,
  type NodeTypes,
} from '@xyflow/react';

import { ProjectNode } from './ProjectNode';
import type { ProjectNodeData } from '../lib/projectGraph';

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
};

function routeIndexFromEdge(edge: Edge): number | null {
  const match = edge.id.match(/^route-(\d+)-/);
  return match ? Number(match[1]) : null;
}

export function ProjectCanvas({
  nodes,
  edges,
  selectedRouteIndex,
  onNodesChange,
  onEdgesChange,
  onSelectNode,
  onOpenContractGraph,
  onSelectRoute,
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
    <main className="canvas canvas--project">
      <div className="flow-shell">
        <ReactFlowProvider>
          <ReactFlow
            nodes={nodes}
            edges={displayedEdges}
            nodeTypes={nodeTypes}
            onNodesChange={onNodesChange}
            onEdgesChange={onEdgesChange}
            onNodeClick={(_, node) => onSelectNode(node.id)}
            onNodeDoubleClick={(_, node) => onOpenContractGraph(node.id)}
            onEdgeClick={(_, edge) => {
              const routeIndex = routeIndexFromEdge(edge);
              if (routeIndex !== null) onSelectRoute(routeIndex);
            }}
            fitView
            fitViewOptions={{ padding: 0.16 }}
            minZoom={0.35}
            maxZoom={1.5}
          >
            <Background color="#1e293b" gap={24} />
            <Controls />
            <MiniMap
              nodeColor={node => (node.data as ProjectNodeData).color}
              pannable
              zoomable
              style={{ background: '#111827' }}
            />
          </ReactFlow>
        </ReactFlowProvider>
      </div>
    </main>
  );
}
