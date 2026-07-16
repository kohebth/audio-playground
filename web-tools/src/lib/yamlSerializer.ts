import yaml from 'js-yaml';
import type { UnitConfig, Stage } from '../types';

function kvToMap(kvs: { key: string; value: string }[]): Record<string, unknown> | null {
  if (kvs.length === 0) return null;
  return Object.fromEntries(kvs.map(({ key, value }) => {
    const num = Number(value);
    return [key, (!isNaN(num) && value.trim() !== '') ? num : value];
  }));
}

function serializeStage(s: Stage): Record<string, unknown> {
  const obj: Record<string, unknown> = { id: s.id, fn: s.fn };
  const outMap = kvToMap(s.out);
  if (outMap) obj.out = outMap;
  const inMap = kvToMap(s.in);
  if (inMap) obj.in = inMap;
  const configMap = kvToMap(s.config);
  if (configMap) obj.config = configMap;
  return obj;
}

export function serializeYaml(unit: UnitConfig): string {
  const params: Record<string, unknown> = {};
  for (const p of unit.params) {
    const entry: Record<string, unknown> = { default: p.default };
    if (p.range) entry.range = p.range;
    params[p.name] = entry;
  }

  const doc: Record<string, unknown> = {
    name: unit.name,
    version: unit.version,
    params,
    signals: unit.signals,
    pipeline: unit.pipeline.map(serializeStage),
  };

  return yaml.dump(doc, { lineWidth: 120, quotingType: '"', noRefs: true });
}
