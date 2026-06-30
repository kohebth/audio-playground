import { useMemo, useState, type CSSProperties } from 'react';

import { ATOM_CATALOG, CATEGORY_COLORS, type AtomDef } from '../atoms/atomCatalog';
import type { UnitInspect } from '../lib/backendSamples';

type Props = {
  unit: UnitInspect;
  manifest: Record<string, string>;
};

function formatFields(fields: string[]): string {
  return fields.length > 0 ? fields.join(', ') : 'none';
}

function configLabel(config: AtomDef['config']): string {
  return config.length > 0 ? config.map(field => `${field.name}:${field.type}`).join(', ') : 'none';
}

export function AtomCatalogPanel({ unit, manifest }: Props) {
  const unitAtomNames = unit.graph.nodes.map(node => node.atom);
  const [selectedAtomName, setSelectedAtomName] = useState(unitAtomNames[0] ?? ATOM_CATALOG[0]?.name ?? '');
  const selectedAtom = ATOM_CATALOG.find(atom => atom.name === selectedAtomName) ?? ATOM_CATALOG[0];
  const categoryCounts = useMemo(
    () =>
      ATOM_CATALOG.reduce<Record<string, number>>((counts, atom) => {
        counts[atom.category] = (counts[atom.category] ?? 0) + 1;
        return counts;
      }, {}),
    [],
  );

  return (
    <section className="inspector-block">
      <div className="inspector-block__label">Atom Palette</div>
      <div className="atom-palette__summary">
        <strong>{ATOM_CATALOG.length} local atoms</strong>
        <span>{manifest.schema} / {manifest.bytes} bytes</span>
      </div>

      <div className="atom-palette__categories">
        {Object.entries(categoryCounts).map(([category, count]) => (
          <span
            key={category}
            className="atom-palette__category"
            style={{ '--category-color': CATEGORY_COLORS[category as AtomDef['category']] } as CSSProperties}
          >
            {category} {count}
          </span>
        ))}
      </div>

      <div className="atom-palette__list" aria-label="Atom palette">
        {ATOM_CATALOG.map(atom => (
          <button
            key={atom.name}
            className={`atom-palette__item ${atom.name === selectedAtom.name ? 'atom-palette__item--active' : ''}`}
            onClick={() => setSelectedAtomName(atom.name)}
            style={{ '--category-color': CATEGORY_COLORS[atom.category] } as CSSProperties}
            type="button"
          >
            <span>{atom.name}</span>
            <strong>{atom.category}</strong>
          </button>
        ))}
      </div>

      <div className="atom-detail">
        <div className="atom-detail__header">
          <span style={{ background: CATEGORY_COLORS[selectedAtom.category] }} />
          <strong>{selectedAtom.name}</strong>
        </div>
        <div className="atom-detail__grid">
          <div>
            <span>Inputs</span>
            <strong>{formatFields(selectedAtom.ins)}</strong>
          </div>
          <div>
            <span>Outputs</span>
            <strong>{formatFields(selectedAtom.outs)}</strong>
          </div>
          <div>
            <span>Config</span>
            <strong>{configLabel(selectedAtom.config)}</strong>
          </div>
        </div>
      </div>

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
    </section>
  );
}
