import { useMemo, useState } from 'react';
import { markPerfSpan } from '../lib/perfTelemetry';
import { GraphContextMenu, GraphMenuButton, type ContextMenuPoint } from './GraphContextMenu';
import { UNIT_DRAG_TYPE } from './ProjectSidebar';

export type EffectLibraryItem = {
  id: string;
  title: string;
  category: string;
  description: string;
  scope: 'built-in' | 'personal';
  recordId?: string;
  placementError?: string;
};

type Props = {
  items: EffectLibraryItem[];
  onAdd: (item: EffectLibraryItem) => void;
  onAddParallel: (item: EffectLibraryItem) => void;
  onDeletePersonal: (recordId: string) => void;
  onEditContract: (item: EffectLibraryItem) => void;
};

const categoryColor: Record<string, string> = {
  dynamics: 'mint',
  modulation: 'violet',
  drive: 'coral',
  amp: 'amber',
  delay: 'blue',
  reverb: 'cyan',
};

export function SimpleLibraryPanel({ items, onAdd, onAddParallel, onDeletePersonal, onEditContract }: Props) {
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('All');
  const [draggingItemKey, setDraggingItemKey] = useState<string | null>(null);
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
        <p>Click + to append, or drag an effect onto the Pipeline or a rail.</p>
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
            className={`effect-library-card${draggingItemKey === `${item.scope}-${item.id}` ? ' effect-library-card--dragging' : ''}`}
            data-testid={`effect-library-item-${item.scope}-${item.id}`}
            draggable={!item.placementError}
            key={`${item.scope}-${item.id}`}
            onContextMenu={event => {
              event.preventDefault();
              setMenu({ item, x: event.clientX, y: event.clientY });
            }}
            tabIndex={0}
            onKeyDown={event => {
              if (event.key !== 'ContextMenu' && !(event.shiftKey && event.key === 'F10')) return;
              event.preventDefault();
              const bounds = event.currentTarget.getBoundingClientRect();
              setMenu({ item, x: bounds.left + 24, y: bounds.top + 24 });
            }}
            onDragEnd={() => setDraggingItemKey(null)}
            onDragStart={event => {
              if (event.target instanceof Element && event.target.closest('.effect-library-card__actions')) {
                event.preventDefault();
                return;
              }
              markPerfSpan('ui.dragStart.projectUnit', () => {
                event.dataTransfer.setData(UNIT_DRAG_TYPE, item.id);
                event.dataTransfer.setData('text/plain', item.title);
                event.dataTransfer.effectAllowed = 'copy';
                setDraggingItemKey(`${item.scope}-${item.id}`);
              }, { unit: item.id });
            }}
            title={item.placementError ?? 'Drag onto the Pipeline or a rail'}
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
            onEditContract(menu.item);
            setMenu(null);
          }}>Edit Contract</GraphMenuButton>
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
