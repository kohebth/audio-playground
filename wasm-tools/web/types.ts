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
  callbackDeadlineMisses: number;
  maxCallbackMs: number;
};

export type AudioTraceStatus = 'idle' | 'running' | 'complete';

export type AudioTraceStageName =
  | 'schedulingJitter'
  | 'inputCopy'
  | 'wasmProcess'
  | 'outputCopy'
  | 'latencyProbe'
  | 'channelCopy'
  | 'callbackTotal';

export type AudioTraceStageStats = {
  sampleCount: number;
  meanMs: number;
  p95Ms: number;
  maxMs: number;
  deadlineUtilization: number;
};

export type AudioTraceSnapshot = {
  status: AudioTraceStatus;
  sampleRate: number;
  quantumFrames: number;
  deadlineMs: number;
  elapsedMs: number;
  durationMs: number;
  callbackCount: number;
  sampleCount: number;
  underrunsDelta: number;
  callbackDeadlineMissesDelta: number;
  stages: Record<AudioTraceStageName, AudioTraceStageStats>;
};

export type AudioTraceReport = {
  schema: 'apg.audio-trace.v1';
  capturedAt: string;
  browser: {
    captureLatencyMs: number | null;
    baseLatencyMs: number | null;
    outputLatencyMs: number | null;
    acousticLoopbackMs: number | null;
  };
  trace: AudioTraceSnapshot;
  slowestInternalStage: Exclude<AudioTraceStageName, 'schedulingJitter' | 'callbackTotal'> | null;
  verdict: 'internal-over-budget' | 'scheduling-delayed' | 'internal-healthy';
  message: string;
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
