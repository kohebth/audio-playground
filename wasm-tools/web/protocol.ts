import type { AudioConfig, PreparedRuntime, ValidationResult, WasmDiagnostic, WorkspaceSnapshot } from './types';

export type ControlRequest =
  | { id: number; type: 'init'; moduleUrl: string }
  | { id: number; type: 'replaceWorkspace'; snapshot: WorkspaceSnapshot }
  | { id: number; type: 'prepare'; revision: number; audio: AudioConfig }
  | { id: number; type: 'dispose' };

export type ControlResponse =
  | { id: number; ok: true; type: 'initialized' }
  | { id: number; ok: true; type: 'validated'; result: ValidationResult }
  | { id: number; ok: true; type: 'prepared'; runtime: PreparedRuntime; image: ArrayBuffer }
  | { id: number; ok: true; type: 'disposed' }
  | { id: number; ok: false; type: 'error'; diagnostic: WasmDiagnostic };

export type ProcessorRequest =
  | { id: number; type: 'stage'; revision: number; image: ArrayBuffer }
  | { id: number; type: 'commit'; revision: number }
  | { id: number; type: 'setParam'; index: number; value: number }
  | { id: number; type: 'setBypass'; index: number; enabled: boolean }
  | { id: number; type: 'setMute'; enabled: boolean }
  | { id: number; type: 'reset' }
  | { id: number; type: 'dispose' };

export type ProcessorResponse =
  | { id: 0; ok: true; type: 'initialized' }
  | { id: number; ok: true; type: 'staged'; revision: number }
  | { id: number; ok: true; type: 'committed'; revision: number }
  | { id: number; ok: true; type: 'command' }
  | { id: number; ok: true; type: 'disposed' }
  | { id: 0; ok: true; type: 'meter'; meter: { peak: number; rms: number; frames: number; valid: boolean } }
  | { id: number; ok: false; type: 'error'; message: string };
