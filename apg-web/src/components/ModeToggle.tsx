import type { StudioMode } from '../lib/projectPackage';

type Props = {
  mode: StudioMode;
  onChange: (mode: StudioMode) => void;
  compact?: boolean;
};

export function ModeToggle({ mode, onChange, compact = false }: Props) {
  return (
    <div className={`mode-toggle ${compact ? 'mode-toggle--compact' : ''}`} aria-label="Editor view">
      <button
        aria-pressed={mode === 'simple'}
        className={mode === 'simple' ? 'active' : ''}
        data-testid="view-effect-pipeline"
        onClick={() => onChange('simple')}
        type="button"
      >
        Pipeline
      </button>
      <button
        aria-pressed={mode === 'pro'}
        className={mode === 'pro' ? 'active' : ''}
        data-testid="view-effect-contract"
        onClick={() => onChange('pro')}
        type="button"
      >
        Contract
      </button>
    </div>
  );
}
