import { type CSSProperties, type PointerEvent, useEffect, useRef, useState } from 'react';

type Props = {
  ariaLabel: string;
  value: string;
  min?: string;
  max?: string;
  unit?: string;
  label?: string;
  compact?: boolean;
  integer?: boolean;
  onChange: (next: string) => void;
};

const RANGE_FRACTION_PER_DRAG_PIXEL = 0.005;

function numberOrNull(value: string | undefined): number | null {
  if (value === undefined || value.trim() === '') return null;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}

function clampValue(value: number, min: number | null, max: number | null): number {
  return Math.min(max ?? value, Math.max(min ?? value, value));
}

function formatValue(value: number): string {
  return Number.isInteger(value) ? `${Math.round(value)}` : `${Number(value.toFixed(6))}`;
}

function percentForValue(value: string, minValue: string | undefined, maxValue: string | undefined): number {
  const current = numberOrNull(value) ?? 0;
  const min = numberOrNull(minValue) ?? 0;
  const max = numberOrNull(maxValue) ?? 1;
  if (max <= min) return 0;
  return clampValue(((current - min) / (max - min)) * 100, 0, 100);
}

export function ParamKnob({ ariaLabel, value, min, max, unit, label, compact = false, integer = false, onChange }: Props) {
  const [draft, setDraft] = useState(value);
  const minValue = numberOrNull(min);
  const maxValue = numberOrNull(max);
  const percent = percentForValue(value, min, max);
  const outOfRange = (() => {
    const parsed = numberOrNull(draft);
    if (parsed === null) return draft.trim() !== '';
    return (minValue !== null && parsed < minValue) || (maxValue !== null && parsed > maxValue);
  })();
  const dragState = useRef<{
    pointerId: number;
    lastY: number;
    value: number;
    integer: boolean;
  } | null>(null);

  useEffect(() => {
    setDraft(value);
  }, [value]);

  const startDrag = (event: PointerEvent<HTMLElement>) => {
    if (event.button !== 0 || event.detail > 1) return;
    const parsed = Number(draft);
    if (!Number.isFinite(parsed)) return;
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    dragState.current = {
      pointerId: event.pointerId,
      lastY: event.clientY,
      value: parsed,
      integer,
    };
  };

  const updateDrag = (event: PointerEvent<HTMLElement>) => {
    const state = dragState.current;
    if (!state) return;
    const dy = state.lastY - event.clientY;
    const range = minValue !== null && maxValue !== null && maxValue > minValue
      ? maxValue - minValue
      : Math.max(Math.abs(state.value), 1);
    const next = clampValue(
      state.integer
        ? Math.round(state.value + dy * range * RANGE_FRACTION_PER_DRAG_PIXEL)
        : state.value + dy * range * RANGE_FRACTION_PER_DRAG_PIXEL,
      minValue,
      maxValue,
    );
    const formatted = formatValue(next);
    state.lastY = event.clientY;
    state.value = Number(formatted);
    setDraft(formatted);
    onChange(formatted);
  };

  const commitValue = (next: string) => {
    setDraft(next);
    const parsed = numberOrNull(next);
    if (parsed === null) return;
    onChange(formatValue(clampValue(parsed, minValue, maxValue)));
  };

  const pointerEvents = {
    onPointerCancel: (event: PointerEvent<HTMLElement>) => {
      event.stopPropagation();
      dragState.current = null;
    },
    onPointerDown: (event: PointerEvent<HTMLElement>) => {
      event.stopPropagation();
      startDrag(event);
    },
    onPointerMove: (event: PointerEvent<HTMLElement>) => {
      event.stopPropagation();
      updateDrag(event);
    },
    onPointerUp: (event: PointerEvent<HTMLElement>) => {
      event.stopPropagation();
      dragState.current = null;
    },
  };

  const control = compact ? (
    <div
      aria-label={`${ariaLabel} percent`}
      aria-valuemax={100}
      aria-valuemin={0}
      aria-valuenow={Math.round(percent)}
      className="knob"
      role="slider"
      style={{ '--knob-percent': `${percent}%`, '--knob-angle': `${percent * 2.7 - 135}deg` } as CSSProperties}
      tabIndex={0}
      {...pointerEvents}
    >
      <div className="knob-ring" />
      <div className="knob-cap">
        <div className="knob-pointer" />
      </div>
    </div>
  ) : (
    <input
      aria-label={`${ariaLabel} percent`}
      className="param-list__knob-input"
      inputMode="decimal"
      readOnly
      style={{ '--knob-percent': `${percent}%` } as CSSProperties}
      value={`${Math.round(percent)}%`}
      {...pointerEvents}
    />
  );

  if (compact) {
    return (
      <label className="unit-knob knob-wrapper nodrag nopan" onPointerDown={event => event.stopPropagation()}>
        {control}
        <span className="knob-label">{label ?? ariaLabel}</span>
        <output className="knob-value">{draft}{unit ? ` ${unit}` : ''}</output>
      </label>
    );
  }

  return (
    <div className="param-list__control">
      {control}
      <label className="param-list__value-field">
        <input
          aria-label={ariaLabel}
          className={outOfRange ? 'param-list__value-input param-list__value-input--invalid' : 'param-list__value-input'}
          inputMode="decimal"
          onBlur={() => commitValue(draft)}
          onChange={event => {
            const next = event.target.value;
            setDraft(next);
            const parsed = numberOrNull(next);
            if (parsed === null) return;
            if ((minValue !== null && parsed < minValue) || (maxValue !== null && parsed > maxValue)) return;
            onChange(formatValue(parsed));
          }}
          value={draft}
        />
        {unit ? <span>{unit}</span> : null}
      </label>
    </div>
  );
}
