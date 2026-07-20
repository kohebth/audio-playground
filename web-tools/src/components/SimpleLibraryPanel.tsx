import { useMemo, useState } from 'react';

export type EffectLibraryItem = {
  id: string;
  title: string;
  category: string;
  description: string;
  scope: 'built-in' | 'personal';
  recordId?: string;
};

type Props = {
  items: EffectLibraryItem[];
  onAdd: (item: EffectLibraryItem) => void;
  onAddParallel: (item: EffectLibraryItem) => void;
  onDeletePersonal: (recordId: string) => void;
};

const categoryColor: Record<string, string> = {
  dynamics: 'mint',
  modulation: 'violet',
  drive: 'coral',
  amp: 'amber',
  delay: 'blue',
  reverb: 'cyan',
};

export function SimpleLibraryPanel({ items, onAdd, onAddParallel, onDeletePersonal }: Props) {
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('All');
  const categories = useMemo(() => ['All', ...new Set(items.map(item => item.category))], [items]);
  const filtered = useMemo(() => items.filter(item => (
    (category === 'All' || item.category === category)
    && `${item.title} ${item.description}`.toLowerCase().includes(query.toLowerCase())
  )), [category, items, query]);

  return (
    <aside className="simple-library" data-tour="library">
      <header>
        <div><span>Effect library</span><strong>{items.length}</strong></div>
        <p>Tap an effect to add it at the end of your board.</p>
      </header>
      <label className="simple-library__search">
        <span aria-hidden="true">⌕</span>
        <input onChange={event => setQuery(event.target.value)} placeholder="Find an effect" value={query} />
      </label>
      <div className="simple-library__categories" aria-label="Effect categories">
        {categories.map(item => (
          <button className={category === item ? 'active' : ''} key={item} onClick={() => setCategory(item)} type="button">
            {item}
          </button>
        ))}
      </div>
      <div className="simple-library__list">
        {filtered.map(item => (
          <article className="effect-library-card" key={`${item.scope}-${item.id}`}>
            <i className={`effect-library-card__icon effect-library-card__icon--${categoryColor[item.category] ?? 'blue'}`}>
              <span />
            </i>
            <span>
              <strong>{item.title}{item.scope === 'personal' ? <b>Yours</b> : null}</strong>
              <small>{item.description}</small>
            </span>
            <div className="effect-library-card__actions">
              <button aria-label={`Add ${item.title}`} onClick={() => onAdd(item)} title="Add in series" type="button">+</button>
              <button aria-label={`Add ${item.title} in parallel`} onClick={() => onAddParallel(item)} title="Add as a wet/dry parallel path" type="button">∥</button>
              {item.scope === 'personal' && item.recordId ? (
                <button aria-label={`Delete ${item.title} from personal library`} onClick={() => onDeletePersonal(item.recordId!)} title="Remove from personal library" type="button">×</button>
              ) : null}
            </div>
          </article>
        ))}
        {filtered.length === 0 ? <p className="simple-library__empty">No effects match that search.</p> : null}
      </div>
    </aside>
  );
}
