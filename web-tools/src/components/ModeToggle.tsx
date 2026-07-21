import type { StudioMode } from '../lib/projectPackage';

type Props = {
  mode: StudioMode;
  onChange: (mode: StudioMode) => void;
  compact?: boolean;
};

export function ModeToggle({ mode, onChange, compact = false }: Props) {
  return (
    <div className={`mode-toggle ${compact ? 'mode-toggle--compact' : ''}`} aria-label="Editor mode">
      <button
        aria-pressed={mode === 'simple'}
        className={mode === 'simple' ? 'active' : ''}
        onClick={() => onChange('simple')}
        type="button"
      >
        Simple
      </button>
      <button
        aria-pressed={mode === 'pro'}
        className={mode === 'pro' ? 'active' : ''}
        onClick={() => onChange('pro')}
        type="button"
      >
        Pro
      </button>
    </div>
  );
}
