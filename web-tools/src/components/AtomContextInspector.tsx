import type { AtomCatalog } from '../lib/backendSamples';
import type { UnitGraphNode } from '../lib/unitV2Graph';

type Props = {
  atom: UnitGraphNode | null;
  catalog: AtomCatalog;
  clipboardReady: boolean;
  error: string | null;
  onCopy: (id?: string) => void;
  onCut: (id?: string) => void;
  onChange: (atom: UnitGraphNode, originalId?: string) => void;
  onPaste: () => void;
  onRemove: (id?: string) => void;
};

export function AtomContextInspector({ atom, catalog, clipboardReady, error, onChange, onCopy, onCut, onPaste, onRemove }: Props) {
  if (!atom) {
    return (
      <aside className="simple-inspector atom-context-inspector" data-testid="atom-context-inspector">
        <span className="simple-inspector__eyebrow">Atom Inspector</span>
        <h2>Choose an atom</h2>
        <p>Select an atom in the graph to inspect its bindings and configuration.</p>
        {error ? <p className="inspector-error" role="alert">{error}</p> : null}
      </aside>
    );
  }
  const definition = catalog.atoms.find(candidate => candidate.name === atom.atom);
  return (
    <aside className="simple-inspector atom-context-inspector" data-testid="atom-context-inspector">
      <span className="simple-inspector__eyebrow">Selected atom</span>
      <h2>{atom.id}</h2>
      <p className="simple-inspector__instance">{atom.atom}</p>
      <div className="simple-inspector__summary">
        <div><strong>{Object.keys(atom.in).length}</strong><span>inputs</span></div>
        <div><strong>{Object.keys(atom.out).length}</strong><span>outputs</span></div>
      </div>
      <section className="atom-context-inspector__fields">
        <strong>Bindings</strong>
        {(['in', 'out'] as const).flatMap(section => Object.entries(atom[section]).map(([key, value]) => (
          <label key={`${section}-${key}`}>
            <span>{section}.{key}</span>
            <input
              aria-label={`${atom.id} ${section} ${key}`}
              onChange={event => onChange({
                ...atom,
                [section]: { ...atom[section], [key]: event.target.value },
              })}
              value={value}
            />
          </label>
        )))}
        <strong>Configuration</strong>
        {Object.entries(atom.config).map(([key, value]) => (
          <label key={key}>
            <span>{key}</span>
            <input
              aria-label={`${atom.id} config ${key}`}
              onChange={event => onChange({ ...atom, config: { ...atom.config, [key]: event.target.value } })}
              value={value}
            />
          </label>
        ))}
        {Object.keys(atom.config).length === 0 ? <p>No atom-specific configuration.</p> : null}
        {definition ? <small>{definition.category}{definition.stateful ? ' · stateful' : ''}</small> : null}
      </section>
      <div className="simple-inspector__actions">
        <button onClick={() => onCopy(atom.id)} type="button">Copy</button>
        <button onClick={() => onCut(atom.id)} type="button">Cut</button>
        <button disabled={!clipboardReady} onClick={onPaste} type="button">Paste</button>
        <button className="danger" onClick={() => onRemove(atom.id)} type="button">Remove</button>
      </div>
      {error ? <p className="inspector-error" role="alert">{error}</p> : null}
    </aside>
  );
}
