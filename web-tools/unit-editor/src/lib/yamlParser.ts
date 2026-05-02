import yaml from 'js-yaml';
import type { UnitConfig, Stage, KV, ParamDef } from '../types';
import { ATOM_MAP } from '../atoms/atomCatalog';

function kvFromMap(obj: Record<string, unknown> | undefined): KV[] {
  if (!obj || typeof obj !== 'object') return [];
  return Object.entries(obj).map(([key, value]) => ({ key, value: String(value) }));
}

function parseStage(raw: Record<string, unknown>): Stage {
  const inn = raw.in;
  const out = raw.out;
  const config = raw.config;

  return {
    id: String(raw.id ?? ''),
    fn: String(raw.fn ?? ''),
    in: kvFromMap(typeof inn === 'object' && inn !== null ? inn as Record<string, unknown> : undefined),
    out: kvFromMap(typeof out === 'object' && out !== null ? out as Record<string, unknown> : undefined),
    config: kvFromMap(typeof config === 'object' && config !== null ? config as Record<string, unknown> : undefined),
  };
}

function validateStage(stage: Stage, index: number): void {
  const atom = ATOM_MAP.get(stage.fn);
  if (!atom) {
    const names = Array.from(ATOM_MAP.keys()).join(', ');
    throw new Error(`Stage ${index + 1}: Unknown atom "${stage.fn}". Valid atoms: ${names}`);
  }

  for (const port of stage.in) {
    if (!atom.ins.includes(port.key)) {
      throw new Error(`Stage ${index + 1} (${stage.fn}): Invalid input port "${port.key}". Valid inputs: ${atom.ins.join(', ')}`);
    }
  }

  for (const port of stage.out) {
    if (!atom.outs.includes(port.key)) {
      throw new Error(`Stage ${index + 1} (${stage.fn}): Invalid output port "${port.key}". Valid outputs: ${atom.outs.join(', ')}`);
    }
  }

  for (const cfg of stage.config) {
    const field = atom.config.find(f => f.name === cfg.key);
    if (!field) {
      throw new Error(`Stage ${index + 1} (${stage.fn}): Unknown config "${cfg.key}". Valid fields: ${atom.config.map(f => f.name).join(', ')}`);
    }
  }
}

function parseParam(name: string, raw: unknown): ParamDef {
  if (!raw || typeof raw !== 'object') return { name, default: 0 };
  const obj = raw as Record<string, unknown>;
  const range = Array.isArray(obj.range) ? [Number(obj.range[0]), Number(obj.range[1])] as [number, number] : undefined;
  return { name, default: Number(obj.default ?? 0), range };
}

export function parseYaml(src: string): UnitConfig {
  const doc = yaml.load(src) as Record<string, unknown>;

  const params: ParamDef[] = [];
  if (doc.params && typeof doc.params === 'object') {
    for (const [k, v] of Object.entries(doc.params as Record<string, unknown>)) {
      params.push(parseParam(k, v));
    }
  }

  const signals: string[] = [];
  if (Array.isArray(doc.signals)) {
    for (const s of doc.signals) {
      if (typeof s === 'string') signals.push(s);
    }
  }

  const pipeline: Stage[] = [];
  if (Array.isArray(doc.pipeline)) {
    for (let i = 0; i < doc.pipeline.length; i++) {
      const raw = doc.pipeline[i];
      if (raw && typeof raw === 'object') {
        const stage = parseStage(raw as Record<string, unknown>);
        validateStage(stage, i);
        pipeline.push(stage);
      }
    }
  }

  return {
    name: String(doc.name ?? 'unnamed'),
    version: String(doc.version ?? '1.0.0'),
    params,
    signals,
    pipeline,
  };
}
