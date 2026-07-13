import { useMemo, useState, type CSSProperties } from 'react';

import type { AtomCatalog, AtomCatalogAtom, AtomCatalogField, UnitInspect } from '../lib/backendSamples';

type Props = {
  unit: UnitInspect;
  catalog: AtomCatalog;
  manifest: Record<string, string>;
};

const CATEGORY_COLORS: Record<string, string> = {
  amplitude: '#10b981',
  delay: '#8b5cf6',
  detect: '#06b6d4',
  filter: '#3b82f6',
  freq: '#6366f1',
  generation: '#f59e0b',
  interpolation: '#84cc16',
  mix: '#ec4899',
  modulation: '#f97316',
  nonlinear: '#ef4444',
  src: '#14b8a6',
};

function categoryColor(category: string): string {
  return CATEGORY_COLORS[category] ?? '#6b7280';
}

function fieldLabel(fields: AtomCatalogField[]): string {
  return fields.length > 0 ? fields.map(field => `${field.name}:${field.type}`).join(', ') : 'none';
}

function profileLabel(atom: AtomCatalogAtom): string {
  const enabled = Object.entries(atom.profiles)
    .filter(([, supported]) => supported)
    .map(([profile]) => profile);
  return enabled.length > 0 ? enabled.join(', ') : 'none';
}

export function AtomCatalogPanel({ unit, catalog, manifest }: Props) {
  const unitAtomNames = unit.graph.nodes.map(node => node.atom);
  const [selectedAtomName, setSelectedAtomName] = useState(unitAtomNames[0] ?? catalog.atoms[0]?.name ?? '');
  const selectedAtom = catalog.atoms.find(atom => atom.name === selectedAtomName) ?? catalog.atoms[0];
  const categoryCounts = useMemo(
    () =>
      catalog.atoms.reduce<Record<string, number>>((counts, atom) => {
        counts[atom.category] = (counts[atom.category] ?? 0) + 1;
        return counts;
      }, {}),
    [catalog.atoms],
  );

  return (
    <details className="inspector-block" open>
      <summary className="inspector-block__label">Atom Palette</summary>
      <div className="atom-palette__summary">
        <strong>{catalog.atoms.length} backend atoms</strong>
        <span>{manifest.schema} / {manifest.bytes} bytes</span>
      </div>

      <div className="atom-palette__categories">
        {Object.entries(categoryCounts).map(([category, count]) => (
          <span
            key={category}
            className="atom-palette__category"
            style={{ '--category-color': categoryColor(category) } as CSSProperties}
          >
            {category} {count}
          </span>
        ))}
      </div>

      <div className="atom-palette__list" aria-label="Atom palette">
        {catalog.atoms.map(atom => (
          <button
            key={atom.name}
            className={`atom-palette__item ${atom.name === selectedAtomName ? 'atom-palette__item--active' : ''}`}
            onClick={() => setSelectedAtomName(atom.name)}
            style={{ '--category-color': categoryColor(atom.category) } as CSSProperties}
            type="button"
          >
            <span>{atom.name}</span>
            <strong>{atom.stateful ? 'stateful' : atom.category}</strong>
          </button>
        ))}
      </div>

      {selectedAtom ? (
        <div className="atom-detail">
          <div className="atom-detail__header">
            <span style={{ background: categoryColor(selectedAtom.category) }} />
            <strong>{selectedAtom.name}</strong>
          </div>
          <div className="atom-detail__grid">
            <div>
              <span>Inputs</span>
              <strong>{fieldLabel(selectedAtom.inputs)}</strong>
            </div>
            <div>
              <span>Outputs</span>
              <strong>{fieldLabel(selectedAtom.outputs)}</strong>
            </div>
            <div>
              <span>Config</span>
              <strong>{fieldLabel(selectedAtom.config)}</strong>
            </div>
            <div>
              <span>Profiles</span>
              <strong>{profileLabel(selectedAtom)}</strong>
            </div>
          </div>
        </div>
      ) : null}

      <div className="unit-inspect">
        <div className="unit-inspect__header">
          <span>Unit Inspect</span>
          <strong>{unit.name}</strong>
        </div>
        <p>{unit.meta?.description ?? unit.file}</p>

        <div className="unit-inspect__meta">
          <span>{unit.params.length} params</span>
          <span>{unit.ports.inputs.length} inputs</span>
          <span>{unit.ports.outputs.length} outputs</span>
          <span>{unit.graph.nodes.length} nodes</span>
        </div>

        <div className="unit-inspect__nodes">
          {unit.graph.nodes.map(node => (
            <button key={node.id} onClick={() => setSelectedAtomName(node.atom)} type="button">
              <span>{node.id}</span>
              <strong>{node.atom}</strong>
            </button>
          ))}
        </div>
      </div>
    </details>
  );
}
