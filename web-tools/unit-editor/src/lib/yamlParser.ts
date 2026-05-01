import yaml from 'js-yaml';
import type { UnitConfig, Stage, KV, ParamDef } from '../types';

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
    for (const s of doc.pipeline) {
      if (s && typeof s === 'object') pipeline.push(parseStage(s as Record<string, unknown>));
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
