export type WorkspaceFileRole = 'project' | 'unit';

export type WorkspaceFile = {
  path: string;
  role: WorkspaceFileRole;
  content: string;
};

export type WorkspaceSnapshot = {
  revision: number;
  entryProject: string;
  files: WorkspaceFile[];
};

export type AudioConfig = {
  sampleRate: number;
  blockFrames: number;
};

export type WasmDiagnostic = {
  revision: number;
  status: number;
  phase: string;
  code: string;
  file: string;
  path: string;
  message: string;
};

export type ValidationResult = {
  ok: boolean;
  revision: number;
  diagnostic: WasmDiagnostic;
};

export type PreparedRuntime = {
  revision: number;
  imageBytes: number;
  params: string[];
  bypassInstances: string[];
};

export type MeterSnapshot = {
  peak: number;
  rms: number;
  frames: number;
  valid: boolean;
  activeRevision: number;
  underruns: number;
};

export type BackendPhase = 'idle' | 'validating' | 'preparing' | 'ready' | 'running' | 'error';

export type BackendState = {
  phase: BackendPhase;
  workspaceRevision: number;
  preparedRevision: number;
  activeRevision: number;
  failedRevision: number;
  lastError: WasmDiagnostic | null;
};

export type BackendResourceSnapshot = {
  workerActive: boolean;
  workletActive: boolean;
  pendingControlRequests: number;
  pendingProcessorRequests: number;
  contextState: AudioContextState | 'none';
  workletStarts: number;
  workletStops: number;
  preparedImageBytes: number;
};

export type WasmBackendOptions = {
  controlModuleUrl: string;
  processorModuleUrl: string;
  processorWasmUrl: string;
  processorWorkletUrl: string;
  audioContext?: AudioContext;
};

export type StartOptions = {
  input?: AudioNode;
  output?: AudioNode;
};
