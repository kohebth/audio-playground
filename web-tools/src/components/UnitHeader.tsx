import type { UnitConfig, ParamDef } from '../types';

type Props = { unit: UnitConfig; onChange: (unit: UnitConfig) => void };

export function UnitHeader({ unit, onChange }: Props) {
  const updateParam = (idx: number, patch: Partial<ParamDef>) => {
    const params = unit.params.map((p, i) => i === idx ? { ...p, ...patch } : p);
    onChange({ ...unit, params });
  };

  const addParam = () =>
    onChange({ ...unit, params: [...unit.params, { name: 'new_param', default: 0, range: [0, 1] }] });

  const removeParam = (idx: number) =>
    onChange({ ...unit, params: unit.params.filter((_, i) => i !== idx) });

  return (
    <header className="unit-header">
      <div className="unit-header__meta">
        <input
          className="unit-header__name"
          value={unit.name}
          onChange={e => onChange({ ...unit, name: e.target.value })}
          placeholder="unit name"
        />
        <input
          className="unit-header__version"
          value={unit.version}
          onChange={e => onChange({ ...unit, version: e.target.value })}
          placeholder="version"
        />
      </div>

      <div className="unit-header__params">
        <div className="unit-header__params-title">
          <span>Parameters</span>
          <button className="unit-header__btn-add" onClick={addParam}>+</button>
        </div>
        {unit.params.map((p, i) => (
          <div key={i} className="unit-header__param-row">
            <input
              className="unit-header__param-name"
              value={p.name}
              onChange={e => updateParam(i, { name: e.target.value })}
              placeholder="name"
            />
            <input
              className="unit-header__param-default"
              type="number"
              value={p.default}
              onChange={e => updateParam(i, { default: parseFloat(e.target.value) || 0 })}
              placeholder="default"
            />
            {p.range && (
              <>
                <input
                  className="unit-header__param-range"
                  type="number"
                  value={p.range[0]}
                  onChange={e => updateParam(i, { range: [parseFloat(e.target.value), p.range![1]] })}
                  placeholder="min"
                />
                <span className="unit-header__param-sep">→</span>
                <input
                  className="unit-header__param-range"
                  type="number"
                  value={p.range[1]}
                  onChange={e => updateParam(i, { range: [p.range![0], parseFloat(e.target.value)] })}
                  placeholder="max"
                />
              </>
            )}
            <button
              className="unit-header__btn-remove"
              onClick={() => removeParam(i)}
            >×</button>
          </div>
        ))}
      </div>
    </header>
  );
}
