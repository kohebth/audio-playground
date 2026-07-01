import { useMemo, useState } from 'react';

import type { RenderResult } from '../lib/backendSamples';
import type { ParamOverride } from '../lib/projectParams';
import { createDeterministicPreviewAdapter, type PreviewState } from '../lib/previewAdapter';

type Props = {
  render: RenderResult;
  paramOverrides: ParamOverride[];
  selectedInstanceId: string | null;
};

export function PreviewPanel({ render, paramOverrides, selectedInstanceId }: Props) {
  const adapter = useMemo(() => createDeterministicPreviewAdapter(render), [render]);
  const [state, setState] = useState<PreviewState>('idle');
  const [lastCommand, setLastCommand] = useState('deterministic render adapter');
  const [bypass, setBypass] = useState(false);
  const meters = adapter.getMeters();
  const firstOverride = paramOverrides[0];

  return (
    <section className="inspector-block">
      <div className="inspector-block__label">Live Preview</div>
      <div className="preview-panel__state">
        <strong>{state}</strong>
        <span>deterministic render backend</span>
      </div>

      <div className="preview-panel__actions">
        <button className="btn btn--ghost" onClick={() => setState(adapter.compile())} type="button">
          Compile
        </button>
        <button className="btn btn--ghost" onClick={() => setState(adapter.start())} type="button">
          Start
        </button>
        <button className="btn btn--ghost" onClick={() => setState(adapter.stop())} type="button">
          Stop
        </button>
      </div>

      <div className="meter-grid">
        <div>
          <span>Peak</span>
          <strong>{meters.peak.toFixed(6)}</strong>
        </div>
        <div>
          <span>RMS</span>
          <strong>{meters.rms.toFixed(6)}</strong>
        </div>
        <div>
          <span>Frames</span>
          <strong>{meters.frames}</strong>
        </div>
      </div>

      <div className="preview-panel__actions">
        <button
          className="btn btn--ghost"
          disabled={!firstOverride}
          onClick={() => firstOverride && setLastCommand(adapter.setParam(firstOverride.path, firstOverride.value))}
          type="button"
        >
          Send param
        </button>
        <button
          className="btn btn--ghost"
          disabled={!selectedInstanceId}
          onClick={() => {
            const next = !bypass;
            setBypass(next);
            if (selectedInstanceId) setLastCommand(adapter.setBypass(selectedInstanceId, next));
          }}
          type="button"
        >
          Toggle bypass
        </button>
      </div>

      <div className="command-panel">
        <span>Preview command</span>
        <code>{lastCommand}</code>
      </div>
    </section>
  );
}
