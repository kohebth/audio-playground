import {
  WasmBackend,
  type BackendPhase,
  type MeterSnapshot,
  type ValidationResult,
} from '@audio-playground/wasm-tools';
import { useCallback, useEffect, useRef, useState } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import type { ParamOverride } from '../lib/projectParams';

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
  const revisionRef = useRef(0);
  const validRevisionRef = useRef(0);
  const backendRef = useRef<WasmBackend | null>(null);
  const runningRef = useRef(false);
  const contextRef = useRef<AudioContext | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const inputRef = useRef<MediaStreamAudioSourceNode | null>(null);
  const firstOverride = paramOverrides[0];

  useEffect(() => {
    const context = new AudioContext();
    contextRef.current = context;
    let disposed = false;
    void WasmBackend.create({
      controlModuleUrl: moduleUrl('apg_control.mjs'),
      processorModuleUrl: moduleUrl('apg_processor.mjs'),
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
    const timer = window.setTimeout(() => {
      void syncWorkspace(running).catch(error => {
        if (revisionRef.current < 1) return;
        setPhase('error');
        setDiagnostic(error instanceof Error ? error.message : String(error));
      });
    }, 300);
    return () => window.clearTimeout(timer);
  }, [backend, running, syncWorkspace, workspaceFiles]);

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

  const start = useCallback(async () => {
    if (!backend) return;
    try {
      if (validRevisionRef.current !== revisionRef.current && !(await syncWorkspace(true))) return;
      else if (backend.getState().preparedRevision !== revisionRef.current && !(await syncWorkspace(true))) return;
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      const context = contextRef.current;
      if (!context) return;
      const input = context.createMediaStreamSource(stream);
      streamRef.current = stream;
      inputRef.current = input;
      await backend.start({ input });
      runningRef.current = true;
      setRunning(true);
      setPhase('running');
      setDiagnostic(`Revision ${revisionRef.current} is processing microphone input.`);
    } catch (error) {
      setPhase('error');
      setDiagnostic(error instanceof Error ? error.message : String(error));
    }
  }, [backend, syncWorkspace]);

  const stop = useCallback(async () => {
    await backend?.stop();
    runningRef.current = false;
    inputRef.current?.disconnect();
    inputRef.current = null;
    streamRef.current?.getTracks().forEach(track => track.stop());
    streamRef.current = null;
    setRunning(false);
    setPhase('ready');
    setMeter(emptyMeter);
  }, [backend]);

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
