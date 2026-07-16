import { useMemo, useState, type CSSProperties } from 'react';

import type { AtomCatalog, AtomCatalogAtom, AtomCatalogField, UnitInspect } from '../lib/backendSamples';
import { markPerfSpan } from '../lib/perfTelemetry';

type Props = {
  unit: UnitInspect;
  catalog: AtomCatalog;
  manifest: Record<string, string>;
  onAddAtom?: (atomName: string) => void;
};

export const ATOM_DRAG_TYPE = 'application/x-apg-atom';

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

export function AtomCatalogPanel({ unit, catalog, manifest, onAddAtom }: Props) {
  const unitAtomNames = unit.graph.nodes.map(node => node.atom);
  const initialPaletteAtom = catalog.atoms.find(atom => atom.visibility === 'public') ?? catalog.atoms[0];
  const [selectedAtomName, setSelectedAtomName] = useState(unitAtomNames[0] ?? initialPaletteAtom?.name ?? '');
  const [filter, setFilter] = useState('');
  const [showAdvanced, setShowAdvanced] = useState(false);
  const paletteAtoms = useMemo(
    () => catalog.atoms.filter(atom => atom.visibility === 'public' || (showAdvanced && atom.visibility === 'advanced')),
    [catalog.atoms, showAdvanced],
  );
  const selectedAtom = catalog.atoms.find(atom => atom.name === selectedAtomName) ?? initialPaletteAtom;
  const filteredAtoms = useMemo(() => {
    const query = filter.trim().toLowerCase();
    if (!query) return paletteAtoms;
    return paletteAtoms.filter(atom => atom.name.toLowerCase().includes(query) || atom.category.toLowerCase().includes(query));
  }, [filter, paletteAtoms]);
  const categoryCounts = useMemo(
    () =>
      paletteAtoms.reduce<Record<string, number>>((counts, atom) => {
        counts[atom.category] = (counts[atom.category] ?? 0) + 1;
        return counts;
      }, {}),
    [paletteAtoms],
  );

  return (
    <details className="inspector-block" open>
      <summary className="inspector-block__label">Atom Palette</summary>
      <div className="atom-palette__summary">
        <strong>{paletteAtoms.length} available / {catalog.atoms.length} backend</strong>
        <span>{manifest.schema} / {manifest.bytes} bytes</span>
      </div>

      <label className="atom-palette__options">
        <input
          checked={showAdvanced}
          data-testid="atom-palette-show-advanced"
          onChange={event => setShowAdvanced(event.target.checked)}
          type="checkbox"
        />
        <span>Advanced</span>
      </label>

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

      <input
        aria-label="Filter atom palette"
        className="atom-palette__filter"
        data-testid="atom-palette-filter"
        onChange={event => setFilter(event.target.value)}
        placeholder="Filter atoms"
        type="search"
        value={filter}
      />

      <div className="atom-palette__list" aria-label="Atom palette">
        {filteredAtoms.map(atom => (
          <button
            key={atom.name}
            className={`atom-palette__item ${atom.name === selectedAtomName ? 'atom-palette__item--active' : ''}`}
            draggable
            data-testid={`atom-palette-item-${atom.name}`}
            onClick={() => setSelectedAtomName(atom.name)}
            onDragStart={event => {
              markPerfSpan('ui.dragStart.atomPalette', () => {
                event.dataTransfer.setData(ATOM_DRAG_TYPE, atom.name);
                event.dataTransfer.effectAllowed = 'copy';
              }, { atom: atom.name });
            }}
            style={{ '--category-color': categoryColor(atom.category) } as CSSProperties}
            type="button"
          >
            <span>{atom.name}</span>
            <strong>{atom.stateful ? 'stateful' : atom.category}</strong>
          </button>
        ))}
        {filteredAtoms.length === 0 ? <span className="atom-palette__empty">No matching atoms</span> : null}
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
          {onAddAtom && paletteAtoms.some(atom => atom.name === selectedAtom.name) ? (
            <button className="atom-detail__add" onClick={() => onAddAtom(selectedAtom.name)} type="button">
              Add atom
            </button>
          ) : null}
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
