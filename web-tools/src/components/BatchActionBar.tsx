type Props = {
  count: number;
  liveReady: boolean;
  onBypass: (enabled: boolean) => void;
  onClear: () => void;
  onRemove: () => void;
};

export function BatchActionBar({ count, liveReady, onBypass, onClear, onRemove }: Props) {
  if (count < 2) return null;
  return (
    <div className="batch-action-bar" data-testid="batch-action-bar">
      <strong>{count} effects selected</strong>
      <button disabled={!liveReady} onClick={() => onBypass(false)} type="button">Turn on</button>
      <button disabled={!liveReady} onClick={() => onBypass(true)} type="button">Bypass</button>
      <button className="danger" onClick={onRemove} type="button">Remove</button>
      <button aria-label="Clear selection" onClick={onClear} type="button">×</button>
    </div>
  );
}
