/// <reference lib="webworker" />

import { instance } from '@viz-js/viz';
import type { GraphvizLayoutRequest, GraphvizLayoutResult } from '../lib/graphvizLayout';

type JsonNode = { name?: string; pos?: string };
type JsonEdge = { id?: string; _draw_?: Array<{ op?: string; points?: Array<[number, number]> }> };
type JsonGraph = { objects?: JsonNode[]; edges?: JsonEdge[] };

const vizPromise = instance();

function parsePosition(value: string | undefined): { x: number; y: number } | null {
  if (!value) return null;
  const [x, y] = value.split(',').map(Number);
  return Number.isFinite(x) && Number.isFinite(y) ? { x, y } : null;
}

self.addEventListener('message', event => {
  const request = event.data as GraphvizLayoutRequest;
  void vizPromise.then(viz => {
    const fixed = request.mode === 'route';
    const graph = {
      name: 'AtomChain',
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
      nodes: request.nodes.map(node => ({
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
      const source = request.nodes.find(candidate => candidate.id === node.name);
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
    const result: GraphvizLayoutResult = { requestId: request.requestId, positions, routes };
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
