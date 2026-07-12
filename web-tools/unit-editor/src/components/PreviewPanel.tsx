import {
  WasmBackend,
  type BackendPhase,
  type MeterSnapshot,
  type ValidationResult,
} from '@audio-playground/wasm-tools';
import { useCallback, useEffect, useRef, useState } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import type { ParamOverride } from '../lib/projectParams';

type InputMode = 'file' | 'microphone';

type Props = {
  entryProject: string;
  workspaceFiles: WorkspaceFile[];
  paramOverrides: ParamOverride[];
  selectedInstanceId: string | null;
};

const emptyMeter: MeterSnapshot = { peak: 0, rms: 0, frames: 0, valid: false };

function moduleUrl(file: string): string {
  return new URL(`wasm/${file}`, `${window.location.origin}${import.meta.env.BASE_URL}`).href;
}

export function PreviewPanel({ entryProject, workspaceFiles, paramOverrides, selectedInstanceId }: Props) {
  const [backend, setBackend] = useState<WasmBackend | null>(null);
  const [phase, setPhase] = useState<BackendPhase>('idle');
  const [diagnostic, setDiagnostic] = useState('WASM backend is initializing.');
  const [meter, setMeter] = useState<MeterSnapshot>(emptyMeter);
  const [bypass, setBypass] = useState(false);
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
  const firstOverride = paramOverrides[0];

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
        setDiagnostic('WASM control backend ready.');
      })
      .catch(error => {
        setPhase('error');
        setDiagnostic(error instanceof Error ? error.message : String(error));
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
  }, []);

  const syncWorkspace = useCallback(
    async (prepare: boolean) => {
      if (!backend || !contextRef.current) return false;
      const revision = revisionRef.current;
      if (validRevisionRef.current !== revision) {
        setPhase('validating');
        const validation: ValidationResult = await backend.replaceWorkspace({
          revision,
          entryProject,
          files: workspaceFiles.map(({ path, role, content }) => ({ path, role, content })),
        });
        if (revision !== revisionRef.current) return false;
        if (!validation.ok) {
          setPhase('error');
          setDiagnostic(validation.diagnostic.message || validation.diagnostic.code);
          return false;
        }
        validRevisionRef.current = revision;
        setDiagnostic(`Revision ${revision} validated.`);
      }
      if (prepare) {
        setPhase('preparing');
        await backend.prepare(revision, {
          sampleRate: Math.round(contextRef.current.sampleRate),
          blockFrames: 128,
        });
        if (revision !== revisionRef.current) return false;
        if (runningRef.current) await backend.commitPrepared(revision);
        if (revision !== revisionRef.current) return false;
        setDiagnostic(`Revision ${revision} prepared for live audio.`);
      }
      setPhase(runningRef.current ? 'running' : prepare ? 'ready' : 'idle');
      return true;
    },
    [backend, entryProject, workspaceFiles],
  );

  useEffect(() => {
    if (!backend) return;
    revisionRef.current += 1;
    backend.setCurrentRevision(revisionRef.current);
    const timer = window.setTimeout(() => {
      void syncWorkspace(runningRef.current).catch(error => {
        if (revisionRef.current < 1) return;
        setPhase('error');
        setDiagnostic(error instanceof Error ? error.message : String(error));
      });
    }, 300);
    return () => window.clearTimeout(timer);
  }, [backend, syncWorkspace, workspaceFiles]);

  useEffect(() => {
    if (!backend || !running) return;
    const timer = window.setInterval(() => setMeter(backend.getMeters()), 100);
    return () => window.clearInterval(timer);
  }, [backend, running]);

  useEffect(() => {
    if (!backend || !running) return;
    for (const override of paramOverrides) {
      const value = Number(override.value);
      if (Number.isFinite(value)) void backend.setParam(override.path, value).catch(() => undefined);
    }
  }, [backend, paramOverrides, running]);

  const compile = useCallback(async () => {
    try {
      await syncWorkspace(true);
    } catch (error) {
      setPhase('error');
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [syncWorkspace]);

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
    runningRef.current = false;
    setRunning(false);
    setPhase('ready');
    setMeter(emptyMeter);
  }, []);

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
      setPhase('error');
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [audioBuffer, audioFileName, backend, inputMode, stopPlayback, syncWorkspace]);

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
      setPhase('error');
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [stopPlayback]);

  const reset = useCallback(async () => {
    if (!backend || !running) return;
    try {
      await backend.reset();
      setDiagnostic(`Revision ${revisionRef.current} runtime reset.`);
    } catch (error) {
      setPhase('error');
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [backend, running]);

  const sendFirstParam = useCallback(async () => {
    if (!backend || !firstOverride) return;
    const value = Number(firstOverride.value);
    if (!Number.isFinite(value)) return;
    try {
      await backend.setParam(firstOverride.path, value);
      setDiagnostic(`Updated ${firstOverride.path}.`);
    } catch (error) {
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [backend, firstOverride]);

  const toggleBypass = useCallback(async () => {
    if (!backend || !selectedInstanceId) return;
    const next = !bypass;
    try {
      await backend.setBypass(selectedInstanceId, next);
      setBypass(next);
      setDiagnostic(`${selectedInstanceId} bypass ${next ? 'enabled' : 'disabled'}.`);
    } catch (error) {
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [backend, bypass, selectedInstanceId]);

  return (
    <section className="inspector-block">
      <div className="inspector-block__label">Live Preview</div>
      <div className="preview-panel__state">
        <strong>{phase}</strong>
        <span>WASM AudioWorklet</span>
      </div>
      <p className="diagnostic-empty">{diagnostic}</p>

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
      </div>

      <div className="preview-panel__actions">
        <button className="btn btn--ghost" disabled={!running || !firstOverride} onClick={() => void sendFirstParam()} type="button">
          Send param
        </button>
        <button className="btn btn--ghost" disabled={!running || !selectedInstanceId} onClick={() => void toggleBypass()} type="button">
          Toggle bypass
        </button>
      </div>
    </section>
  );
}
