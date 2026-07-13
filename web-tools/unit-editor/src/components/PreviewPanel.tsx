import {
  WasmBackend,
  type BackendState,
  type BackendPhase,
  type MeterSnapshot,
  type ValidationResult,
  type WasmDiagnostic,
} from '@audio-playground/wasm-tools';
import { useCallback, useEffect, useRef, useState } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import { useLiveBypass } from '../lib/liveBypass';
import type { ParamOverride } from '../lib/projectParams';

type InputMode = 'file' | 'microphone';

type Props = {
  entryProject: string;
  workspaceFiles: WorkspaceFile[];
  paramOverrides: ParamOverride[];
};

const emptyMeter: MeterSnapshot = {
  peak: 0,
  rms: 0,
  frames: 0,
  valid: false,
  activeRevision: 0,
  underruns: 0,
};

const emptyBackendState: BackendState = {
  phase: 'idle',
  workspaceRevision: 0,
  preparedRevision: 0,
  activeRevision: 0,
  failedRevision: 0,
  lastError: null,
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

export function PreviewPanel({ entryProject, workspaceFiles, paramOverrides }: Props) {
  const { setController } = useLiveBypass();
  const [backend, setBackend] = useState<WasmBackend | null>(null);
  const [phase, setPhase] = useState<BackendPhase>('idle');
  const [diagnostic, setDiagnostic] = useState('WASM backend is initializing.');
  const [backendState, setBackendState] = useState<BackendState>(emptyBackendState);
  const [backendDiagnostic, setBackendDiagnostic] = useState<WasmDiagnostic | null>(null);
  const [meter, setMeter] = useState<MeterSnapshot>(emptyMeter);
  const [latencyMs, setLatencyMs] = useState<number | null>(null);
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
  const previousOverridesRef = useRef<Map<string, ParamOverride>>(new Map());
  const firstOverride = paramOverrides[0];

  const refreshBackendState = useCallback((clearDiagnostic = false) => {
    const instance = backendRef.current;
    if (!instance) return;
    const next = instance.getState();
    setBackendState(next);
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
    const context = new AudioContext();
    contextRef.current = context;
    let disposed = false;
    void WasmBackend.create({
      controlModuleUrl: moduleUrl('apg_control.mjs'),
      processorModuleUrl: moduleUrl('apg_processor.mjs'),
      processorWasmUrl: moduleUrl('apg_processor.wasm'),
      processorWorkletUrl: moduleUrl('apg_processor.worklet.js'),
      audioContext: context,
    })
      .then(instance => {
        if (disposed) {
          void instance.destroy();
          return;
        }
        backendRef.current = instance;
        setBackend(instance);
        setBackendState(instance.getState());
        setDiagnostic('WASM control backend ready.');
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
  }, [reportError]);

  const syncWorkspace = useCallback(
    async (prepare: boolean) => {
      if (!backend || !contextRef.current) return false;
      const revision = revisionRef.current;
      if (validRevisionRef.current !== revision) {
        setPhase('validating');
        const validationPromise = backend.replaceWorkspace({
          revision,
          entryProject,
          files: workspaceFiles.map(({ path, role, content }) => ({ path, role, content })),
        });
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
        setDiagnostic(`Revision ${revision} validated.`);
      }
      if (prepare) {
        setPhase('preparing');
        const preparePromise = backend.prepare(revision, {
          sampleRate: Math.round(contextRef.current.sampleRate),
          blockFrames: 128,
        });
        refreshBackendState(true);
        await preparePromise;
        if (revision !== revisionRef.current) return false;
        if (runningRef.current) await backend.commitPrepared(revision);
        if (revision !== revisionRef.current) return false;
        refreshBackendState(true);
        setDiagnostic(`Revision ${revision} prepared for live audio.`);
      }
      setPhase(runningRef.current ? 'running' : prepare ? 'ready' : 'idle');
      return true;
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
    const timer = window.setInterval(() => {
      if (polling) return;
      polling = true;
      void backend
        .pollMeters()
        .then(setMeter)
        .catch(error => {
          reportError(error, 'meter');
        })
        .finally(() => {
          polling = false;
        });
    }, 100);
    return () => window.clearInterval(timer);
  }, [backend, reportError, running]);

  useEffect(() => {
    if (!running || !contextRef.current) return;
    const refreshLatency = () => setLatencyMs(outputLatencyMs(contextRef.current!));
    refreshLatency();
    const timer = window.setInterval(refreshLatency, 500);
    return () => window.clearInterval(timer);
  }, [running]);

  useEffect(() => {
    if (!backend || !running) return;
    const previous = previousOverridesRef.current;
    const current = new Map(paramOverrides.map(override => [override.path, override]));
    const commands = paramOverrides.map(override => ({ path: override.path, value: override.value }));
    for (const [path, override] of previous) {
      if (!current.has(path)) commands.push({ path, value: override.originalValue });
    }
    previousOverridesRef.current = current;
    for (const command of commands) {
      const value = Number(command.value);
      if (Number.isFinite(value)) {
        void backend.setParam(command.path, value).catch(error => {
          reportError(error, 'control');
        });
      }
    }
  }, [backend, paramOverrides, reportError, running]);

  const compile = useCallback(async () => {
    try {
      await syncWorkspace(true);
    } catch (error) {
      reportError(error, 'compile');
    }
  }, [reportError, syncWorkspace]);

  const stopPlayback = useCallback(async () => {
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
    await backendRef.current?.stop();
    refreshBackendState(true);
    runningRef.current = false;
    setRunning(false);
    setPhase('ready');
    setMeter(emptyMeter);
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
        if (!audioBuffer) throw new Error('Select an audio file before starting playback.');
        const source = context.createBufferSource();
        source.buffer = audioBuffer;
        fileSourceRef.current = source;
        input = source;
      } else {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        const source = context.createMediaStreamSource(stream);
        streamRef.current = stream;
        inputRef.current = source;
        input = source;
      }
      await backend.start({ input });
      refreshBackendState(true);
      runningRef.current = true;
      setRunning(true);
      setPhase('running');
      if (fileSourceRef.current) {
        fileSourceRef.current.onended = () => void stopPlayback();
        fileSourceRef.current.start();
      }
      setDiagnostic(
        inputMode === 'file'
          ? `Revision ${revisionRef.current} is processing ${audioFileName}.`
          : `Revision ${revisionRef.current} is processing microphone input.`,
      );
    } catch (error) {
      await stopPlayback();
      reportError(error, 'start');
    }
  }, [audioBuffer, audioFileName, backend, inputMode, refreshBackendState, reportError, stopPlayback, syncWorkspace]);

  const stop = useCallback(async () => {
    await stopPlayback();
    setDiagnostic(`Revision ${revisionRef.current} is ready.`);
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
      setDiagnostic(`Revision ${revisionRef.current} runtime reset.`);
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

  useEffect(() => {
    setController({ running, latencyMs, bypassByInstance, setBypass: setInstanceBypass });
  }, [bypassByInstance, latencyMs, running, setController, setInstanceBypass]);

  useEffect(() => () => setController(null), [setController]);

  const toggleMute = useCallback(async () => {
    if (!backend) return;
    const next = !muted;
    try {
      await backend.setMute(next);
      setMuted(next);
      setDiagnostic(`Project output ${next ? 'muted' : 'unmuted'}.`);
    } catch (error) {
      reportError(error, 'control');
    }
  }, [backend, muted, reportError]);

  return (
    <section className="inspector-block">
      <div className="inspector-block__label">Live Preview</div>
      <div className="preview-panel__state">
        <strong>{phase}</strong>
        <span>WASM AudioWorklet</span>
      </div>
      <p className={`diagnostic-empty${phase === 'error' ? ' diagnostic-empty--error' : ''}`}>{diagnostic}</p>
      <div className="revision-grid" aria-label="WASM revision state">
        <div><span>Workspace</span><strong>{backendState.workspaceRevision || '-'}</strong></div>
        <div><span>Prepared</span><strong>{backendState.preparedRevision || '-'}</strong></div>
        <div><span>Active</span><strong>{backendState.activeRevision || '-'}</strong></div>
        <div><span>Failed</span><strong>{backendState.failedRevision || '-'}</strong></div>
      </div>
      {backendDiagnostic && (
        <dl className="wasm-diagnostic" aria-label="WASM diagnostic">
          <div><dt>Code</dt><dd>{backendDiagnostic.code}</dd></div>
          <div><dt>Phase</dt><dd>{backendDiagnostic.phase}</dd></div>
          <div><dt>Revision</dt><dd>{backendDiagnostic.revision}</dd></div>
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
        <button className="btn btn--ghost" disabled={!backend} onClick={() => void compile()} type="button">
          Compile
        </button>
        <button className="btn btn--ghost" disabled={!backend || running} onClick={() => void start()} type="button">
          Start
        </button>
        <button className="btn btn--ghost" disabled={!running} onClick={() => void stop()} type="button">
          Stop
        </button>
        <button className="btn btn--ghost" disabled={!running} onClick={() => void reset()} type="button">
          Reset
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
          <span>Active</span>
          <strong>{meter.activeRevision || '-'}</strong>
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
        <button className="btn btn--ghost" disabled={!running} onClick={() => void toggleMute()} type="button">
          {muted ? 'Unmute output' : 'Mute output'}
        </button>
      </div>
    </section>
  );
}
