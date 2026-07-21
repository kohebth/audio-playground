import { useEffect, useRef, type ReactNode } from 'react';

export type ContextMenuPoint = { x: number; y: number };

type MenuProps = {
  children: ReactNode;
  label: string;
  onClose: () => void;
  point: ContextMenuPoint;
};

export function GraphContextMenu({ children, label, onClose, point }: MenuProps) {
  const menuRef = useRef<HTMLDivElement>(null);
  const left = typeof window === 'undefined' ? point.x : Math.min(point.x, window.innerWidth - 304);
  const top = typeof window === 'undefined' ? point.y : Math.min(point.y, window.innerHeight - 420);

  useEffect(() => {
    const menu = menuRef.current;
    menu?.querySelector<HTMLElement>('[role="menuitem"]:not(:disabled)')?.focus();
    const dismiss = (event: PointerEvent) => {
      if (!menu?.contains(event.target as globalThis.Node)) onClose();
    };
    const escape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    document.addEventListener('pointerdown', dismiss, true);
    window.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('pointerdown', dismiss, true);
      window.removeEventListener('keydown', escape);
    };
  }, [onClose]);

  return (
    <div
      aria-label={label}
      className="graph-context-menu nodrag nopan"
      data-testid="graph-context-menu"
      onContextMenu={event => event.preventDefault()}
      onKeyDown={event => {
        if (!['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(event.key)) return;
        const items = [...event.currentTarget.querySelectorAll<HTMLElement>('[role="menuitem"]:not(:disabled)')];
        if (items.length === 0) return;
        event.preventDefault();
        const current = items.indexOf(document.activeElement as HTMLElement);
        const next = event.key === 'Home'
          ? 0
          : event.key === 'End'
            ? items.length - 1
            : event.key === 'ArrowDown'
              ? (current + 1 + items.length) % items.length
              : (current - 1 + items.length) % items.length;
        items[next].focus();
      }}
      ref={menuRef}
      role="menu"
      style={{ left: Math.max(8, left), top: Math.max(8, top) }}
    >
      {children}
    </div>
  );
}

type MenuButtonProps = {
  children: ReactNode;
  danger?: boolean;
  disabled?: boolean;
  icon: string;
  onClick: () => void;
  title?: string;
};

export function GraphMenuButton({ children, danger = false, disabled = false, icon, onClick, title }: MenuButtonProps) {
  return (
    <button
      className={`graph-context-menu__item${danger ? ' graph-context-menu__item--danger' : ''}`}
      disabled={disabled}
      onClick={onClick}
      role="menuitem"
      title={title}
      type="button"
    >
      <i className={`fa-solid ${icon}`} aria-hidden="true" />
      <span>{children}</span>
    </button>
  );
}
