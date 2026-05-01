import type { UnitConfig, Stage, KV } from '../types';
import { ATOM_CATALOG, ATOM_MAP } from '../atoms/atomCatalog';

type Props = {
  unit: UnitConfig;
  selectedId: string | null;
  onChange: (unit: UnitConfig) => void;
};

function updateStage(unit: UnitConfig, id: string, patch: Partial<Stage>): UnitConfig {
  return { ...unit, pipeline: unit.pipeline.map(s => s.id === id ? { ...s, ...patch } : s) };
}

export function Inspector({ unit, selectedId, onChange }: Props) {
  const stage = unit.pipeline.find(s => s.id === selectedId);

  if (!stage) {
    return (
      <aside className="inspector inspector--empty">
        <p>Select a node to inspect it</p>
      </aside>
    );
  }

  const atomDef = ATOM_MAP.get(stage.fn);

  const setField = (field: keyof Stage, kvs: KV[]) =>
    onChange(updateStage(unit, stage.id, { [field]: kvs }));

  const updateKV = (field: keyof Stage, idx: number, key: string, value: string) => {
    const arr = [...(stage[field] as KV[])];
    arr[idx] = { key, value };
    setField(field, arr);
  };

  const addKV = (field: keyof Stage) =>
    setField(field, [...(stage[field] as KV[]), { key: '', value: '' }]);

  const removeKV = (field: keyof Stage, idx: number) => {
    const arr = [...(stage[field] as KV[])];
    arr.splice(idx, 1);
    setField(field, arr);
  };

  return (
    <aside className="inspector">
      <div className="inspector__section">
        <label className="inspector__label">ID</label>
        <input
          className="inspector__input"
          value={stage.id}
          onChange={e => onChange(updateStage(unit, stage.id, { id: e.target.value }))}
        />
      </div>

      <div className="inspector__section">
        <label className="inspector__label">Function (fn)</label>
        <select
          className="inspector__select"
          value={stage.fn}
          onChange={e => onChange(updateStage(unit, stage.id, { fn: e.target.value }))}
        >
          {ATOM_CATALOG.map(a => (
            <option key={a.name} value={a.name}>{a.name}</option>
          ))}
        </select>
      </div>

      <KVSection
        label="Inputs (in)"
        kvs={stage.in}
        signals={unit.signals}
        onUpdate={(i, k, v) => updateKV('in', i, k, v)}
        onAdd={() => addKV('in')}
        onRemove={(i) => removeKV('in', i)}
        suggestKeys={atomDef?.ins ?? []}
      />

      <KVSection
        label="Outputs (out)"
        kvs={stage.out}
        signals={unit.signals}
        onUpdate={(i, k, v) => updateKV('out', i, k, v)}
        onAdd={() => addKV('out')}
        onRemove={(i) => removeKV('out', i)}
        suggestKeys={atomDef?.outs ?? []}
      />

      <KVSection
        label="Config"
        kvs={stage.config}
        signals={[]}
        onUpdate={(i, k, v) => updateKV('config', i, k, v)}
        onAdd={() => addKV('config')}
        onRemove={(i) => removeKV('config', i)}
        suggestKeys={atomDef?.config.map(f => f.name) ?? []}
        freeValue
      />
    </aside>
  );
}

type KVSectionProps = {
  label: string;
  kvs: KV[];
  signals: string[];
  suggestKeys: string[];
  freeValue?: boolean;
  onUpdate: (i: number, key: string, value: string) => void;
  onAdd: () => void;
  onRemove: (i: number) => void;
};

function KVSection({ label, kvs, signals, suggestKeys, freeValue, onUpdate, onAdd, onRemove }: KVSectionProps) {
  return (
    <div className="inspector__section">
      <div className="inspector__section-header">
        <label className="inspector__label">{label}</label>
        <button className="inspector__btn-add" onClick={onAdd}>+</button>
      </div>
      {kvs.map((kv, i) => (
        <div key={i} className="inspector__kv-row">
          {suggestKeys.length > 0 ? (
            <select
              className="inspector__select inspector__select--sm"
              value={kv.key}
              onChange={e => onUpdate(i, e.target.value, kv.value)}
            >
              {suggestKeys.map(k => <option key={k} value={k}>{k}</option>)}
              <option value={kv.key}>{kv.key || '(custom)'}</option>
            </select>
          ) : (
            <input
              className="inspector__input inspector__input--sm"
              placeholder="key"
              value={kv.key}
              onChange={e => onUpdate(i, e.target.value, kv.value)}
            />
          )}

          {freeValue ? (
            <input
              className="inspector__input inspector__input--sm"
              placeholder="value"
              value={kv.value}
              onChange={e => onUpdate(i, kv.key, e.target.value)}
            />
          ) : (
            <select
              className="inspector__select inspector__select--sm"
              value={kv.value}
              onChange={e => onUpdate(i, kv.key, e.target.value)}
            >
              <option value="">— pick signal —</option>
              {signals.map(s => <option key={s} value={s}>{s}</option>)}
            </select>
          )}
          <button className="inspector__btn-remove" onClick={() => onRemove(i)}>×</button>
        </div>
      ))}
    </div>
  );
}
