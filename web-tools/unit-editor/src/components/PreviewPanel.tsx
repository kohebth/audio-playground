import {
  WasmBackend,
  type BackendPhase,
  type MeterSnapshot,
  type ValidationResult,
  type WasmDiagnostic,
} from '@audio-playground/wasm-tools';
import { useCallback, useEffect, useRef, useState } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import { useLiveBypass } from '../lib/liveBypass';
import type { ParamOverride } from '../lib/projectParams';
import { markPerfSpan, recordRuntimeSnapshot } from '../lib/perfTelemetry';

type InputMode = 'file' | 'microphone';

type Props = {
  entryProject: string;
  workspaceFiles: WorkspaceFile[];
  paramOverrides: ParamOverride[];
  compact?: boolean;
  onRuntimeReady?: () => void;
  onSaveWorkspace?: () => void;
};

const emptyMeter: MeterSnapshot = {
  peak: 0,
  rms: 0,
  frames: 0,
  valid: false,
  activeRevision: 0,
  underruns: 0,
};

function moduleUrl(file: string): string {
  return new URL(`wasm/${file}`, `${window.location.origin}${import.meta.env.BASE_URL}`).href;
}

function outputLatencyMs(context: AudioContext): number | null {
  const base = Number.isFinite(context.baseLatency) ? Math.max(context.baseLatency, 0) : 0;
  const output = (context as AudioContext & { outputLatency?: number }).outputLatency;
  const device = Number.isFinite(output) ? Math.max(output ?? 0, 0) : 0;
  return base > 0 || device > 0 ? (base + device) * 1000 : null;
}

function captureLatencyMs(stream: MediaStream): number | null {
  const settings = stream.getAudioTracks()[0]?.getSettings() as MediaTrackSettings & { latency?: number };
  const latency = settings?.latency;
  return Number.isFinite(latency) && latency !== undefined && latency >= 0 ? latency * 1000 : null;
}

function createDefaultPreviewBuffer(context: AudioContext): AudioBuffer {
  const sampleRate = context.sampleRate;
  const durationSeconds = 2.5;
  const buffer = context.createBuffer(1, Math.floor(sampleRate * durationSeconds), sampleRate);
  const data = buffer.getChannelData(0);
  for (let i = 0; i < data.length; i += 1) {
    const t = i / sampleRate;
    const envelope = Math.exp(-2.8 * t);
    const pluck =
      Math.sin(2 * Math.PI * 110 * t) * 0.5 +
      Math.sin(2 * Math.PI * 220 * t) * 0.25 +
      Math.sin(2 * Math.PI * 330 * t) * 0.12;
    data[i] = pluck * envelope * 0.32;
  }
  return buffer;
}

const lowLatencyMicrophoneConstraints = {
  autoGainControl: false,
  channelCount: 1,
  echoCancellation: false,
  latency: { ideal: 0 },
  noiseSuppression: false,
} as MediaTrackConstraints & { latency: { ideal: number } };

export function PreviewPanel({
  entryProject,
  workspaceFiles,
  paramOverrides,
  compact = false,
  onRuntimeReady,
  onSaveWorkspace,
}: Props) {
  const { setController } = useLiveBypass();
  const [backend, setBackend] = useState<WasmBackend | null>(null);
  const [phase, setPhase] = useState<BackendPhase>('idle');
  const [diagnostic, setDiagnostic] = useState('Audio engine is initializing.');
  const [backendDiagnostic, setBackendDiagnostic] = useState<WasmDiagnostic | null>(null);
  const [meter, setMeter] = useState<MeterSnapshot>(emptyMeter);
  const [latencyMs, setLatencyMs] = useState<number | null>(null);
  const [captureLatency, setCaptureLatency] = useState<number | null>(null);
  const [measuredLatencyMs, setMeasuredLatencyMs] = useState<number | null>(null);
  const [measuringLatency, setMeasuringLatency] = useState(false);
  const [bypassByInstance, setBypassByInstance] = useState<Record<string, boolean>>({});
  const [muted, setMuted] = useState(false);
  const [running, setRunning] = useState(false);
  const [inputMode, setInputMode] = useState<InputMode>('file');
  const [audioBuffer, setAudioBuffer] = useState<AudioBuffer | null>(null);
  const [audioFileName, setAudioFileName] = useState('No audio file selected');
  const revisionRef = useRef(0);
  const validRevisionRef = useRef(0);
  const backendRef = useRef<WasmBackend | null>(null);
  const runningRef = useRef(false);
  const contextRef = useRef<AudioContext | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const inputRef = useRef<MediaStreamAudioSourceNode | null>(null);
  const fileSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const meterTimerActiveRef = useRef(false);
  const latencyTimerActiveRef = useRef(false);
  const syncQueueRef = useRef<Promise<void>>(Promise.resolve());
  const previousOverridesRef = useRef<Map<string, ParamOverride>>(new Map());
  const paramControlQueueRef = useRef<Map<string, { draining: boolean; pendingValue: number | null }>>(new Map());
  const firstOverride = paramOverrides[0];

  const refreshBackendState = useCallback((clearDiagnostic = false) => {
    const instance = backendRef.current;
    if (!instance) return;
    const next = instance.getState();
    if (next.lastError) setBackendDiagnostic(next.lastError);
    else if (clearDiagnostic) setBackendDiagnostic(null);
  }, []);

  const reportError = useCallback((error: unknown, errorPhase: string) => {
    const known = backendRef.current?.getLastError();
    const detail: WasmDiagnostic = known ?? {
      revision: revisionRef.current,
      status: -1,
      phase: errorPhase,
      code: 'APG_WEB_RUNTIME_ERROR',
      file: '',
      path: '',
      message: error instanceof Error ? error.message : String(error),
    };
    refreshBackendState();
    setBackendDiagnostic(detail);
    setPhase('error');
    setDiagnostic(detail.message || detail.code);
  }, [refreshBackendState]);

  useEffect(() => {
    const context = new AudioContext({ latencyHint: 'interactive' });
    contextRef.current = context;
    setAudioBuffer(createDefaultPreviewBuffer(context));
    setAudioFileName('test_tone.wav');
    let disposed = false;
    void WasmBackend.create({
      controlModuleUrl: moduleUrl('apg_control.mjs'),
      processorModuleUrl: moduleUrl('apg_processor.mjs'),
      processorWasmUrl: moduleUrl('apg_processor.wasm'),
      processorWorkletUrl: moduleUrl('processor.worklet.js'),
      audioContext: context,
    })
      .then(instance => {
        if (disposed) {
          void instance.destroy();
          return;
        }
        backendRef.current = instance;
        setBackend(instance);
        setDiagnostic('Audio engine ready.');
        onRuntimeReady?.();
      })
      .catch(error => {
        reportError(error, 'initialize');
      });
    return () => {
      disposed = true;
      streamRef.current?.getTracks().forEach(track => track.stop());
      inputRef.current?.disconnect();
      fileSourceRef.current?.disconnect();
      const instance = backendRef.current;
      if (instance) void instance.destroy().finally(() => context.close());
      else void context.close();
    };
  }, [onRuntimeReady, reportError]);

  const syncWorkspace = useCallback(
    (prepare: boolean) => {
      const synchronize = async () => {
        const context = contextRef.current;
        if (!backend || !context) return false;
        const revision = revisionRef.current;
        if (validRevisionRef.current !== revision) {
          setPhase('validating');
          const validationPromise = markPerfSpan('runtime.validate.workspace', () => backend.replaceWorkspace({
            revision,
            entryProject,
            files: workspaceFiles.map(({ path, role, content }) => ({ path, role, content })),
          }), { revision });
          refreshBackendState(true);
          const validation: ValidationResult = await validationPromise;
          if (revision !== revisionRef.current) return false;
          refreshBackendState(validation.ok);
          if (!validation.ok) {
            setPhase('error');
            setBackendDiagnostic(validation.diagnostic);
            setDiagnostic(validation.diagnostic.message || validation.diagnostic.code);
            return false;
          }
          validRevisionRef.current = revision;
          setDiagnostic('Project validated.');
        }
        if (prepare) {
          setPhase('preparing');
          const preparePromise = markPerfSpan('runtime.prepare.workspace', () => backend.prepare(revision, {
            sampleRate: Math.round(context.sampleRate),
            blockFrames: 128,
          }), { revision });
          refreshBackendState(true);
          await preparePromise;
          if (revision !== revisionRef.current) return false;
          if (runningRef.current) {
            await markPerfSpan('runtime.commit.workspace', () => backend.commitPrepared(revision), { revision });
          }
          if (revision !== revisionRef.current) return false;
          refreshBackendState(true);
          setDiagnostic('Project prepared for live audio.');
        }
        setPhase(runningRef.current ? 'running' : prepare ? 'ready' : 'idle');
        return true;
      };
      const instrumented = () => markPerfSpan('runtime.sync.workspace', synchronize, { revision: revisionRef.current });
      const result = syncQueueRef.current.then(instrumented, instrumented);
      syncQueueRef.current = result.then(() => undefined, () => undefined);
      return result;
    },
    [backend, entryProject, refreshBackendState, workspaceFiles],
  );

  useEffect(() => {
    if (!backend) return;
    revisionRef.current += 1;
    backend.setCurrentRevision(revisionRef.current);
    refreshBackendState(true);
    const timer = window.setTimeout(() => {
      void syncWorkspace(runningRef.current).catch(error => {
        if (revisionRef.current < 1) return;
        reportError(error, 'synchronize');
      });
    }, 300);
    return () => window.clearTimeout(timer);
  }, [backend, refreshBackendState, reportError, syncWorkspace, workspaceFiles]);

  useEffect(() => {
    if (!backend || !running) return;
    let polling = false;
    meterTimerActiveRef.current = true;
    const timer = window.setInterval(() => {
      if (polling) return;
      polling = true;
      void markPerfSpan('runtime.meter.poll', () => backend.pollMeters())
        .then(setMeter)
        .catch(error => {
          if (runningRef.current) reportError(error, 'meter');
        })
        .finally(() => {
          polling = false;
        });
    }, 100);
    return () => {
      window.clearInterval(timer);
      meterTimerActiveRef.current = false;
    };
  }, [backend, reportError, running]);

  useEffect(() => {
    if (!running || !contextRef.current) return;
    const refreshLatency = () => setLatencyMs(outputLatencyMs(contextRef.current!));
    refreshLatency();
    latencyTimerActiveRef.current = true;
    const timer = window.setInterval(refreshLatency, 500);
    return () => {
      window.clearInterval(timer);
      latencyTimerActiveRef.current = false;
    };
  }, [running]);

  useEffect(() => {
    if (!backend || !running) return;
    const previous = previousOverridesRef.current;
    const current = new Map(paramOverrides.map(override => [override.path, override]));
    const commands = paramOverrides
      .filter(override => previous.get(override.path)?.value !== override.value)
      .map(override => ({ path: override.path, value: override.value }));
    for (const [path, override] of previous) {
      if (!current.has(path)) commands.push({ path, value: override.originalValue });
    }
    previousOverridesRef.current = current;
    for (const command of commands) {
      const value = Number(command.value);
      if (Number.isFinite(value)) {
        let entry = paramControlQueueRef.current.get(command.path);
        if (!entry) {
          entry = { draining: false, pendingValue: null };
          paramControlQueueRef.current.set(command.path, entry);
        }
        entry.pendingValue = value;
        if (entry.draining) continue;
        entry.draining = true;
        void (async () => {
          try {
            while (runningRef.current && entry.pendingValue !== null) {
              const nextValue = entry.pendingValue;
              entry.pendingValue = null;
              await markPerfSpan(
                'runtime.control.param',
                () => backend.setParam(command.path, nextValue),
                { path: command.path },
              );
            }
          } catch (error) {
            reportError(error, 'control');
          } finally {
            entry.draining = false;
            if (entry.pendingValue === null) paramControlQueueRef.current.delete(command.path);
          }
        })();
      }
    }
  }, [backend, paramOverrides, reportError, running]);

  const compile = useCallback(async () => {
    if (!backend) return false;
    try {
      const prepared = await syncWorkspace(true);
      return prepared;
    } catch (error) {
      reportError(error, 'compile');
      return false;
    }
  }, [backend, reportError, syncWorkspace]);

  const stopPlayback = useCallback(async () => {
    runningRef.current = false;
    paramControlQueueRef.current.clear();
    setRunning(false);
    const fileSource = fileSourceRef.current;
    if (fileSource) {
      fileSource.onended = null;
      try {
        fileSource.stop();
      } catch {
        // The source already reached its natural end.
      }
      fileSource.disconnect();
      fileSourceRef.current = null;
    }
    inputRef.current?.disconnect();
    inputRef.current = null;
    streamRef.current?.getTracks().forEach(track => track.stop());
    streamRef.current = null;
    if (backendRef.current) await markPerfSpan('runtime.stop', () => backendRef.current!.stop());
    refreshBackendState(true);
    setPhase('ready');
    setMeter(emptyMeter);
    setCaptureLatency(null);
    setMeasuredLatencyMs(null);
  }, [refreshBackendState]);

  const start = useCallback(async () => {
    if (!backend) return;
    try {
      if (validRevisionRef.current !== revisionRef.current && !(await syncWorkspace(true))) return;
      else if (backend.getState().preparedRevision !== revisionRef.current && !(await syncWorkspace(true))) return;
      const context = contextRef.current;
      if (!context) return;
      let input: AudioNode;
      if (inputMode === 'file') {
        setCaptureLatency(null);
        if (!audioBuffer) throw new Error('Select an audio file before starting playback.');
        const source = context.createBufferSource();
        source.buffer = audioBuffer;
        fileSourceRef.current = source;
        input = source;
      } else {
        const stream = await navigator.mediaDevices.getUserMedia({
          audio: lowLatencyMicrophoneConstraints,
        });
        setCaptureLatency(captureLatencyMs(stream));
        const source = context.createMediaStreamSource(stream);
        streamRef.current = stream;
        inputRef.current = source;
        input = source;
      }
      await markPerfSpan('runtime.start', () => backend.start({ input }));
      refreshBackendState(true);
      runningRef.current = true;
      setRunning(true);
      setPhase('running');
      setMeasuredLatencyMs(null);
      if (fileSourceRef.current) {
        fileSourceRef.current.onended = () => void stopPlayback();
        fileSourceRef.current.start();
      }
      setDiagnostic(
        inputMode === 'file'
          ? `Processing ${audioFileName}.`
          : 'Processing microphone input.',
      );
    } catch (error) {
      await stopPlayback();
      reportError(error, 'start');
    }
  }, [audioBuffer, audioFileName, backend, inputMode, refreshBackendState, reportError, stopPlayback, syncWorkspace]);

  const stop = useCallback(async () => {
    await stopPlayback();
    setDiagnostic('Project is ready.');
  }, [stopPlayback]);

  const chooseAudioFile = useCallback(async (file: File | undefined) => {
    if (!file || !contextRef.current) return;
    try {
      if (runningRef.current) await stopPlayback();
      const encoded = await file.arrayBuffer();
      const decoded = await contextRef.current.decodeAudioData(encoded.slice(0));
      setAudioBuffer(decoded);
      setAudioFileName(file.name);
      setDiagnostic(`${file.name} ready (${decoded.duration.toFixed(2)} s).`);
    } catch (error) {
      setAudioBuffer(null);
      setAudioFileName('No audio file selected');
      reportError(error, 'decode');
    }
  }, [reportError, stopPlayback]);

  const reset = useCallback(async () => {
    if (!backend || !running) return;
    try {
      await backend.reset();
      setDiagnostic('Runtime reset.');
    } catch (error) {
      reportError(error, 'reset');
    }
  }, [backend, reportError, running]);

  const sendFirstParam = useCallback(async () => {
    if (!backend || !firstOverride) return;
    const value = Number(firstOverride.value);
    if (!Number.isFinite(value)) return;
    try {
      await backend.setParam(firstOverride.path, value);
      setDiagnostic(`Updated ${firstOverride.path}.`);
    } catch (error) {
      reportError(error, 'control');
    }
  }, [backend, firstOverride, reportError]);

  const setInstanceBypass = useCallback(async (instanceId: string, enabled: boolean) => {
    if (!backend) return;
    try {
      await backend.setBypass(instanceId, enabled);
      setBypassByInstance(current => ({ ...current, [instanceId]: enabled }));
      setDiagnostic(`${instanceId} bypass ${enabled ? 'enabled' : 'disabled'}.`);
    } catch (error) {
      reportError(error, 'control');
    }
  }, [backend, reportError]);

  const measureAcousticLatency = useCallback(async () => {
    if (!backend || !running || inputMode !== 'microphone') return;
    setMeasuringLatency(true);
    setMeasuredLatencyMs(null);
    try {
      const measured = await backend.measureLatencyProbe();
      setMeasuredLatencyMs(measured);
      setDiagnostic(`Measured acoustic loopback: ${measured.toFixed(1)} ms.`);
    } catch (error) {
      reportError(error, 'latency-probe');
    } finally {
      setMeasuringLatency(false);
    }
  }, [backend, inputMode, reportError, running]);

  useEffect(() => {
    setController({
      running,
      latencyMs,
      captureLatencyMs: captureLatency,
      measuredLatencyMs,
      bypassByInstance,
      setBypass: setInstanceBypass,
    });
  }, [bypassByInstance, captureLatency, latencyMs, measuredLatencyMs, running, setController, setInstanceBypass]);

  useEffect(() => () => setController(null), [setController]);

  useEffect(() => {
    if (!backend) return;
    const state = backend.getState();
    const resources = backend.getResourceSnapshot();
    recordRuntimeSnapshot({
      phase,
      activeRevision: state.activeRevision,
      preparedRevision: state.preparedRevision,
      meter: { frames: meter.frames, valid: meter.valid, underruns: meter.underruns },
      resources: {
        ...resources,
        streamTracks: streamRef.current?.getTracks().length ?? 0,
        inputNodeActive: inputRef.current !== null,
        fileSourceActive: fileSourceRef.current !== null,
        meterTimerActive: meterTimerActiveRef.current,
        latencyTimerActive: latencyTimerActiveRef.current,
      },
      at: Date.now(),
    });
  }, [backend, meter, phase, running]);

  const toggleMute = useCallback(async () => {
    if (!backend || !running) return;
    const next = !muted;
    try {
      await backend.setMute(next);
      setMuted(next);
      setDiagnostic(`Project output ${next ? 'muted' : 'unmuted'}.`);
    } catch (error) {
      reportError(error, 'control');
    }
  }, [backend, muted, reportError, running]);

  const togglePlayback = useCallback(() => {
    void (running ? stop() : start());
  }, [running, start, stop]);

  const handleBuildAndSave = useCallback(async () => {
    if (backend) {
      const compiled = await compile();
      if (!compiled) return;
    }
    onSaveWorkspace?.();
  }, [backend, compile, onSaveWorkspace]);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.repeat) return;

      const target = event.target;
      if (!(target instanceof HTMLElement)) return;
      if (target.isContentEditable || target.closest('[contenteditable="true"]')) return;
      const tag = target.tagName.toLowerCase();
      if (tag === 'input' || tag === 'textarea' || tag === 'select' || tag === 'option') return;

      const key = event.key.toLowerCase();
      if ((event.ctrlKey || event.metaKey) && key === 's') {
        event.preventDefault();
        onSaveWorkspace?.();
        return;
      }

      if (key === ' ' || event.code === 'Space') {
        event.preventDefault();
        togglePlayback();
        return;
      }

      if (key === 'm') {
        event.preventDefault();
        void toggleMute();
        return;
      }

      if (key === 'b') {
        event.preventDefault();
        void handleBuildAndSave();
      }
    };

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [handleBuildAndSave, onSaveWorkspace, toggleMute, togglePlayback]);

  return (
    <section className={compact ? 'transport-island preview-panel--compact' : 'inspector-block'}>
      {compact ? (
        <>
      <div className="transport-group">
            <button
              className={`transport-btn ${running ? 'active' : ''}`}
              data-testid="preview-start-stop"
              disabled={!backend}
              onClick={() => void togglePlayback()}
              type="button"
            >
              <i className={`fa-solid ${running ? 'fa-stop' : 'fa-play'}`} aria-hidden="true" />
            </button>
            <button className="transport-btn" data-testid="preview-compile" disabled={!backend} onClick={() => void compile()} title="Compile" type="button">
              <i className="fa-solid fa-hammer" aria-hidden="true" />
            </button>
        </div>
          <div className="transport-group preview-panel__mode" aria-label="Audio input mode" role="group">
            <button
              aria-pressed={inputMode === 'file'}
              data-testid="preview-mode-file"
              className={`source-toggle ${inputMode === 'file' ? 'active' : ''}`}
              disabled={running}
              onClick={() => setInputMode('file')}
              type="button"
            >
              <i className="fa-solid fa-file-audio" aria-hidden="true" />
              Audio File
            </button>
            <button
              aria-pressed={inputMode === 'microphone'}
              data-testid="preview-mode-mic"
              className={`source-toggle ${inputMode === 'microphone' ? 'active' : ''}`}
              disabled={running}
              onClick={() => setInputMode('microphone')}
              type="button"
            >
              <i className="fa-solid fa-microphone" aria-hidden="true" />
              Mic
            </button>
            {inputMode === 'file' && (
              <label className="transport-file">
                <span>{audioFileName}</span>
                <input accept="audio/*,.wav,.mp3,.flac,.ogg,.m4a,.aac" disabled={running} onChange={event => void chooseAudioFile(event.target.files?.[0])} type="file" />
              </label>
            )}
          </div>
          <div className="transport-group">
            <div className="mini-viz" aria-hidden="true">
              {[0, 1, 2, 3, 4].map(index => (
                <span key={index} className={`mini-viz-bar ${running ? 'active' : ''}`} />
              ))}
            </div>
            <span className={`transport-state transport-state--${phase}`}>{phase}</span>
            <button className="transport-btn" disabled={!running} onClick={() => void toggleMute()} title={muted ? 'Unmute output' : 'Mute output'} type="button">
              <i className={`fa-solid ${muted ? 'fa-volume-xmark' : 'fa-volume-high'}`} aria-hidden="true" />
            </button>
          </div>
        </>
      ) : (
        <>
      <div className="inspector-block__label">Live Preview</div>
      <div className="preview-panel__state">
        <strong>{phase}</strong>
        <span>Live engine</span>
      </div>
      <p className={`diagnostic-empty${phase === 'error' ? ' diagnostic-empty--error' : ''}`}>{diagnostic}</p>
      {backendDiagnostic && (
        <dl className="wasm-diagnostic" aria-label="WASM diagnostic">
          <div><dt>Code</dt><dd>{backendDiagnostic.code}</dd></div>
          <div><dt>Phase</dt><dd>{backendDiagnostic.phase}</dd></div>
          {backendDiagnostic.file && <div><dt>File</dt><dd>{backendDiagnostic.file}</dd></div>}
          {backendDiagnostic.path && <div><dt>Path</dt><dd>{backendDiagnostic.path}</dd></div>}
        </dl>
      )}

      <div className="preview-panel__mode" aria-label="Audio input mode" role="group">
        <button
          aria-pressed={inputMode === 'file'}
          disabled={running}
          onClick={() => setInputMode('file')}
          type="button"
        >
          Audio file
        </button>
        <button
          aria-pressed={inputMode === 'microphone'}
          disabled={running}
          onClick={() => setInputMode('microphone')}
          type="button"
        >
          Microphone
        </button>
      </div>

      {inputMode === 'file' && (
        <label className="preview-panel__file">
          <span>{audioFileName}</span>
          <input
            accept="audio/*,.wav,.mp3,.flac,.ogg,.m4a,.aac"
            disabled={running}
            onChange={event => void chooseAudioFile(event.target.files?.[0])}
            type="file"
          />
        </label>
      )}

      <div className="preview-panel__actions">
        <button className="btn btn--ghost" data-testid="preview-compile" disabled={!backend} onClick={() => void compile()} type="button">
          Compile
        </button>
        <button
          className="btn btn--ghost"
          data-testid="preview-start-stop"
          disabled={!backend}
          onClick={() => void togglePlayback()}
          type="button"
        >
          {running ? 'Stop' : 'Start'}
        </button>
        <button className="btn btn--ghost" data-testid="preview-reset" disabled={!running} onClick={() => void reset()} type="button">
          Reset
        </button>
        <button
          className="btn btn--ghost"
          data-testid="preview-latency"
          disabled={!running || inputMode !== 'microphone' || measuringLatency}
          onClick={() => void measureAcousticLatency()}
          type="button"
        >
          {measuringLatency ? 'Listening...' : 'Latency chirp'}
        </button>
      </div>

      <div className="meter-grid">
        <div>
          <span>Peak</span>
          <strong>{meter.peak.toFixed(6)}</strong>
        </div>
        <div>
          <span>RMS</span>
          <strong>{meter.rms.toFixed(6)}</strong>
        </div>
        <div>
          <span>Frames</span>
          <strong>{meter.frames}</strong>
        </div>
        <div>
          <span>Underruns</span>
          <strong>{meter.underruns}</strong>
        </div>
      </div>

      <div className="preview-panel__actions">
        <button className="btn btn--ghost" disabled={!running || !firstOverride} onClick={() => void sendFirstParam()} type="button">
          Send param
        </button>
        <button className="btn btn--ghost" data-testid="preview-mute" disabled={!running} onClick={() => void toggleMute()} type="button">
          {muted ? 'Unmute output' : 'Mute output'}
        </button>
      </div>
        </>
      )}
    </section>
  );
}
