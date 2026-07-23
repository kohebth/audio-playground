import {
  WasmBackend,
  type AudioTraceReport,
  type AudioTraceStatus,
  type BackendPhase,
  type MeterSnapshot,
  type ValidationResult,
  type WasmDiagnostic,
} from '@audio-playground/wasm-tools';
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import { useLiveBypass } from '../lib/liveBypass';
import { createAudioTraceReport } from '../lib/audioTrace';
import {
  AUDIO_CALIBRATION_HINTS,
  EMPTY_AUDIO_CALIBRATION,
  browserMicrophoneCapability,
  calibrationCandidate,
  collectAudioDevices,
  createConfiguredAudioContext,
  describeAudioIssue,
  loadAudioIoPreference,
  readAudioRuntimeSettings,
  recommendedOutputDeviceId,
  requestMicrophoneStream,
  resolveAudioIoPreference,
  saveAudioIoPreference,
  selectCalibrationCandidate,
  type AudioCalibrationCandidate,
  type AudioCalibrationState,
  type AudioDeviceOption,
  type AudioIssue,
  type AudioIoPreference,
  type AudioRuntimeSettings,
  type ConfiguredAudioContext,
} from '../lib/audioIo';
import type { ParamOverride } from '../lib/projectParams';
import {
  createMonoAudioAsset,
  decodeBytesBase64,
  type ApgAudioAsset,
  type ProjectReadinessSnapshot,
  type StudioMode,
} from '../lib/projectPackage';
import { markPerfSpan, recordRuntimeSnapshot } from '../lib/perfTelemetry';

type InputMode = 'file' | 'microphone';

type Props = {
  entryProject: string;
  workspaceFiles: WorkspaceFile[];
  paramOverrides: ParamOverride[];
  compact?: boolean;
  onRuntimeReady?: () => void;
  onSaveWorkspace?: () => void;
  studioMode: StudioMode;
  packagedAudio: ApgAudioAsset[];
  onAudioAssetChange?: (asset: ApgAudioAsset | null) => void;
  onReadinessUpdate?: (update: Partial<ProjectReadinessSnapshot>) => void;
};

const emptyMeter: MeterSnapshot = {
  peak: 0,
  rms: 0,
  frames: 0,
  valid: false,
  activeRevision: 0,
  underruns: 0,
  callbackDeadlineMisses: 0,
  maxCallbackMs: 0,
};

function moduleUrl(file: string): string {
  return new URL(`wasm/${file}`, `${window.location.origin}${import.meta.env.BASE_URL}`).href;
}

function outputPathLatency(settings: AudioRuntimeSettings): number | null {
  return settings.baseLatencyMs !== null || settings.outputLatencyMs !== null
    ? (settings.baseLatencyMs ?? 0) + (settings.outputLatencyMs ?? 0)
    : null;
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

export function PreviewPanel({
  entryProject,
  workspaceFiles,
  paramOverrides,
  compact = false,
  onRuntimeReady,
  onSaveWorkspace,
  studioMode,
  packagedAudio,
  onAudioAssetChange,
  onReadinessUpdate,
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
  const [audioTraceStatus, setAudioTraceStatus] = useState<AudioTraceStatus>('idle');
  const [audioTraceProgress, setAudioTraceProgress] = useState(0);
  const [audioTraceReport, setAudioTraceReport] = useState<AudioTraceReport | null>(null);
  const [audioDevices, setAudioDevices] = useState<AudioDeviceOption[]>([]);
  const [audioIoPreference, setAudioIoPreference] = useState<AudioIoPreference>(() => (
    loadAudioIoPreference(window.localStorage)
  ));
  const [audioRuntimeSettings, setAudioRuntimeSettings] = useState<AudioRuntimeSettings | null>(null);
  const [audioCalibration, setAudioCalibration] = useState<AudioCalibrationState>(EMPTY_AUDIO_CALIBRATION);
  const [audioIssue, setAudioIssue] = useState<AudioIssue | null>(null);
  const [bypassByInstance, setBypassByInstance] = useState<Record<string, boolean>>({});
  const [muted, setMuted] = useState(false);
  const [running, setRunning] = useState(false);
  const [inputMode, setInputMode] = useState<InputMode>('microphone');
  const [audioBuffer, setAudioBuffer] = useState<AudioBuffer | null>(null);
  const [audioFileName, setAudioFileName] = useState('No audio file selected');
  const revisionRef = useRef(0);
  const validRevisionRef = useRef(0);
  const validatedBackendRevisionsRef = useRef(new WeakMap<WasmBackend, number>());
  const synchronizedWorkspaceRef = useRef({ entryProject, workspaceFiles });
  const backendRef = useRef<WasmBackend | null>(null);
  const runningRef = useRef(false);
  const contextRef = useRef<AudioContext | null>(null);
  const outputRoutingRef = useRef<ConfiguredAudioContext['outputRouting']>('default');
  const outputRoutingWarningRef = useRef<string | null>(null);
  const audioIoPreferenceRef = useRef(audioIoPreference);
  const inputModeRef = useRef<InputMode>('microphone');
  const audioIssueSequenceRef = useRef(0);
  const audioCalibrationTokenRef = useRef(0);
  const audioReconfigureQueueRef = useRef<Promise<void>>(Promise.resolve());
  const streamRef = useRef<MediaStream | null>(null);
  const inputRef = useRef<MediaStreamAudioSourceNode | null>(null);
  const fileSourceRef = useRef<AudioBufferSourceNode | null>(null);
  const meterTimerActiveRef = useRef(false);
  const latencyTimerActiveRef = useRef(false);
  const audioTracePollingActiveRef = useRef(false);
  const audioTraceTokenRef = useRef(0);
  const syncQueueRef = useRef<Promise<void>>(Promise.resolve());
  const previousOverridesRef = useRef<Map<string, ParamOverride>>(new Map());
  const paramControlQueueRef = useRef<Map<string, { draining: boolean; pendingValue: number | null }>>(new Map());
  const bypassByInstanceRef = useRef(bypassByInstance);
  const mutedRef = useRef(muted);
  const paramOverridesRef = useRef(paramOverrides);
  const firstOverride = paramOverrides[0];
  const microphoneCapability = useMemo(() => browserMicrophoneCapability(), []);
  const microphoneBlocked = inputMode === 'microphone' && !microphoneCapability.available;
  const microphoneIssueHidden = inputMode === 'file' && audioIssue?.source === 'microphone';
  const displayedAudioIssue = microphoneIssueHidden
    ? null
    : audioIssue ?? (microphoneBlocked ? microphoneCapability.issue : null);
  const transportPhase: BackendPhase = displayedAudioIssue
    ? 'error'
    : microphoneIssueHidden && phase === 'error'
      ? backend ? 'ready' : 'idle'
      : phase;
  const transportDiagnostic = displayedAudioIssue?.message ?? (
    microphoneIssueHidden ? 'Audio file input is ready.' : diagnostic
  );
  const refreshBackendState = useCallback((clearDiagnostic = false) => {
    const instance = backendRef.current;
    if (!instance) return;
    const next = instance.getState();
    if (next.lastError) setBackendDiagnostic(next.lastError);
    else if (clearDiagnostic) setBackendDiagnostic(null);
  }, []);

  const clearAudioIssue = useCallback(() => setAudioIssue(null), []);

  const reportAudioIssue = useCallback((
    error: unknown,
    errorPhase: string,
    source: AudioIssue['source'],
  ) => {
    audioIssueSequenceRef.current += 1;
    setAudioIssue(describeAudioIssue(
      error,
      errorPhase,
      source,
      `audio-${audioIssueSequenceRef.current}`,
    ));
  }, []);

  const reportError = useCallback((error: unknown, errorPhase: string) => {
    const readinessPhase = errorPhase === 'initialize' || errorPhase === 'compile' || errorPhase === 'synchronize';
    const known = readinessPhase ? backendRef.current?.getLastError() : null;
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
    if (readinessPhase) {
      onReadinessUpdate?.({
        checkedAt: new Date().toISOString(),
        preview: 'blocked',
        diagnostics: [{ code: detail.code, path: detail.path, message: detail.message || detail.code }],
      });
    } else {
      const microphonePhase = inputModeRef.current === 'microphone'
        && (errorPhase === 'start' || errorPhase === 'latency-probe' || errorPhase.startsWith('audio-'));
      reportAudioIssue(error, errorPhase, microphonePhase ? 'microphone' : 'audio-engine');
    }
  }, [onReadinessUpdate, refreshBackendState, reportAudioIssue]);

  const createBackendSession = useCallback(async (preference: AudioIoPreference) => {
    const configured = await createConfiguredAudioContext(preference);
    try {
      const instance = await WasmBackend.create({
        controlModuleUrl: moduleUrl('apg_control.mjs'),
        processorModuleUrl: moduleUrl('apg_processor.mjs'),
        processorWasmUrl: moduleUrl('apg_processor.wasm'),
        processorWorkletUrl: moduleUrl('processor.worklet.js'),
        audioContext: configured.context,
      });
      return { ...configured, backend: instance };
    } catch (error) {
      await configured.context.close();
      throw error;
    }
  }, []);

  const refreshRuntimeSettings = useCallback((stream: MediaStream | null = streamRef.current) => {
    const context = contextRef.current;
    if (!context) return null;
    const settings = readAudioRuntimeSettings(
      context,
      stream,
      outputRoutingRef.current,
      outputRoutingWarningRef.current,
    );
    setAudioRuntimeSettings(settings);
    setLatencyMs(outputPathLatency(settings));
    setCaptureLatency(settings.captureLatencyMs);
    return settings;
  }, []);

  const refreshAudioDevices = useCallback(async () => {
    if (!navigator.mediaDevices?.enumerateDevices) return [];
    const devices = collectAudioDevices(await navigator.mediaDevices.enumerateDevices());
    const resolved = resolveAudioIoPreference(audioIoPreferenceRef.current, devices);
    audioIoPreferenceRef.current = resolved;
    setAudioIoPreference(resolved);
    setAudioDevices(devices);
    saveAudioIoPreference(window.localStorage, resolved);
    return devices;
  }, []);

  useEffect(() => {
    audioIoPreferenceRef.current = audioIoPreference;
  }, [audioIoPreference]);

  useEffect(() => {
    inputModeRef.current = inputMode;
  }, [inputMode]);

  useEffect(() => {
    bypassByInstanceRef.current = bypassByInstance;
  }, [bypassByInstance]);

  useEffect(() => {
    mutedRef.current = muted;
  }, [muted]);

  useEffect(() => {
    paramOverridesRef.current = paramOverrides;
  }, [paramOverrides]);

  useEffect(() => {
    let disposed = false;
    void createBackendSession(audioIoPreferenceRef.current)
      .then(session => {
        if (disposed) {
          void session.backend.destroy().finally(() => session.context.close());
          return;
        }
        backendRef.current = session.backend;
        contextRef.current = session.context;
        outputRoutingRef.current = session.outputRouting;
        outputRoutingWarningRef.current = session.outputRoutingWarning;
        setBackend(session.backend);
        setAudioBuffer(createDefaultPreviewBuffer(session.context));
        setAudioFileName('test_tone.wav');
        refreshRuntimeSettings(null);
        setDiagnostic('Audio engine ready.');
        void refreshAudioDevices();
        onRuntimeReady?.();
      })
      .catch(error => {
        reportError(error, 'initialize');
        onRuntimeReady?.();
      });
    return () => {
      disposed = true;
      runningRef.current = false;
      audioTraceTokenRef.current += 1;
      audioCalibrationTokenRef.current += 1;
      audioTracePollingActiveRef.current = false;
      streamRef.current?.getTracks().forEach(track => track.stop());
      inputRef.current?.disconnect();
      fileSourceRef.current?.disconnect();
      const instance = backendRef.current;
      const context = contextRef.current;
      backendRef.current = null;
      contextRef.current = null;
      if (instance) void instance.destroy().finally(() => context?.close());
      else void context?.close();
    };
  }, [createBackendSession, onRuntimeReady, refreshAudioDevices, refreshRuntimeSettings, reportError]);

  useEffect(() => {
    if (!backend || !contextRef.current || studioMode !== 'pro' || packagedAudio.length === 0) return;
    const asset = packagedAudio[0];
    if (audioFileName === asset.name) return;
    let cancelled = false;
    const bytes = decodeBytesBase64(asset.data);
    const encoded = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer;
    void contextRef.current.decodeAudioData(encoded).then(decoded => {
      if (cancelled) return;
      if (decoded.numberOfChannels !== 1) throw new Error('Packaged audio must be mono.');
      setAudioBuffer(decoded);
      setAudioFileName(asset.name);
      setDiagnostic(`${asset.name} restored from this project.`);
    }).catch(error => {
      if (!cancelled) reportError(error, 'decode');
    });
    return () => { cancelled = true; };
  }, [audioFileName, backend, packagedAudio, reportError, studioMode]);

  useEffect(() => {
    const mediaDevices = navigator.mediaDevices;
    if (!mediaDevices?.addEventListener) return;
    const onDeviceChange = () => void refreshAudioDevices();
    mediaDevices.addEventListener('devicechange', onDeviceChange);
    return () => mediaDevices.removeEventListener('devicechange', onDeviceChange);
  }, [refreshAudioDevices]);

  const synchronizeBackend = useCallback(async (
    instance: WasmBackend,
    context: AudioContext,
    prepare: boolean,
    commit: boolean,
    announce = true,
  ) => {
    const revision = revisionRef.current;
    instance.setCurrentRevision(revision);
    if (validatedBackendRevisionsRef.current.get(instance) !== revision) {
      if (announce) {
        setPhase('validating');
      }
      const validationPromise = markPerfSpan('runtime.validate.workspace', () => instance.replaceWorkspace({
        revision,
        entryProject,
        files: workspaceFiles.map(({ path, role, content }) => ({ path, role, content })),
      }), { revision });
      const validation: ValidationResult = await validationPromise;
      if (revision !== revisionRef.current) return false;
      if (!validation.ok) {
        if (announce) {
          setPhase('error');
          setBackendDiagnostic(validation.diagnostic);
          setDiagnostic(validation.diagnostic.message || validation.diagnostic.code);
        }
        onReadinessUpdate?.({
          checkedAt: new Date().toISOString(),
          validation: 'blocked',
          preview: 'blocked',
          diagnostics: [{
            code: validation.diagnostic.code,
            path: validation.diagnostic.path,
            message: validation.diagnostic.message || validation.diagnostic.code,
          }],
        });
        return false;
      }
      onReadinessUpdate?.({ checkedAt: new Date().toISOString(), validation: 'ready', diagnostics: [] });
      validRevisionRef.current = revision;
      validatedBackendRevisionsRef.current.set(instance, revision);
      if (announce) setDiagnostic('Project validated.');
    }
    if (prepare) {
      if (announce) setPhase('preparing');
      const preparePromise = markPerfSpan('runtime.prepare.workspace', () => instance.prepare(revision, {
        sampleRate: Math.round(context.sampleRate),
        blockFrames: 128,
      }), { revision });
      await preparePromise;
      if (revision !== revisionRef.current) return false;
      if (commit) {
        await markPerfSpan('runtime.commit.workspace', () => instance.commitPrepared(revision), { revision });
      }
      if (revision !== revisionRef.current) return false;
      if (announce) setDiagnostic('Project prepared for live audio.');
      onReadinessUpdate?.({ checkedAt: new Date().toISOString(), preview: 'ready', diagnostics: [] });
    }
    if (announce) {
      const prepared = instance.getState().preparedRevision === revision;
      setPhase(runningRef.current || commit ? 'running' : prepare || prepared ? 'ready' : 'idle');
    }
    return true;
  }, [entryProject, onReadinessUpdate, workspaceFiles]);

  const syncWorkspace = useCallback(
    (prepare: boolean) => {
      const synchronize = async () => {
        const context = contextRef.current;
        if (!backend || !context) return false;
        const synchronized = await synchronizeBackend(backend, context, prepare, runningRef.current);
        refreshBackendState(true);
        return synchronized;
      };
      const instrumented = () => markPerfSpan('runtime.sync.workspace', synchronize, { revision: revisionRef.current });
      const result = syncQueueRef.current.then(instrumented, instrumented);
      syncQueueRef.current = result.then(() => undefined, () => undefined);
      return result;
    },
    [backend, refreshBackendState, synchronizeBackend],
  );

  useEffect(() => {
    if (!backend) return;
    const previousWorkspace = synchronizedWorkspaceRef.current;
    const workspaceChanged = previousWorkspace.entryProject !== entryProject
      || previousWorkspace.workspaceFiles !== workspaceFiles;
    if (revisionRef.current === 0) revisionRef.current = 1;
    else if (workspaceChanged) revisionRef.current += 1;
    synchronizedWorkspaceRef.current = { entryProject, workspaceFiles };
    backend.setCurrentRevision(revisionRef.current);
    refreshBackendState(true);
    const timer = window.setTimeout(() => {
      void syncWorkspace(runningRef.current).catch(error => {
        if (revisionRef.current < 1) return;
        reportError(error, 'synchronize');
      });
    }, 300);
    return () => window.clearTimeout(timer);
  }, [backend, entryProject, refreshBackendState, reportError, syncWorkspace, workspaceFiles]);

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
    const refreshLatency = () => refreshRuntimeSettings();
    refreshLatency();
    latencyTimerActiveRef.current = true;
    const timer = window.setInterval(refreshLatency, 500);
    return () => {
      window.clearInterval(timer);
      latencyTimerActiveRef.current = false;
    };
  }, [refreshRuntimeSettings, running]);

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
            if (runningRef.current) reportError(error, 'control');
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
    audioTraceTokenRef.current += 1;
    audioTracePollingActiveRef.current = false;
    setAudioTraceStatus(current => current === 'running' ? 'idle' : current);
    setAudioTraceProgress(current => current < 1 ? 0 : current);
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
    refreshRuntimeSettings(null);
    setMeasuredLatencyMs(null);
  }, [refreshBackendState, refreshRuntimeSettings]);

  useEffect(() => {
    if (studioMode !== 'simple' || inputMode === 'microphone') return;
    if (runningRef.current) void stopPlayback().finally(() => setInputMode('microphone'));
    else setInputMode('microphone');
  }, [inputMode, stopPlayback, studioMode]);

  const destroyCurrentSession = useCallback(async () => {
    const instance = backendRef.current;
    const context = contextRef.current;
    backendRef.current = null;
    contextRef.current = null;
    setBackend(null);
    setAudioRuntimeSettings(null);
    setLatencyMs(null);
    setCaptureLatency(null);
    if (instance) await instance.destroy();
    if (context && context.state !== 'closed') await context.close();
  }, []);

  const applyRuntimeControls = useCallback(async (instance: WasmBackend, forceMute?: boolean) => {
    for (const override of paramOverridesRef.current) {
      const value = Number(override.value);
      if (Number.isFinite(value)) await instance.setParam(override.path, value);
    }
    for (const [instanceId, enabled] of Object.entries(bypassByInstanceRef.current)) {
      await instance.setBypass(instanceId, enabled);
    }
    const nextMute = forceMute ?? mutedRef.current;
    if (nextMute) await instance.setMute(true);
    previousOverridesRef.current = new Map(paramOverridesRef.current.map(override => [override.path, override]));
  }, []);

  const installSession = useCallback((session: Awaited<ReturnType<typeof createBackendSession>>) => {
    backendRef.current = session.backend;
    contextRef.current = session.context;
    outputRoutingRef.current = session.outputRouting;
    outputRoutingWarningRef.current = session.outputRoutingWarning;
    setBackend(session.backend);
  }, []);

  const activateSession = useCallback(async (
    session: Awaited<ReturnType<typeof createBackendSession>>,
    shouldRun: boolean,
    mode: InputMode,
    preference: AudioIoPreference,
  ) => {
    let stream: MediaStream | null = null;
    let input: AudioNode | null = null;
    let fileSource: AudioBufferSourceNode | null = null;
    try {
      const synchronized = await synchronizeBackend(session.backend, session.context, true, false, false);
      if (!synchronized) throw new Error('The current workspace could not be prepared for audio.');
      if (shouldRun) {
        if (mode === 'microphone') {
          stream = await requestMicrophoneStream(preference, session.context.sampleRate);
          input = session.context.createMediaStreamSource(stream);
        } else {
          if (!audioBuffer) throw new Error('Select an audio file before starting playback.');
          fileSource = session.context.createBufferSource();
          fileSource.buffer = audioBuffer;
          input = fileSource;
        }
        await session.backend.start({ input });
        await applyRuntimeControls(session.backend);
      }

      installSession(session);
      streamRef.current = stream;
      inputRef.current = mode === 'microphone' ? input as MediaStreamAudioSourceNode | null : null;
      fileSourceRef.current = fileSource;
      runningRef.current = shouldRun;
      setRunning(shouldRun);
      setPhase(shouldRun ? 'running' : 'ready');
      const settings = readAudioRuntimeSettings(
        session.context,
        stream,
        session.outputRouting,
        session.outputRoutingWarning,
      );
      setAudioRuntimeSettings(settings);
      setLatencyMs(outputPathLatency(settings));
      setCaptureLatency(settings.captureLatencyMs);
      setMeasuredLatencyMs(null);
      if (fileSource) {
        fileSource.onended = () => void stopPlayback();
        fileSource.start();
      }
    } catch (error) {
      input?.disconnect();
      fileSource?.disconnect();
      stream?.getTracks().forEach(track => track.stop());
      await session.backend.destroy();
      if (session.context.state !== 'closed') await session.context.close();
      throw error;
    }
  }, [applyRuntimeControls, audioBuffer, installSession, stopPlayback, synchronizeBackend]);

  const reconfigureAudio = useCallback((preference: AudioIoPreference) => {
    const reconfigure = async () => {
      const previousPreference = audioIoPreferenceRef.current;
      const wasRunning = runningRef.current;
      const mode = inputMode;
      setPhase('preparing');
      setDiagnostic('Reconfiguring audio I/O...');
      try {
        await stopPlayback();
        await destroyCurrentSession();
        const session = await createBackendSession(preference);
        await activateSession(session, wasRunning, mode, preference);
        audioIoPreferenceRef.current = preference;
        setAudioIoPreference(preference);
        saveAudioIoPreference(window.localStorage, preference);
        void refreshAudioDevices();
        clearAudioIssue();
        setDiagnostic(wasRunning ? 'Audio I/O reconfigured and playback restored.' : 'Audio I/O reconfigured.');
      } catch (error) {
        const failedMessage = error instanceof Error ? error.message : String(error);
        try {
          await destroyCurrentSession();
          const restored = await createBackendSession(previousPreference);
          await activateSession(restored, wasRunning, mode, previousPreference);
          audioIoPreferenceRef.current = previousPreference;
          setAudioIoPreference(previousPreference);
          setDiagnostic(`Audio I/O change failed; previous configuration restored. ${failedMessage}`);
          reportAudioIssue(error, 'audio-io', mode === 'microphone' ? 'microphone' : 'audio-engine');
        } catch (restoreError) {
          reportError(restoreError, 'audio-io-restore');
        }
      }
    };
    const result = audioReconfigureQueueRef.current.then(reconfigure, reconfigure);
    audioReconfigureQueueRef.current = result.then(() => undefined, () => undefined);
    return result;
  }, [activateSession, clearAudioIssue, createBackendSession, destroyCurrentSession, inputMode, refreshAudioDevices, reportAudioIssue, reportError, stopPlayback]);

  const selectAudioInput = useCallback(async (deviceId: string) => {
    const current = audioIoPreferenceRef.current;
    const preference = {
      ...current,
      inputDeviceId: deviceId,
      outputDeviceId: recommendedOutputDeviceId(audioDevices, deviceId, current.outputDeviceId),
    };
    await reconfigureAudio(preference);
  }, [audioDevices, reconfigureAudio]);

  const selectAudioOutput = useCallback(async (deviceId: string) => {
    await reconfigureAudio({ ...audioIoPreferenceRef.current, outputDeviceId: deviceId });
  }, [reconfigureAudio]);

  const runCalibrationCandidate = useCallback(async (
    preference: AudioIoPreference,
    token: number,
  ): Promise<AudioCalibrationCandidate> => {
    const session = await createBackendSession(preference);
    let stream: MediaStream | null = null;
    let input: MediaStreamAudioSourceNode | null = null;
    try {
      const synchronized = await synchronizeBackend(session.backend, session.context, true, false, false);
      if (!synchronized) throw new Error('The workspace could not be prepared for calibration.');
      stream = await requestMicrophoneStream(preference, session.context.sampleRate);
      input = session.context.createMediaStreamSource(stream);
      await session.backend.start({ input });
      await applyRuntimeControls(session.backend, true);
      await new Promise(resolve => window.setTimeout(resolve, 500));
      if (audioCalibrationTokenRef.current !== token) throw new Error('Calibration cancelled.');
      await session.backend.startAudioTrace();
      let trace = await session.backend.pollAudioTrace();
      while (trace.status !== 'complete') {
        await new Promise(resolve => window.setTimeout(resolve, 250));
        if (audioCalibrationTokenRef.current !== token) throw new Error('Calibration cancelled.');
        trace = await session.backend.pollAudioTrace();
      }
      const runtime = readAudioRuntimeSettings(
        session.context,
        stream,
        session.outputRouting,
        session.outputRoutingWarning,
      );
      return calibrationCandidate(preference.latencyHint, runtime, trace);
    } finally {
      input?.disconnect();
      stream?.getTracks().forEach(track => track.stop());
      await session.backend.destroy();
      if (session.context.state !== 'closed') await session.context.close();
    }
  }, [applyRuntimeControls, createBackendSession, synchronizeBackend]);

  const calibrateAudio = useCallback(async () => {
    if (
      inputMode !== 'microphone'
      || !microphoneCapability.available
      || audioCalibration.status === 'running'
    ) return;
    const token = audioCalibrationTokenRef.current + 1;
    audioCalibrationTokenRef.current = token;
    const previousPreference = audioIoPreferenceRef.current;
    const wasRunning = runningRef.current;
    const candidates: AudioCalibrationCandidate[] = [];
    setAudioCalibration({ status: 'running', progress: 0, candidates, selectedHint: null, error: null });
    setDiagnostic('Calibrating audio latency...');
    try {
      await stopPlayback();
      await destroyCurrentSession();
      for (let index = 0; index < AUDIO_CALIBRATION_HINTS.length; index += 1) {
        const latencyHint = AUDIO_CALIBRATION_HINTS[index];
        const candidate = await runCalibrationCandidate({ ...previousPreference, latencyHint }, token);
        candidates.push(candidate);
        setAudioCalibration({
          status: 'running',
          progress: (index + 1) / AUDIO_CALIBRATION_HINTS.length,
          candidates: [...candidates],
          selectedHint: null,
          error: null,
        });
      }
      const selected = selectCalibrationCandidate(candidates);
      if (!selected) throw new Error('No latency candidate completed without underruns or deadline misses.');
      const preference = { ...previousPreference, latencyHint: selected.requestedHint };
      const session = await createBackendSession(preference);
      await activateSession(session, wasRunning, 'microphone', preference);
      audioIoPreferenceRef.current = preference;
      setAudioIoPreference(preference);
      saveAudioIoPreference(window.localStorage, preference);
      setAudioCalibration({
        status: 'complete',
        progress: 1,
        candidates,
        selectedHint: selected.requestedHint,
        error: null,
      });
      void refreshAudioDevices();
      clearAudioIssue();
      setDiagnostic('Latency calibration complete. Run the chirp to verify round-trip latency.');
    } catch (error) {
      if (audioCalibrationTokenRef.current !== token) return;
      const message = error instanceof Error ? error.message : String(error);
      try {
        await destroyCurrentSession();
        const restored = await createBackendSession(previousPreference);
        await activateSession(restored, wasRunning, 'microphone', previousPreference);
        audioIoPreferenceRef.current = previousPreference;
        setAudioIoPreference(previousPreference);
        setAudioCalibration({ status: 'error', progress: 0, candidates, selectedHint: null, error: message });
        setDiagnostic(`Calibration failed; previous configuration restored. ${message}`);
        reportAudioIssue(error, 'audio-calibration', 'microphone');
      } catch (restoreError) {
        setAudioCalibration({ status: 'error', progress: 0, candidates, selectedHint: null, error: message });
        reportError(restoreError, 'audio-calibration-restore');
      }
    }
  }, [activateSession, audioCalibration.status, clearAudioIssue, createBackendSession, destroyCurrentSession, inputMode, microphoneCapability.available, refreshAudioDevices, reportAudioIssue, reportError, runCalibrationCandidate, stopPlayback]);

  const start = useCallback(async () => {
    if (!backend || (inputMode === 'microphone' && !microphoneCapability.available)) return;
    try {
      if (validRevisionRef.current !== revisionRef.current && !(await syncWorkspace(true))) return;
      else if (backend.getState().preparedRevision !== revisionRef.current && !(await syncWorkspace(true))) return;
      const context = contextRef.current;
      if (!context) return;
      let input: AudioNode;
      if (inputMode === 'file') {
        refreshRuntimeSettings(null);
        if (!audioBuffer) throw new Error('Select an audio file before starting playback.');
        const source = context.createBufferSource();
        source.buffer = audioBuffer;
        fileSourceRef.current = source;
        input = source;
      } else {
        const stream = await requestMicrophoneStream(audioIoPreferenceRef.current, context.sampleRate);
        const source = context.createMediaStreamSource(stream);
        streamRef.current = stream;
        inputRef.current = source;
        input = source;
        refreshRuntimeSettings(stream);
        void refreshAudioDevices();
      }
      await markPerfSpan('runtime.start', () => backend.start({ input }));
      refreshBackendState(true);
      runningRef.current = true;
      setRunning(true);
      setPhase('running');
      clearAudioIssue();
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
      await stopPlayback().catch(() => undefined);
      reportError(error, 'start');
    }
  }, [audioBuffer, audioFileName, backend, clearAudioIssue, inputMode, microphoneCapability.available, refreshAudioDevices, refreshBackendState, refreshRuntimeSettings, reportError, stopPlayback, syncWorkspace]);

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
      if (decoded.numberOfChannels !== 1) {
        throw new Error('Stereo and multi-channel files are not supported. Choose a mono audio file.');
      }
      setAudioBuffer(decoded);
      setAudioFileName(file.name);
      setDiagnostic(`${file.name} ready (${decoded.duration.toFixed(2)} s).`);
      onAudioAssetChange?.(createMonoAudioAsset({
        id: packagedAudio[0]?.id ?? globalThis.crypto?.randomUUID?.() ?? `audio-${Date.now()}`,
        name: file.name,
        mimeType: file.type || 'application/octet-stream',
        channels: decoded.numberOfChannels,
        sampleRate: decoded.sampleRate,
        durationSeconds: decoded.duration,
        bytes: new Uint8Array(encoded),
      }));
    } catch (error) {
      setAudioBuffer(null);
      setAudioFileName('No audio file selected');
      reportError(error, 'decode');
    }
  }, [onAudioAssetChange, packagedAudio, reportError, stopPlayback]);

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

  const clearAudioTrace = useCallback(() => {
    setAudioTraceReport(null);
    setAudioTraceProgress(0);
    setAudioTraceStatus('idle');
  }, []);

  const profileAudio = useCallback(async () => {
    if (!backend || !running || inputMode !== 'microphone' || audioTracePollingActiveRef.current) return;
    const token = audioTraceTokenRef.current + 1;
    audioTraceTokenRef.current = token;
    setAudioTraceReport(null);
    setAudioTraceProgress(0);
    setAudioTraceStatus('running');
    audioTracePollingActiveRef.current = true;
    try {
      await backend.startAudioTrace();
      while (audioTraceTokenRef.current === token && runningRef.current) {
        await new Promise(resolve => window.setTimeout(resolve, 250));
        if (audioTraceTokenRef.current !== token || !runningRef.current) return;
        const trace = await backend.pollAudioTrace();
        setAudioTraceProgress(Math.min(1, trace.durationMs > 0 ? trace.elapsedMs / trace.durationMs : 0));
        if (trace.status !== 'complete') continue;
        const runtime = refreshRuntimeSettings();
        setAudioTraceReport(createAudioTraceReport(trace, {
          captureLatencyMs: runtime?.captureLatencyMs ?? null,
          baseLatencyMs: runtime?.baseLatencyMs ?? null,
          outputLatencyMs: runtime?.outputLatencyMs ?? null,
          acousticLoopbackMs: measuredLatencyMs,
        }));
        setAudioTraceProgress(1);
        setAudioTraceStatus('complete');
        setDiagnostic('Audio latency profile complete.');
        return;
      }
    } catch (error) {
      if (audioTraceTokenRef.current === token) {
        setAudioTraceStatus('idle');
        reportError(error, 'audio-trace');
      }
    } finally {
      if (audioTraceTokenRef.current === token) audioTracePollingActiveRef.current = false;
    }
  }, [backend, inputMode, measuredLatencyMs, refreshRuntimeSettings, reportError, running]);

  useEffect(() => {
    setController({
      running,
      latencyMs,
      captureLatencyMs: captureLatency,
      measuredLatencyMs,
      measuringLatency,
      inputMode,
      audioTraceStatus,
      audioTraceProgress,
      audioTraceReport,
      audioDevices,
      audioIoPreference,
      audioRuntimeSettings,
      audioCalibration,
      audioIssue,
      microphoneCapability,
      bypassByInstance,
      setBypass: setInstanceBypass,
      profileAudio,
      clearAudioIssue,
      clearAudioTrace,
      refreshAudioDevices,
      selectAudioInput,
      selectAudioOutput,
      calibrateAudio,
      measureAcousticLatency,
    });
  }, [audioCalibration, audioDevices, audioIoPreference, audioIssue, audioRuntimeSettings, audioTraceProgress, audioTraceReport, audioTraceStatus, bypassByInstance, calibrateAudio, captureLatency, clearAudioIssue, clearAudioTrace, inputMode, latencyMs, measureAcousticLatency, measuredLatencyMs, measuringLatency, microphoneCapability, profileAudio, refreshAudioDevices, running, selectAudioInput, selectAudioOutput, setController, setInstanceBypass]);

  useEffect(() => () => setController(null), [setController]);

  useEffect(() => {
    if (!backend) return;
    const state = backend.getState();
    const resources = backend.getResourceSnapshot();
    recordRuntimeSnapshot({
      phase,
      activeRevision: state.activeRevision,
      preparedRevision: state.preparedRevision,
      meter: {
        frames: meter.frames,
        valid: meter.valid,
        underruns: meter.underruns,
        callbackDeadlineMisses: meter.callbackDeadlineMisses,
        maxCallbackMs: meter.maxCallbackMs,
      },
      resources: {
        ...resources,
        streamTracks: streamRef.current?.getTracks().length ?? 0,
        inputNodeActive: inputRef.current !== null,
        fileSourceActive: fileSourceRef.current !== null,
        meterTimerActive: meterTimerActiveRef.current,
        latencyTimerActive: latencyTimerActiveRef.current,
        audioTracePollingActive: audioTracePollingActiveRef.current,
      },
      at: Date.now(),
    });
  }, [audioTraceStatus, backend, meter, phase, running]);

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

  const shortcutActionsRef = useRef({
    buildAndSave: handleBuildAndSave,
    mute: toggleMute,
    save: onSaveWorkspace,
    togglePlayback,
  });
  shortcutActionsRef.current = {
    buildAndSave: handleBuildAndSave,
    mute: toggleMute,
    save: onSaveWorkspace,
    togglePlayback,
  };

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.repeat) return;

      const target = event.target;
      if (target instanceof HTMLElement) {
        if (target.isContentEditable || target.closest('[contenteditable="true"]')) return;
        const tag = target.tagName.toLowerCase();
        if (tag === 'input' || tag === 'textarea' || tag === 'select' || tag === 'option') return;
      }

      const key = event.key.toLowerCase();
      if ((event.ctrlKey || event.metaKey) && key === 's') {
        event.preventDefault();
        shortcutActionsRef.current.save?.();
        return;
      }

      if (key === ' ' || event.code === 'Space') {
        event.preventDefault();
        shortcutActionsRef.current.togglePlayback();
        return;
      }

      if (key === 'm') {
        event.preventDefault();
        void shortcutActionsRef.current.mute();
        return;
      }

      if (key === 'b') {
        event.preventDefault();
        void shortcutActionsRef.current.buildAndSave();
      }
    };

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, []);

  return (
    <section className={compact ? 'transport-island preview-panel--compact' : 'inspector-block'}>
      {compact ? (
        <>
      <div className="transport-group">
            <button
              aria-label={running
                ? 'Stop preview'
                : microphoneBlocked
                  ? 'Microphone unavailable. Open Audio Playground over trusted HTTPS.'
                  : 'Start preview'}
              className={`transport-btn ${running ? 'active' : ''}`}
              data-testid="preview-start-stop"
              disabled={!backend || (!running && microphoneBlocked)}
              onClick={() => void togglePlayback()}
              title={microphoneBlocked ? 'Microphone requires a trusted HTTPS address' : running ? 'Stop preview' : 'Start preview'}
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
            {studioMode === 'pro' ? (
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
            ) : null}
            {studioMode === 'pro' && inputMode === 'file' && (
              <label className="transport-file">
                <span>{audioFileName}</span>
                <input accept="audio/*,.wav,.mp3,.flac,.ogg,.m4a,.aac" disabled={running} onChange={event => void chooseAudioFile(event.target.files?.[0])} type="file" />
              </label>
            )}
          </div>
          <div className={`transport-group${transportPhase === 'running' ? ' transport-group--running' : ''}`}>
            <span
              aria-label={`Audio engine ${transportPhase}`}
              className={`transport-state transport-state--${transportPhase}`}
            >
              {transportPhase}
            </span>
            <button className="transport-btn" disabled={!running} onClick={() => void toggleMute()} title={muted ? 'Unmute output' : 'Mute output'} type="button">
              <i className={`fa-solid ${muted ? 'fa-volume-xmark' : 'fa-volume-high'}`} aria-hidden="true" />
            </button>
          </div>
        </>
      ) : (
        <>
      <div className="inspector-block__label">Live Preview</div>
      <div className="preview-panel__state">
        <strong>{transportPhase}</strong>
        <span>Live engine</span>
      </div>
      <p className={`diagnostic-empty${transportPhase === 'error' ? ' diagnostic-empty--error' : ''}`}>{transportDiagnostic}</p>
      {backendDiagnostic && !microphoneIssueHidden && (
        <dl className="wasm-diagnostic" aria-label="WASM diagnostic">
          <div><dt>Code</dt><dd>{backendDiagnostic.code}</dd></div>
          <div><dt>Phase</dt><dd>{backendDiagnostic.phase}</dd></div>
          {backendDiagnostic.file && <div><dt>File</dt><dd>{backendDiagnostic.file}</dd></div>}
          {backendDiagnostic.path && <div><dt>Path</dt><dd>{backendDiagnostic.path}</dd></div>}
        </dl>
      )}

      <div className="preview-panel__mode" aria-label="Audio input mode" role="group">
        <button
          aria-pressed={inputMode === 'microphone'}
          disabled={running}
          onClick={() => setInputMode('microphone')}
          type="button"
        >
          Microphone
        </button>
        {studioMode === 'pro' ? (
          <button
            aria-pressed={inputMode === 'file'}
            disabled={running}
            onClick={() => setInputMode('file')}
            type="button"
          >
            Audio file
          </button>
        ) : null}
      </div>

      {studioMode === 'pro' && inputMode === 'file' && (
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
          disabled={!backend || (!running && microphoneBlocked)}
          onClick={() => void togglePlayback()}
          title={microphoneBlocked ? 'Microphone requires a trusted HTTPS address' : undefined}
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
        <div>
          <span>Deadline misses</span>
          <strong>{meter.callbackDeadlineMisses}</strong>
        </div>
        <div>
          <span>Max callback</span>
          <strong>{meter.maxCallbackMs.toFixed(1)} ms</strong>
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
