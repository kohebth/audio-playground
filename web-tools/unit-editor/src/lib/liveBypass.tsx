import { createContext, useContext, type Dispatch, type SetStateAction } from 'react';

export type LiveBypassController = {
  running: boolean;
  latencyMs: number | null;
  bypassByInstance: Record<string, boolean>;
  setBypass: (instanceId: string, enabled: boolean) => Promise<void>;
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
