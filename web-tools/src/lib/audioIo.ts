import type { AudioTraceSnapshot } from '@audio-playground/wasm-tools';

export const AUDIO_IO_STORAGE_KEY = 'apg.audio-io.v1';
export const AUDIO_CALIBRATION_HINTS = [0.0027, 0.0053, 0.0107, 'interactive'] as const;

export type AudioLatencyHint = number | 'interactive';
export type MicPathLatencySeverity = 'normal' | 'warning' | 'danger';

export type AudioIssue = {
  id: string;
  source: 'microphone' | 'audio-engine';
  phase: string;
  code: string;
  message: string;
  detail: string | null;
};

export function describeAudioIssue(
  error: unknown,
  phase: string,
  source: AudioIssue['source'],
  id: string,
): AudioIssue {
  const code = error instanceof DOMException || error instanceof Error
    ? error.name || 'APG_WEB_AUDIO_ERROR'
    : 'APG_WEB_AUDIO_ERROR';
  const detail = error instanceof Error ? error.message : String(error);
  const microphoneMessages: Record<string, string> = {
    NotAllowedError: 'Microphone access was denied. Allow microphone access for this site and try again.',
    NotFoundError: 'No microphone was found. Connect an input device and try again.',
    NotReadableError: 'The microphone is busy or unavailable. Close other audio apps and try again.',
    OverconstrainedError: 'The selected microphone is unavailable. Choose another input in Audio I/O.',
    SecurityError: 'Microphone access requires HTTPS or localhost.',
  };
  const message = source === 'microphone'
    ? microphoneMessages[code] ?? detail
    : detail;
  return {
    id,
    source,
    phase,
    code,
    message: message || 'The audio engine could not complete this action.',
    detail: detail && detail !== message ? detail : null,
  };
}

export function micPathLatencySeverity(latencyMs: number): MicPathLatencySeverity {
  if (latencyMs > 30) return 'danger';
  if (latencyMs > 20) return 'warning';
  return 'normal';
}

export type AudioDeviceOption = {
  deviceId: string;
  groupId: string;
  kind: 'audioinput' | 'audiooutput';
  label: string;
};

export type AudioIoPreference = {
  inputDeviceId: string;
  outputDeviceId: string;
  latencyHint: AudioLatencyHint;
};

export type AudioRuntimeSettings = {
  contextSampleRate: number;
  inputSampleRate: number | null;
  inputChannelCount: number | null;
  captureLatencyMs: number | null;
  baseLatencyMs: number | null;
  outputLatencyMs: number | null;
  estimatedPathLatencyMs: number | null;
  sampleRateMismatch: boolean;
  outputRouting: 'selected' | 'default';
  outputRoutingWarning: string | null;
};

export type AudioCalibrationCandidate = {
  requestedHint: AudioLatencyHint;
  runtime: AudioRuntimeSettings;
  trace: AudioTraceSnapshot;
  stable: boolean;
  rejectionReason: string | null;
};

export type AudioCalibrationState = {
  status: 'idle' | 'running' | 'complete' | 'error';
  progress: number;
  candidates: AudioCalibrationCandidate[];
  selectedHint: AudioLatencyHint | null;
  error: string | null;
};

export type ConfiguredAudioContext = {
  context: AudioContext;
  outputRouting: AudioRuntimeSettings['outputRouting'];
  outputRoutingWarning: string | null;
};

export const DEFAULT_AUDIO_IO_PREFERENCE: AudioIoPreference = {
  inputDeviceId: 'default',
  outputDeviceId: 'default',
  latencyHint: 'interactive',
};

export const EMPTY_AUDIO_CALIBRATION: AudioCalibrationState = {
  status: 'idle',
  progress: 0,
  candidates: [],
  selectedHint: null,
  error: null,
};

function isLatencyHint(value: unknown): value is AudioLatencyHint {
  return value === 'interactive' || (typeof value === 'number' && Number.isFinite(value) && value > 0);
}

export function loadAudioIoPreference(storage: Pick<Storage, 'getItem'>): AudioIoPreference {
  try {
    const encoded = storage.getItem(AUDIO_IO_STORAGE_KEY);
    if (!encoded) return DEFAULT_AUDIO_IO_PREFERENCE;
    const value = JSON.parse(encoded) as Partial<AudioIoPreference>;
    return {
      inputDeviceId: typeof value.inputDeviceId === 'string' && value.inputDeviceId ? value.inputDeviceId : 'default',
      outputDeviceId: typeof value.outputDeviceId === 'string' && value.outputDeviceId ? value.outputDeviceId : 'default',
      latencyHint: isLatencyHint(value.latencyHint) ? value.latencyHint : 'interactive',
    };
  } catch {
    return DEFAULT_AUDIO_IO_PREFERENCE;
  }
}

export function saveAudioIoPreference(storage: Pick<Storage, 'setItem'>, preference: AudioIoPreference): void {
  storage.setItem(AUDIO_IO_STORAGE_KEY, JSON.stringify(preference));
}

export function collectAudioDevices(devices: readonly MediaDeviceInfo[]): AudioDeviceOption[] {
  const counts = { audioinput: 0, audiooutput: 0 };
  const seen = new Set<string>();
  return devices.reduce<AudioDeviceOption[]>((options, device) => {
    if (device.kind !== 'audioinput' && device.kind !== 'audiooutput') return options;
    const deviceId = device.deviceId || 'default';
    const key = `${device.kind}:${deviceId}`;
    if (seen.has(key)) return options;
    seen.add(key);
    counts[device.kind] += 1;
    const fallback = device.kind === 'audioinput' ? 'Audio input' : 'Audio output';
    options.push({
      deviceId,
      groupId: device.groupId,
      kind: device.kind,
      label: device.label || (
        deviceId === 'default' ? `System default ${fallback.toLowerCase()}` : `${fallback} ${counts[device.kind]}`
      ),
    });
    return options;
  }, []);
}

function fallbackDeviceId(devices: readonly AudioDeviceOption[], kind: AudioDeviceOption['kind']): string {
  const available = devices.filter(device => device.kind === kind);
  return available.find(device => device.deviceId === 'default')?.deviceId ?? available[0]?.deviceId ?? 'default';
}

export function resolveAudioIoPreference(
  preference: AudioIoPreference,
  devices: readonly AudioDeviceOption[],
): AudioIoPreference {
  const inputExists = devices.some(device => device.kind === 'audioinput' && device.deviceId === preference.inputDeviceId);
  const outputExists = devices.some(device => device.kind === 'audiooutput' && device.deviceId === preference.outputDeviceId);
  return {
    ...preference,
    inputDeviceId: inputExists ? preference.inputDeviceId : fallbackDeviceId(devices, 'audioinput'),
    outputDeviceId: outputExists ? preference.outputDeviceId : fallbackDeviceId(devices, 'audiooutput'),
  };
}

export function recommendedOutputDeviceId(
  devices: readonly AudioDeviceOption[],
  inputDeviceId: string,
  currentOutputDeviceId: string,
): string {
  if (currentOutputDeviceId !== 'default') return currentOutputDeviceId;
  const input = devices.find(device => device.kind === 'audioinput' && device.deviceId === inputDeviceId);
  if (!input?.groupId) return currentOutputDeviceId;
  return devices.find(device => (
    device.kind === 'audiooutput' && device.groupId === input.groupId && device.deviceId !== 'default'
  ))?.deviceId ?? currentOutputDeviceId;
}

export function microphoneConstraints(preference: AudioIoPreference, sampleRate: number): MediaTrackConstraints {
  return {
    autoGainControl: false,
    channelCount: { ideal: 1 },
    deviceId: !preference.inputDeviceId || preference.inputDeviceId === 'default'
      ? undefined
      : { exact: preference.inputDeviceId },
    echoCancellation: false,
    latency: { ideal: 0 },
    noiseSuppression: false,
    sampleRate: { ideal: Math.round(sampleRate) },
  } as MediaTrackConstraints;
}

export async function createConfiguredAudioContext(preference: AudioIoPreference): Promise<ConfiguredAudioContext> {
  const sinkRequested = Boolean(preference.outputDeviceId) && preference.outputDeviceId !== 'default';
  const options = {
    latencyHint: preference.latencyHint,
    ...(sinkRequested ? { sinkId: preference.outputDeviceId } : {}),
  } as AudioContextOptions & { sinkId?: string };
  let context: AudioContext;
  try {
    context = new AudioContext(options);
  } catch (error) {
    if (!sinkRequested) throw error;
    context = new AudioContext({ latencyHint: preference.latencyHint });
    return {
      context,
      outputRouting: 'default',
      outputRoutingWarning: error instanceof Error ? error.message : 'The saved output device is unavailable.',
    };
  }
  if (!sinkRequested) return { context, outputRouting: 'default', outputRoutingWarning: null };

  const routedContext = context as AudioContext & { setSinkId?: (sinkId: string) => Promise<void> };
  if (!routedContext.setSinkId) {
    return {
      context,
      outputRouting: 'default',
      outputRoutingWarning: 'This browser cannot select an audio output device.',
    };
  }
  try {
    await routedContext.setSinkId(preference.outputDeviceId);
    return { context, outputRouting: 'selected', outputRoutingWarning: null };
  } catch (error) {
    return {
      context,
      outputRouting: 'default',
      outputRoutingWarning: error instanceof Error ? error.message : 'Could not route audio to the selected output.',
    };
  }
}

function finiteMilliseconds(value: number | undefined): number | null {
  return Number.isFinite(value) && (value ?? 0) >= 0 ? value! * 1000 : null;
}

export function readAudioRuntimeSettings(
  context: AudioContext,
  stream: MediaStream | null,
  outputRouting: AudioRuntimeSettings['outputRouting'],
  outputRoutingWarning: string | null,
): AudioRuntimeSettings {
  const track = stream?.getAudioTracks()[0];
  const settings = track?.getSettings() as (MediaTrackSettings & { latency?: number }) | undefined;
  const captureLatencyMs = finiteMilliseconds(settings?.latency);
  const baseLatencyMs = finiteMilliseconds(context.baseLatency);
  const outputLatencyMs = finiteMilliseconds((context as AudioContext & { outputLatency?: number }).outputLatency);
  const knownPathParts = [captureLatencyMs, baseLatencyMs, outputLatencyMs].filter(value => value !== null);
  const inputSampleRate = Number.isFinite(settings?.sampleRate) ? settings!.sampleRate! : null;
  return {
    contextSampleRate: context.sampleRate,
    inputSampleRate,
    inputChannelCount: Number.isFinite(settings?.channelCount) ? settings!.channelCount! : null,
    captureLatencyMs,
    baseLatencyMs,
    outputLatencyMs,
    estimatedPathLatencyMs: knownPathParts.length > 0
      ? knownPathParts.reduce((total, value) => total + value!, 0)
      : null,
    sampleRateMismatch: inputSampleRate !== null && inputSampleRate !== context.sampleRate,
    outputRouting,
    outputRoutingWarning,
  };
}

export function calibrationCandidate(
  requestedHint: AudioLatencyHint,
  runtime: AudioRuntimeSettings,
  trace: AudioTraceSnapshot,
): AudioCalibrationCandidate {
  const overDeadline = trace.deadlineMs > 0 && trace.stages.callbackTotal.maxMs > trace.deadlineMs;
  const rejectionReason = trace.underrunsDelta > 0
    ? `${trace.underrunsDelta} underrun${trace.underrunsDelta === 1 ? '' : 's'}`
    : trace.callbackDeadlineMissesDelta > 0
      ? `${trace.callbackDeadlineMissesDelta} deadline miss${trace.callbackDeadlineMissesDelta === 1 ? '' : 'es'}`
      : overDeadline
        ? `Callback max ${trace.stages.callbackTotal.maxMs.toFixed(3)} ms exceeded ${trace.deadlineMs.toFixed(3)} ms`
        : null;
  return { requestedHint, runtime, trace, stable: rejectionReason === null, rejectionReason };
}

function hintRank(hint: AudioLatencyHint): number {
  return hint === 'interactive' ? Number.POSITIVE_INFINITY : hint * 1000;
}

export function selectCalibrationCandidate(
  candidates: readonly AudioCalibrationCandidate[],
): AudioCalibrationCandidate | null {
  const stable = candidates.filter(candidate => candidate.stable);
  stable.sort((left, right) => {
    const leftLatency = left.runtime.estimatedPathLatencyMs ?? Number.POSITIVE_INFINITY;
    const rightLatency = right.runtime.estimatedPathLatencyMs ?? Number.POSITIVE_INFINITY;
    if (leftLatency !== rightLatency) return leftLatency - rightLatency;
    const leftBase = left.runtime.baseLatencyMs ?? Number.POSITIVE_INFINITY;
    const rightBase = right.runtime.baseLatencyMs ?? Number.POSITIVE_INFINITY;
    if (leftBase !== rightBase) return leftBase - rightBase;
    return hintRank(left.requestedHint) - hintRank(right.requestedHint);
  });
  return stable[0] ?? null;
}

export function formatLatencyHint(hint: AudioLatencyHint): string {
  return hint === 'interactive' ? 'Interactive' : `${(hint * 1000).toFixed(1)} ms`;
}
