import { useEffect, useRef } from 'react';

import { formatLatencyHint } from '../lib/audioIo';
import { AUDIO_TRACE_STAGE_LABELS, exportAudioTraceReport, formatAudioTraceBudget } from '../lib/audioTrace';
import { useLiveBypass } from '../lib/liveBypass';

type Props = {
  onClose: () => void;
  open: boolean;
};

function formatLatency(value: number | null): string {
  return value === null ? 'Unavailable' : `${value.toFixed(3)} ms`;
}

export function AudioIoDrawer({ onClose, open }: Props) {
  const { controller: liveAudio } = useLiveBypass();
  const drawerRef = useRef<HTMLElement>(null);
  const audioInputs = liveAudio?.audioDevices.filter(device => device.kind === 'audioinput') ?? [];
  const audioOutputs = liveAudio?.audioDevices.filter(device => device.kind === 'audiooutput') ?? [];
  const audioRuntime = liveAudio?.audioRuntimeSettings ?? null;
  const calibrationRunning = liveAudio?.audioCalibration.status === 'running';

  useEffect(() => {
    if (!open) return;
    drawerRef.current?.focus();
    const close = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', close);
    return () => window.removeEventListener('keydown', close);
  }, [onClose, open]);

  if (!open) return null;

  return (
    <aside
      aria-label="Audio I/O settings"
      className="audio-io-drawer"
      data-testid="audio-io-drawer"
      ref={drawerRef}
      tabIndex={-1}
    >
      <header>
        <div><span>Live preview</span><strong>Audio I/O</strong></div>
        <button aria-label="Close Audio I/O" onClick={onClose} type="button">×</button>
      </header>
      <section className="audio-io-panel" data-testid="audio-io-panel">
        <div className="audio-io-fieldset">
          <label>
            <span>Input</span>
            <select
              data-testid="audio-input-device"
              disabled={!liveAudio || calibrationRunning}
              onChange={event => void liveAudio?.selectAudioInput(event.target.value)}
              value={liveAudio?.audioIoPreference.inputDeviceId ?? 'default'}
            >
              {audioInputs.length > 0
                ? audioInputs.map(device => <option key={`input-${device.deviceId}`} value={device.deviceId}>{device.label}</option>)
                : <option value="default">System default</option>}
            </select>
          </label>
          <label>
            <span>Output</span>
            <select
              data-testid="audio-output-device"
              disabled={!liveAudio || calibrationRunning}
              onChange={event => void liveAudio?.selectAudioOutput(event.target.value)}
              value={liveAudio?.audioIoPreference.outputDeviceId ?? 'default'}
            >
              {audioOutputs.length > 0
                ? audioOutputs.map(device => <option key={`output-${device.deviceId}`} value={device.deviceId}>{device.label}</option>)
                : <option value="default">System default</option>}
            </select>
          </label>
        </div>
        <div className="audio-io-actions">
          <button
            aria-label="Refresh audio devices"
            className="icon-button"
            disabled={!liveAudio || calibrationRunning}
            onClick={() => void liveAudio?.refreshAudioDevices()}
            title="Refresh audio devices"
            type="button"
          >
            <i className="fa-solid fa-rotate" aria-hidden="true" />
          </button>
          <button
            data-testid="audio-calibrate"
            disabled={!liveAudio || liveAudio.inputMode !== 'microphone' || calibrationRunning}
            onClick={() => void liveAudio?.calibrateAudio()}
            type="button"
          >
            <i className="fa-solid fa-gauge-high" aria-hidden="true" />
            {calibrationRunning ? 'Calibrating' : 'Calibrate latency'}
          </button>
          <button
            aria-label="Run latency chirp"
            className="icon-button"
            data-testid="audio-latency-chirp"
            disabled={!liveAudio?.running || liveAudio.inputMode !== 'microphone' || liveAudio.measuringLatency || calibrationRunning}
            onClick={() => void liveAudio?.measureAcousticLatency()}
            title="Run latency chirp"
            type="button"
          >
            <i className="fa-solid fa-wave-square" aria-hidden="true" />
          </button>
        </div>
        {calibrationRunning ? (
          <div className="audio-trace-progress" aria-label="Latency calibration progress">
            <span style={{ width: `${Math.round((liveAudio?.audioCalibration.progress ?? 0) * 100)}%` }} />
          </div>
        ) : null}
        {audioRuntime ? (
          <div className="audio-io-runtime">
            <div><span>Context</span><strong>{audioRuntime.contextSampleRate} Hz</strong></div>
            <div><span>Input</span><strong>{audioRuntime.inputSampleRate === null ? 'Unavailable' : `${audioRuntime.inputSampleRate} Hz`}</strong></div>
            <div><span>Capture</span><strong>{formatLatency(audioRuntime.captureLatencyMs)}</strong></div>
            <div><span>Base</span><strong>{formatLatency(audioRuntime.baseLatencyMs)}</strong></div>
            <div><span>Output</span><strong>{formatLatency(audioRuntime.outputLatencyMs)}</strong></div>
            <div><span>Hint</span><strong>{formatLatencyHint(liveAudio!.audioIoPreference.latencyHint)}</strong></div>
          </div>
        ) : null}
        {audioRuntime?.sampleRateMismatch ? (
          <p className="audio-io-warning">Input and output sample rates differ; browser resampling adds work and may add buffering.</p>
        ) : null}
        {audioRuntime?.outputRoutingWarning ? (
          <p className="audio-io-warning">{audioRuntime.outputRoutingWarning}</p>
        ) : null}
        {liveAudio?.audioCalibration.status === 'complete' ? (
          <p className="audio-io-result">Selected {formatLatencyHint(liveAudio.audioCalibration.selectedHint!)}</p>
        ) : null}
        {liveAudio?.audioCalibration.status === 'error' ? (
          <p className="audio-io-warning">{liveAudio.audioCalibration.error}</p>
        ) : null}
        <section className="audio-trace-panel" data-testid="audio-trace-panel">
          <div className="audio-trace-panel__header">
            <div>
              <span>Audio latency profile</span>
              <strong data-testid="audio-trace-status">{liveAudio?.audioTraceStatus ?? 'idle'}</strong>
            </div>
            <div className="audio-trace-panel__actions">
              <button
                data-testid="audio-trace-profile"
                disabled={!liveAudio?.running || liveAudio.inputMode !== 'microphone' || liveAudio.audioTraceStatus === 'running'}
                onClick={() => void liveAudio?.profileAudio()}
                title="Profile microphone audio"
                type="button"
              >
                <i className="fa-solid fa-stopwatch" aria-hidden="true" />
                Profile audio
              </button>
              {liveAudio?.audioTraceReport ? (
                <>
                  <button
                    aria-label="Export audio latency profile"
                    className="icon-button"
                    data-testid="audio-trace-export"
                    onClick={() => exportAudioTraceReport(liveAudio.audioTraceReport!)}
                    title="Export audio latency profile"
                    type="button"
                  >
                    <i className="fa-solid fa-file-export" aria-hidden="true" />
                  </button>
                  <button
                    aria-label="Clear audio latency profile"
                    className="icon-button"
                    onClick={liveAudio.clearAudioTrace}
                    title="Clear audio latency profile"
                    type="button"
                  >
                    <i className="fa-solid fa-xmark" aria-hidden="true" />
                  </button>
                </>
              ) : null}
            </div>
          </div>
          {liveAudio?.audioTraceStatus === 'running' ? (
            <div className="audio-trace-progress" aria-label="Audio profile progress">
              <span style={{ width: `${Math.round(liveAudio.audioTraceProgress * 100)}%` }} />
            </div>
          ) : null}
          {liveAudio?.audioTraceReport ? (
            <div className="audio-trace-report" data-testid="audio-trace-report">
              <p className={`audio-trace-verdict audio-trace-verdict--${liveAudio.audioTraceReport.verdict}`}>
                {liveAudio.audioTraceReport.message}
              </p>
              <div className="audio-trace-summary">
                <div><span>Sample rate</span><strong>{liveAudio.audioTraceReport.trace.sampleRate} Hz</strong></div>
                <div><span>Quantum</span><strong>{liveAudio.audioTraceReport.trace.quantumFrames} frames</strong></div>
                <div><span>Deadline</span><strong>{formatLatency(liveAudio.audioTraceReport.trace.deadlineMs)}</strong></div>
                <div><span>Capture est.</span><strong>{formatLatency(liveAudio.audioTraceReport.browser.captureLatencyMs)}</strong></div>
                <div><span>Base est.</span><strong>{formatLatency(liveAudio.audioTraceReport.browser.baseLatencyMs)}</strong></div>
                <div><span>Output est.</span><strong>{formatLatency(liveAudio.audioTraceReport.browser.outputLatencyMs)}</strong></div>
                <div><span>Loopback</span><strong>{formatLatency(liveAudio.audioTraceReport.browser.acousticLoopbackMs)}</strong></div>
                <div><span>Underruns</span><strong>{liveAudio.audioTraceReport.trace.underrunsDelta}</strong></div>
                <div><span>Deadline misses</span><strong>{liveAudio.audioTraceReport.trace.callbackDeadlineMissesDelta}</strong></div>
              </div>
              <div className="audio-trace-table" role="table" aria-label="Audio callback stage timings">
                <div className="audio-trace-table__header" role="row">
                  <span>Stage</span><span>Mean</span><span>P95</span><span>Max</span><span>Budget</span>
                </div>
                {Object.entries(liveAudio.audioTraceReport.trace.stages).map(([name, stage]) => (
                  <div className="audio-trace-table__row" key={name} role="row">
                    <strong>{AUDIO_TRACE_STAGE_LABELS[name as keyof typeof AUDIO_TRACE_STAGE_LABELS]}</strong>
                    <span>{stage.meanMs.toFixed(3)}</span>
                    <span>{stage.p95Ms.toFixed(3)}</span>
                    <span>{stage.maxMs.toFixed(3)}</span>
                    <span>{formatAudioTraceBudget(name as keyof typeof AUDIO_TRACE_STAGE_LABELS, stage.deadlineUtilization)}</span>
                  </div>
                ))}
              </div>
              <p className="audio-trace-slowest">
                Slowest internal stage: <strong>{liveAudio.audioTraceReport.slowestInternalStage
                  ? AUDIO_TRACE_STAGE_LABELS[liveAudio.audioTraceReport.slowestInternalStage]
                  : 'Unavailable'}</strong>
              </p>
            </div>
          ) : null}
        </section>
        {liveAudio?.audioCalibration.candidates.length ? (
          <section className="audio-calibration-report" data-testid="audio-calibration-report">
            <div className="audio-calibration-report__header">
              <span>Calibration candidates</span>
              <strong>{liveAudio.audioCalibration.status}</strong>
            </div>
            <div className="audio-calibration-table" role="table" aria-label="Latency calibration candidates">
              <div className="audio-calibration-table__row audio-calibration-table__row--header" role="row">
                <span>Hint</span><span>Path</span><span>Max</span><span>Result</span>
              </div>
              {liveAudio.audioCalibration.candidates.map(candidate => (
                <div className="audio-calibration-table__row" key={formatLatencyHint(candidate.requestedHint)} role="row">
                  <strong>{formatLatencyHint(candidate.requestedHint)}</strong>
                  <span>{formatLatency(candidate.runtime.estimatedPathLatencyMs)}</span>
                  <span>{candidate.trace.stages.callbackTotal.maxMs.toFixed(3)} ms</span>
                  <span className={candidate.stable ? 'audio-calibration-ok' : 'audio-calibration-bad'}>
                    {candidate.stable ? 'Stable' : candidate.rejectionReason}
                  </span>
                </div>
              ))}
            </div>
          </section>
        ) : null}
      </section>
    </aside>
  );
}
