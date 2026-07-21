export type GraphvizLayoutNode = {
  id: string;
  x: number;
  y: number;
  width: number;
  height: number;
};

export type GraphvizLayoutEdge = {
  id: string;
  source: string;
  target: string;
};

export type GraphvizLayoutRequest = {
  requestId: number;
  mode: 'layout' | 'route';
  nodes: GraphvizLayoutNode[];
  edges: GraphvizLayoutEdge[];
};

export type GraphvizLayoutResult = {
  requestId: number;
  positions: Record<string, { x: number; y: number }>;
  routes: Record<string, Array<{ x: number; y: number }>>;
  error?: string;
};
