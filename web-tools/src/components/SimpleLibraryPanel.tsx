import { useMemo, useState } from 'react';
import { GraphContextMenu, GraphMenuButton, type ContextMenuPoint } from './GraphContextMenu';

export const EFFECT_LIBRARY_DRAG_TYPE = 'application/x-apg-effect-library';

export type EffectLibraryItem = {
  id: string;
  title: string;
  category: string;
  description: string;
  scope: 'built-in' | 'personal';
  recordId?: string;
  placementError?: string;
};

export type EffectLibraryDragPayload = Pick<EffectLibraryItem, 'id' | 'scope' | 'recordId' | 'title'>;

type Props = {
  items: EffectLibraryItem[];
  onAdd: (item: EffectLibraryItem) => void;
  onAddParallel: (item: EffectLibraryItem) => void;
  onDeletePersonal: (recordId: string) => void;
  onEditDefinition: (item: EffectLibraryItem) => void;
};

const categoryColor: Record<string, string> = {
  dynamics: 'mint',
  modulation: 'violet',
  drive: 'coral',
  amp: 'amber',
  delay: 'blue',
  reverb: 'cyan',
};

export function SimpleLibraryPanel({ items, onAdd, onAddParallel, onDeletePersonal, onEditDefinition }: Props) {
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('All');
  const categories = useMemo(() => ['All', ...new Set(items.map(item => item.category))], [items]);
  const filtered = useMemo(() => items.filter(item => (
    (category === 'All' || item.category === category)
    && `${item.title} ${item.description}`.toLowerCase().includes(query.toLowerCase())
  )), [category, items, query]);
  const [menu, setMenu] = useState<(ContextMenuPoint & { item: EffectLibraryItem }) | null>(null);

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
          <article
            className="effect-library-card"
            draggable={!item.placementError}
            key={`${item.scope}-${item.id}`}
            onContextMenu={event => {
              event.preventDefault();
              setMenu({ item, x: event.clientX, y: event.clientY });
            }}
            onDragStart={event => {
              if (item.placementError) {
                event.preventDefault();
                return;
              }
              const payload: EffectLibraryDragPayload = {
                id: item.id,
                scope: item.scope,
                recordId: item.recordId,
                title: item.title,
              };
              event.dataTransfer.setData(EFFECT_LIBRARY_DRAG_TYPE, JSON.stringify(payload));
              event.dataTransfer.effectAllowed = 'copy';
            }}
          >
            <i className={`effect-library-card__icon effect-library-card__icon--${categoryColor[item.category] ?? 'blue'}`}>
              <span />
            </i>
            <span>
              <strong>{item.title}{item.scope === 'personal' ? <b>Yours</b> : null}</strong>
              <small>{item.description}</small>
            </span>
            <div className="effect-library-card__actions">
              <button
                aria-label={`Add ${item.title}`}
                disabled={Boolean(item.placementError)}
                onClick={() => onAdd(item)}
                title={item.placementError ?? 'Add in series'}
                type="button"
              >+</button>
              <button
                aria-label={`Add ${item.title} in parallel`}
                disabled={Boolean(item.placementError)}
                onClick={() => onAddParallel(item)}
                title={item.placementError ?? 'Add as a wet/dry parallel path'}
                type="button"
              >∥</button>
              {item.scope === 'personal' && item.recordId ? (
                <button aria-label={`Delete ${item.title} from personal library`} onClick={() => onDeletePersonal(item.recordId!)} title="Remove from personal library" type="button">×</button>
              ) : null}
            </div>
          </article>
        ))}
        {filtered.length === 0 ? <p className="simple-library__empty">No effects match that search.</p> : null}
      </div>
      {menu ? (
        <GraphContextMenu label={`${menu.item.title} library actions`} onClose={() => setMenu(null)} point={menu}>
          <div className="graph-context-menu__title">
            <strong>{menu.item.title}</strong>
            <span>{menu.item.scope === 'personal' ? 'Personal effect' : 'Built-in effect'}</span>
          </div>
          <GraphMenuButton icon="fa-diagram-project" onClick={() => {
            onEditDefinition(menu.item);
            setMenu(null);
          }}>Edit Atom Chain</GraphMenuButton>
          {menu.item.scope === 'personal' && menu.item.recordId ? (
            <GraphMenuButton danger icon="fa-trash" onClick={() => {
              onDeletePersonal(menu.item.recordId!);
              setMenu(null);
            }}>Remove from library</GraphMenuButton>
          ) : null}
        </GraphContextMenu>
      ) : null}
    </aside>
  );
}
