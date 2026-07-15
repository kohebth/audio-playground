import { createContext, useContext, type Dispatch, type SetStateAction } from 'react';
import type { AudioTraceReport, AudioTraceStatus } from '@audio-playground/wasm-tools';
import type {
  AudioCalibrationState,
  AudioDeviceOption,
  AudioIoPreference,
  AudioRuntimeSettings,
} from './audioIo';

export type LiveBypassController = {
  running: boolean;
  latencyMs: number | null;
  captureLatencyMs: number | null;
  measuredLatencyMs: number | null;
  measuringLatency: boolean;
  inputMode: 'file' | 'microphone';
  audioTraceStatus: AudioTraceStatus;
  audioTraceProgress: number;
  audioTraceReport: AudioTraceReport | null;
  audioDevices: AudioDeviceOption[];
  audioIoPreference: AudioIoPreference;
  audioRuntimeSettings: AudioRuntimeSettings | null;
  audioCalibration: AudioCalibrationState;
  bypassByInstance: Record<string, boolean>;
  setBypass: (instanceId: string, enabled: boolean) => Promise<void>;
  profileAudio: () => Promise<void>;
  clearAudioTrace: () => void;
  refreshAudioDevices: () => Promise<AudioDeviceOption[]>;
  selectAudioInput: (deviceId: string) => Promise<void>;
  selectAudioOutput: (deviceId: string) => Promise<void>;
  calibrateAudio: () => Promise<void>;
  measureAcousticLatency: () => Promise<void>;
};

type LiveBypassContextValue = {
  controller: LiveBypassController | null;
  setController: Dispatch<SetStateAction<LiveBypassController | null>>;
};

export const LiveBypassContext = createContext<LiveBypassContextValue | null>(null);

export function useLiveBypass(): LiveBypassContextValue {
  const value = useContext(LiveBypassContext);
  if (!value) throw new Error('LiveBypassContext is unavailable.');
  return value;
}
