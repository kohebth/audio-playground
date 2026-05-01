import { useRef } from 'react';
import type { UnitConfig, Stage } from '../types';
import { ATOM_CATALOG, CATEGORY_COLORS } from '../atoms/atomCatalog';

type Props = {
  unit: UnitConfig;
  selectedId: string | null;
  onSelectId: (id: string) => void;
  onChange: (unit: UnitConfig) => void;
};

export function PipelineSidebar({ unit, selectedId, onSelectId, onChange }: Props) {
  const dragIdx = useRef<number | null>(null);

  const handleDragStart = (i: number) => { dragIdx.current = i; };

  const handleDrop = (targetIdx: number) => {
    if (dragIdx.current === null || dragIdx.current === targetIdx) return;
    const pipeline = [...unit.pipeline];
    const [moved] = pipeline.splice(dragIdx.current, 1);
    pipeline.splice(targetIdx, 0, moved);
    onChange({ ...unit, pipeline });
    dragIdx.current = null;
  };

  const addStep = () => {
    const newStep: Stage = {
      id: `step_${unit.pipeline.length + 1}`,
      fn: 'amplitude_add',
      in: [],
      out: [],
      config: [],
    };
    onChange({ ...unit, pipeline: [...unit.pipeline, newStep] });
  };

  const removeStep = (id: string) => {
    onChange({ ...unit, pipeline: unit.pipeline.filter(s => s.id !== id) });
  };

  return (
    <aside className="sidebar">
      <div className="sidebar__header">
        <span className="sidebar__title">Pipeline</span>
        <span className="sidebar__count">{unit.pipeline.length} steps</span>
        <button className="sidebar__btn-add" onClick={addStep} title="Add step">+</button>
      </div>

      <ul className="sidebar__list">
        {unit.pipeline.map((step, i) => {
          const cat = ATOM_CATALOG.find(a => a.name === step.fn)?.category ?? 'filter';
          const accent = CATEGORY_COLORS[cat] ?? '#6b7280';
          return (
            <li
              key={step.id}
              className={`sidebar__item ${step.id === selectedId ? 'sidebar__item--active' : ''}`}
              draggable
              onDragStart={() => handleDragStart(i)}
              onDragOver={e => e.preventDefault()}
              onDrop={() => handleDrop(i)}
              onClick={() => onSelectId(step.id)}
            >
              <span className="sidebar__drag-handle">⠿</span>
              <span className="sidebar__step-num">{i + 1}</span>
              <span className="sidebar__accent-dot" style={{ background: accent }} />
              <div className="sidebar__step-info">
                <span className="sidebar__step-id">{step.id}</span>
                <span className="sidebar__step-fn">{step.fn}</span>
              </div>
              <button
                className="sidebar__btn-remove"
                onClick={e => { e.stopPropagation(); removeStep(step.id); }}
                title="Remove"
              >×</button>
            </li>
          );
        })}
      </ul>
    </aside>
  );
}
