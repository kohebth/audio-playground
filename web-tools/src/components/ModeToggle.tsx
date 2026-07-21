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
        aria-pressed={mode === 'effect-chain'}
        className={mode === 'effect-chain' ? 'active' : ''}
        onClick={() => onChange('effect-chain')}
        type="button"
      >
        Effect Chain
      </button>
      <button
        aria-pressed={mode === 'atom-chain'}
        className={mode === 'atom-chain' ? 'active' : ''}
        onClick={() => onChange('atom-chain')}
        type="button"
      >
        Atom Chain
      </button>
    </div>
  );
}
