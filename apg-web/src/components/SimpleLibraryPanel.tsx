import { useMemo, useRef, useState, type CSSProperties, type PointerEvent as ReactPointerEvent } from 'react';
import { UNIT_DRAG_TYPE, type ProjectLibraryPointerDrag } from '../lib/graphDragTypes';
import { markPerfSpan } from '../lib/perfTelemetry';
import { GraphContextMenu, GraphMenuButton, type ContextMenuPoint } from './GraphContextMenu';

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
  onDeletePersonal: (recordId: string) => void;
  onEditContract: (item: EffectLibraryItem) => void;
  onPointerDrag: (drag: ProjectLibraryPointerDrag) => void;
  purpose: 'pipeline' | 'contract';
};

const categoryColor: Record<string, string> = {
  dynamics: 'mint',
  modulation: 'violet',
  drive: 'coral',
  amp: 'amber',
  delay: 'blue',
  reverb: 'cyan',
};

const categoryBadgeColor: Record<string, string> = {
  all: '#34d399',
  dynamics: '#9be6bf',
  modulation: '#c4a7ff',
  drive: '#ff9b75',
  amp: '#f1c56e',
  delay: '#a9c7ff',
  reverb: '#83e6d1',
};

type PointerDragSession = {
  pointerId: number;
  item: EffectLibraryItem;
  itemKey: string;
  startX: number;
  startY: number;
  active: boolean;
};

export function SimpleLibraryPanel({
  items,
  onDeletePersonal,
  onEditContract,
  onPointerDrag,
  purpose,
}: Props) {
  const [query, setQuery] = useState('');
  const [category, setCategory] = useState('All');
  const [draggingItemKey, setDraggingItemKey] = useState<string | null>(null);
  const pointerDragRef = useRef<PointerDragSession | null>(null);
  const picksContract = purpose === 'contract';
  const categories = useMemo(() => ['All', ...new Set(items.map(item => item.category))], [items]);
  const filtered = useMemo(() => items.filter(item => (
    (category === 'All' || item.category === category)
    && `${item.title} ${item.description}`.toLowerCase().includes(query.toLowerCase())
  )), [category, items, query]);
  const [menu, setMenu] = useState<(ContextMenuPoint & { item: EffectLibraryItem }) | null>(null);

  const startPointerDrag = (event: ReactPointerEvent<HTMLElement>, item: EffectLibraryItem) => {
    if (event.pointerType === 'mouse' || picksContract || item.placementError) return;
    if (event.target instanceof Element && event.target.closest('button')) return;
    const itemKey = `${item.scope}-${item.id}`;
    pointerDragRef.current = {
      pointerId: event.pointerId,
      item,
      itemKey,
      startX: event.clientX,
      startY: event.clientY,
      active: false,
    };
    try {
      event.currentTarget.setPointerCapture(event.pointerId);
    } catch {
      // Synthetic test pointers and older touch browsers may not support pointer capture.
    }
  };

  const movePointerDrag = (event: ReactPointerEvent<HTMLElement>) => {
    const session = pointerDragRef.current;
    if (!session || session.pointerId !== event.pointerId) return;
    if (!session.active) {
      const distance = Math.hypot(event.clientX - session.startX, event.clientY - session.startY);
      if (distance < 8) return;
      session.active = true;
      setDraggingItemKey(session.itemKey);
      markPerfSpan('ui.dragStart.projectUnit', () => undefined, { unit: session.item.id, input: 'touch' });
    }
    event.preventDefault();
    onPointerDrag({
      phase: 'dragging',
      unitId: session.item.id,
      title: session.item.title,
      clientX: event.clientX,
      clientY: event.clientY,
    });
  };

  const finishPointerDrag = (
    event: ReactPointerEvent<HTMLElement>,
    phase: ProjectLibraryPointerDrag['phase'],
  ) => {
    const session = pointerDragRef.current;
    if (!session || session.pointerId !== event.pointerId) return;
    pointerDragRef.current = null;
    setDraggingItemKey(null);
    if (session.active) {
      onPointerDrag({
        phase,
        unitId: session.item.id,
        title: session.item.title,
        clientX: event.clientX,
        clientY: event.clientY,
      });
    }
    try {
      event.currentTarget.releasePointerCapture(event.pointerId);
    } catch {
      // Pointer capture may already have been released by the browser.
    }
  };

  return (
    <aside className="simple-library" data-tour="library">
      <header>
        <div><span>{picksContract ? 'Unit library' : 'Effect library'}</span><strong>{items.length}</strong></div>
        <p>{picksContract
          ? 'Click a unit to edit its Contract.'
          : 'Drag an effect onto the Pipeline or a rail to place it inline.'}</p>
      </header>
      <label className="simple-library__search">
        <span aria-hidden="true">⌕</span>
        <input
          onChange={event => setQuery(event.target.value)}
          placeholder={picksContract ? 'Find a unit' : 'Find an effect'}
          value={query}
        />
      </label>
      <div className="simple-library__categories" aria-label={picksContract ? 'Unit categories' : 'Effect categories'}>
        {categories.map(item => (
          <button
            className={category === item ? 'active' : ''}
            key={item}
            onClick={() => setCategory(item)}
            style={{ '--library-category-color': categoryBadgeColor[item.toLowerCase()] ?? '#a9c7ff' } as CSSProperties}
            type="button"
          >
            {item}
          </button>
        ))}
      </div>
      <div className="simple-library__list">
        {filtered.map(item => (
          <article
            aria-disabled={picksContract && item.placementError ? true : undefined}
            aria-label={picksContract ? `Edit ${item.title} Contract` : undefined}
            className={`effect-library-card${picksContract ? ' effect-library-card--contract' : ' effect-library-card--touch-enabled'}${draggingItemKey === `${item.scope}-${item.id}` ? ' effect-library-card--dragging' : ''}`}
            data-testid={`effect-library-item-${item.scope}-${item.id}`}
            draggable={!picksContract && !item.placementError}
            key={`${item.scope}-${item.id}`}
            onClick={event => {
              if (!picksContract || item.placementError) return;
              if (event.target instanceof Element && event.target.closest('button')) return;
              onEditContract(item);
            }}
            onContextMenu={event => {
              event.preventDefault();
              setMenu({ item, x: event.clientX, y: event.clientY });
            }}
            role={picksContract ? 'button' : undefined}
            tabIndex={0}
            onKeyDown={event => {
              if (picksContract && !item.placementError && (event.key === 'Enter' || event.key === ' ')) {
                event.preventDefault();
                onEditContract(item);
                return;
              }
              if (event.key !== 'ContextMenu' && !(event.shiftKey && event.key === 'F10')) return;
              event.preventDefault();
              const bounds = event.currentTarget.getBoundingClientRect();
              setMenu({ item, x: bounds.left + 24, y: bounds.top + 24 });
            }}
            onDragEnd={() => setDraggingItemKey(null)}
            onDragStart={event => {
              if (event.target instanceof Element && event.target.closest('button')) {
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
            onPointerCancel={event => finishPointerDrag(event, 'cancel')}
            onPointerDown={event => startPointerDrag(event, item)}
            onPointerMove={movePointerDrag}
            onPointerUp={event => finishPointerDrag(event, 'drop')}
            title={item.placementError ?? (picksContract ? `Edit ${item.title} Contract` : 'Drag onto the Pipeline or a rail')}
          >
            <i className={`effect-library-card__icon effect-library-card__icon--${categoryColor[item.category] ?? 'blue'}`}>
              <span />
            </i>
            <span>
              <strong>{item.title}{item.scope === 'personal' ? <b>Yours</b> : null}</strong>
              <small>{item.description}</small>
            </span>
            {item.scope === 'personal' && item.recordId && !picksContract ? (
              <button
                aria-label={`Delete ${item.title} from personal library`}
                onClick={() => onDeletePersonal(item.recordId!)}
                title="Remove from personal library"
                type="button"
              >×</button>
            ) : null}
          </article>
        ))}
        {filtered.length === 0 ? (
          <p className="simple-library__empty">No {picksContract ? 'units' : 'effects'} match that search.</p>
        ) : null}
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
