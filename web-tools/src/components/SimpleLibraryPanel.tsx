import { useMemo, useState } from 'react';

export type EffectLibraryItem = {
  id: string;
  title: string;
  category: string;
  description: string;
};

type Props = {
  items: EffectLibraryItem[];
  onAdd: (item: EffectLibraryItem) => void;
};

const categoryColor: Record<string, string> = {
  dynamics: 'mint',
  modulation: 'violet',
  drive: 'coral',
  amp: 'amber',
  delay: 'blue',
  reverb: 'cyan',
};

export function SimpleLibraryPanel({ items, onAdd }: Props) {
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
          <button className="effect-library-card" key={item.id} onClick={() => onAdd(item)} type="button">
            <i className={`effect-library-card__icon effect-library-card__icon--${categoryColor[item.category] ?? 'blue'}`}>
              <span />
            </i>
            <span><strong>{item.title}</strong><small>{item.description}</small></span>
            <em>+</em>
          </button>
        ))}
        {filtered.length === 0 ? <p className="simple-library__empty">No effects match that search.</p> : null}
      </div>
    </aside>
  );
}
