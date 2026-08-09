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

function toTestId(value: string): string {
  return value
    .normalize('NFKD')
    .replace(/[\u0300-\u036f]/g, '')
    .toLowerCase()
    .replace(/[^a-z0-9._-]+/g, '-')
    .replace(/-+/g, '-')
    .replace(/^-|-$/g, '')
    .slice(0, 90);
}

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

function polarToCartesian(cx: number, cy: number, r: number, angleInDegrees: number) {
  const angleInRadians = ((angleInDegrees - 90) * Math.PI) / 180.0;
  return {
    x: cx + r * Math.cos(angleInRadians),
    y: cy + r * Math.sin(angleInRadians),
  };
}

function describeArc(cx: number, cy: number, r: number, startAngle: number, endAngle: number) {
  const start = polarToCartesian(cx, cy, r, startAngle);
  const end = polarToCartesian(cx, cy, r, endAngle);
  const sweep = endAngle - startAngle;
  if (sweep <= 0.001) return '';
  const largeArcFlag = sweep > 180 ? 1 : 0;

  return [
    'M', start.x.toFixed(3), start.y.toFixed(3),
    'A', r, r, 0, largeArcFlag, 1, end.x.toFixed(3), end.y.toFixed(3),
  ].join(' ');
}

function SvgKnobDial({ percent }: { percent: number }) {
  const cx = 22;
  const cy = 22;
  const r = 17;
  const trackPath = describeArc(cx, cy, r, 225, 495);
  const sweepAngle = (percent / 100) * 270;
  const valuePath = describeArc(cx, cy, r, 225, 225 + sweepAngle);
  const pointerAngle = 225 + sweepAngle;
  const pointerStart = polarToCartesian(cx, cy, r - 13, pointerAngle);
  const pointerEnd = polarToCartesian(cx, cy, r - 5, pointerAngle);

  return (
    <svg viewBox="0 0 44 44" className="knob-svg" aria-hidden="true">
      <path d={trackPath} className="knob-track-svg" />
      {valuePath ? <path d={valuePath} className="knob-value-svg" /> : null}
      <circle cx={cx} cy={cy} r="12.5" className="knob-cap-svg" />
      <line
        x1={pointerStart.x.toFixed(3)}
        y1={pointerStart.y.toFixed(3)}
        x2={pointerEnd.x.toFixed(3)}
        y2={pointerEnd.y.toFixed(3)}
        className="knob-pointer-svg"
      />
    </svg>
  );
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
    lastX: number;
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
      lastX: event.clientX,
      value: parsed,
      integer,
    };
  };

  const updateDrag = (event: PointerEvent<HTMLElement>) => {
    const state = dragState.current;
    if (!state) return;
    const dx = event.clientX - state.lastX;
    const range = minValue !== null && maxValue !== null && maxValue > minValue
      ? maxValue - minValue
      : Math.max(Math.abs(state.value), 1);
    const next = clampValue(
      state.integer
        ? Math.round(state.value + dx * range * RANGE_FRACTION_PER_DRAG_PIXEL)
        : state.value + dx * range * RANGE_FRACTION_PER_DRAG_PIXEL,
      minValue,
      maxValue,
    );
    const formatted = formatValue(next);
    state.lastX = event.clientX;
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

  const knobTestId = `param-knob-${toTestId(ariaLabel)}`;

  if (compact) {
    return (
      <label className="unit-knob knob-wrapper nodrag nopan" onPointerDown={event => event.stopPropagation()}>
        <div
          aria-label={`${ariaLabel} percent`}
          aria-valuemax={100}
          aria-valuemin={0}
          aria-valuenow={Math.round(percent)}
          className="knob"
          data-testid={knobTestId}
          role="slider"
          style={{ '--knob-percent': `${percent}%`, '--knob-sweep': `${percent * 2.7}deg` } as CSSProperties}
          tabIndex={0}
          title="Drag left or right to adjust"
          {...pointerEvents}
        >
          <SvgKnobDial percent={percent} />
        </div>
        <span className="knob-label">{label ?? ariaLabel}</span>
        <output className="knob-value">{draft}{unit ? ` ${unit}` : ''}</output>
      </label>
    );
  }

  return (
    <div className="param-list__control">
      <div
        aria-label={`${ariaLabel} percent`}
        aria-valuemax={100}
        aria-valuemin={0}
        aria-valuenow={Math.round(percent)}
        className="knob"
        data-testid={`${knobTestId}-dial`}
        role="slider"
        style={{ '--knob-percent': `${percent}%`, '--knob-sweep': `${percent * 2.7}deg` } as CSSProperties}
        tabIndex={0}
        title="Drag left or right to adjust"
        {...pointerEvents}
      >
        <SvgKnobDial percent={percent} />
      </div>
      <label className="param-list__value-field">
        <input
          aria-label={ariaLabel}
          className={outOfRange ? 'param-list__value-input param-list__value-input--invalid' : 'param-list__value-input'}
          data-testid={`${knobTestId}-field`}
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
