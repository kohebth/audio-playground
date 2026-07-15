import { createContext, useContext, type Dispatch, type SetStateAction } from 'react';
import type { AudioTraceReport, AudioTraceStatus } from '@audio-playground/wasm-tools';

export type LiveBypassController = {
  running: boolean;
  latencyMs: number | null;
  captureLatencyMs: number | null;
  measuredLatencyMs: number | null;
  inputMode: 'file' | 'microphone';
  audioTraceStatus: AudioTraceStatus;
  audioTraceProgress: number;
  audioTraceReport: AudioTraceReport | null;
  bypassByInstance: Record<string, boolean>;
  setBypass: (instanceId: string, enabled: boolean) => Promise<void>;
  profileAudio: () => Promise<void>;
  clearAudioTrace: () => void;
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
