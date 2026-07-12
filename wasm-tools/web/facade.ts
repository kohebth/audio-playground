import ControlWorker from './control.worker?worker';

import type { ControlRequest, ControlResponse, ProcessorRequest, ProcessorResponse } from './protocol';
import type {
  AudioConfig,
  BackendState,
  MeterSnapshot,
  PreparedRuntime,
  StartOptions,
  ValidationResult,
  WasmBackendOptions,
  WasmDiagnostic,
  WorkspaceSnapshot,
} from './types';

type Pending<T> = { resolve: (value: T) => void; reject: (error: Error) => void };
type WithoutId<T> = T extends unknown ? Omit<T, 'id'> : never;

export class WasmBackend {
  private readonly options: WasmBackendOptions;
  private readonly worker = new ControlWorker();
  private readonly pending = new Map<number, Pending<ControlResponse>>();
  private readonly processorPending = new Map<number, Pending<ProcessorResponse>>();
  private requestId = 1;
  private context: AudioContext | null = null;
  private node: AudioWorkletNode | null = null;
  private prepared: PreparedRuntime | null = null;
  private preparedImage: ArrayBuffer | null = null;
  private validatedRevision = 0;
  private readonly bypassShadows = new Map<string, boolean>();
  private muteShadow = false;
  private processorReadyResolve: (() => void) | null = null;
  private processorReadyReject: ((error: Error) => void) | null = null;
  private meter: MeterSnapshot = { peak: 0, rms: 0, frames: 0, valid: false, activeRevision: 0, underruns: 0 };
  private state: BackendState = {
    phase: 'idle',
    workspaceRevision: 0,
    preparedRevision: 0,
    activeRevision: 0,
    failedRevision: 0,
    lastError: null,
  };

  private constructor(options: WasmBackendOptions) {
    this.options = options;
    this.worker.onmessage = (event: MessageEvent<ControlResponse>) => this.handleControlResponse(event.data);
  }

  static async create(options: WasmBackendOptions): Promise<WasmBackend> {
    const backend = new WasmBackend(options);
    await backend.controlRequest({ type: 'init', moduleUrl: options.controlModuleUrl });
    return backend;
  }

  private nextId() {
    const id = this.requestId;
    this.requestId += 1;
    return id;
  }

  private controlRequest(request: WithoutId<ControlRequest>): Promise<ControlResponse> {
    const id = this.nextId();
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.worker.postMessage({ ...request, id } as ControlRequest);
    });
  }

  private processorRequest(
    request: WithoutId<ProcessorRequest>,
    transfer: Transferable[] = [],
  ): Promise<ProcessorResponse> {
    if (!this.node) return Promise.reject(new Error('AudioWorklet is not started'));
    const id = this.nextId();
    return new Promise((resolve, reject) => {
      this.processorPending.set(id, { resolve, reject });
      this.node?.port.postMessage({ ...request, id } as ProcessorRequest, transfer);
    });
  }

  private handleControlResponse(response: ControlResponse) {
    const pending = this.pending.get(response.id);
    if (!pending) return;
    this.pending.delete(response.id);
    if (response.ok) pending.resolve(response);
    else {
      this.state = { ...this.state, phase: 'error', lastError: response.diagnostic };
      pending.reject(new Error(response.diagnostic.message || response.diagnostic.code));
    }
  }

  private handleProcessorResponse(response: ProcessorResponse) {
    if (response.type === 'initialized') {
      this.processorReadyResolve?.();
      this.processorReadyResolve = null;
      this.processorReadyReject = null;
      return;
    }
    const pending = this.processorPending.get(response.id);
    if (!pending) {
      if (!response.ok) {
        const error = new Error(response.message);
        this.processorReadyReject?.(error);
        this.processorReadyResolve = null;
        this.processorReadyReject = null;
        this.state = { ...this.state, phase: 'error' };
      }
      return;
    }
    this.processorPending.delete(response.id);
    if (response.ok) {
      if (response.type === 'committed') {
        this.state = { ...this.state, phase: 'running', activeRevision: response.revision };
      }
      pending.resolve(response);
    } else {
      pending.reject(new Error(response.message));
    }
  }

  private recordRuntimeError(revision: number, phase: string, error: unknown): void {
    const message = error instanceof Error ? error.message : String(error);
    this.state = {
      ...this.state,
      phase: 'error',
      failedRevision: revision,
      lastError: {
        revision,
        status: -1,
        phase,
        code: 'APG_WASM_PROCESSOR_ERROR',
        file: '',
        path: '',
        message,
      },
    };
  }

  setCurrentRevision(revision: number): void {
    if (!Number.isSafeInteger(revision) || revision <= 0) throw new Error('Workspace revision must be a positive integer');
    if (revision < this.state.workspaceRevision) throw new Error('Workspace revision cannot move backwards');
    if (revision === this.state.workspaceRevision) return;
    this.state = { ...this.state, workspaceRevision: revision, lastError: null };
  }

  async replaceWorkspace(snapshot: WorkspaceSnapshot): Promise<ValidationResult> {
    this.setCurrentRevision(snapshot.revision);
    this.state = { ...this.state, phase: 'validating', lastError: null };
    const response = await this.controlRequest({ type: 'replaceWorkspace', snapshot });
    if (response.type !== 'validated') throw new Error(`Unexpected control response: ${response.type}`);
    if (snapshot.revision !== this.state.workspaceRevision) return response.result;
    this.state = {
      ...this.state,
      phase: response.result.ok ? 'idle' : 'error',
      failedRevision: response.result.ok ? this.state.failedRevision : snapshot.revision,
      lastError: response.result.ok ? null : response.result.diagnostic,
    };
    if (response.result.ok) this.validatedRevision = snapshot.revision;
    return response.result;
  }

  async prepare(revision: number, audio: AudioConfig): Promise<PreparedRuntime> {
    if (revision !== this.state.workspaceRevision) throw new Error(`Workspace revision ${revision} is stale`);
    this.state = { ...this.state, phase: 'preparing', lastError: null };
    let response: ControlResponse;
    try {
      response = await this.controlRequest({ type: 'prepare', revision, audio });
    } catch (error) {
      if (revision === this.state.workspaceRevision) {
        this.state = { ...this.state, phase: 'error', failedRevision: revision };
      }
      throw error;
    }
    if (response.type !== 'prepared') throw new Error(`Unexpected control response: ${response.type}`);
    if (revision !== this.state.workspaceRevision) throw new Error(`Prepared revision ${revision} is stale`);
    this.prepared = response.runtime;
    this.preparedImage = response.image;
    this.state = { ...this.state, phase: 'ready', preparedRevision: revision };
    return response.runtime;
  }

  async compile(snapshot: WorkspaceSnapshot, audio: AudioConfig): Promise<PreparedRuntime> {
    if (this.validatedRevision !== snapshot.revision) {
      const validation = await this.replaceWorkspace(snapshot);
      if (!validation.ok) throw new Error(validation.diagnostic.message || validation.diagnostic.code);
    }
    return this.prepare(snapshot.revision, audio);
  }

  async commitPrepared(revision: number): Promise<void> {
    if (revision !== this.state.workspaceRevision) throw new Error(`Prepared revision ${revision} is stale`);
    if (!this.prepared || !this.preparedImage || this.prepared.revision !== revision) {
      throw new Error(`Revision ${revision} is not prepared`);
    }
    if (!this.node) throw new Error('AudioWorklet is not started');
    const image = this.preparedImage.slice(0);
    try {
      await this.processorRequest({ type: 'stage', revision: this.prepared.revision, image }, [image]);
    } catch (error) {
      if (revision === this.state.workspaceRevision) this.recordRuntimeError(revision, 'hydrate', error);
      throw error;
    }
    if (revision !== this.state.workspaceRevision) throw new Error(`Staged revision ${revision} is stale`);
    for (const [instanceId, enabled] of this.bypassShadows) {
      const index = this.prepared.bypassInstances.indexOf(instanceId);
      if (index >= 0) await this.processorRequest({ type: 'setBypass', index, enabled });
    }
    if (this.muteShadow) await this.processorRequest({ type: 'setMute', enabled: true });
    if (revision !== this.state.workspaceRevision) throw new Error(`Controlled revision ${revision} is stale`);
    let response: ProcessorResponse;
    try {
      response = await this.processorRequest({ type: 'commit', revision });
    } catch (error) {
      if (revision === this.state.workspaceRevision) this.recordRuntimeError(revision, 'commit', error);
      throw error;
    }
    if (response.type !== 'committed') throw new Error(`Unexpected processor response: ${response.type}`);
  }

  async start(options: StartOptions = {}): Promise<void> {
    if (this.node) return;
    const AudioContextConstructor = globalThis.AudioContext;
    this.context = this.options.audioContext ?? new AudioContextConstructor();
    if (this.context.state === 'suspended') await this.context.resume();
    await this.context.audioWorklet.addModule(this.options.processorWorkletUrl);
    const processorResponse = await fetch(this.options.processorWasmUrl);
    if (!processorResponse.ok) {
      throw new Error(`Could not load processor WASM: ${processorResponse.status} ${processorResponse.statusText}`);
    }
    const wasmBinary = await processorResponse.arrayBuffer();
    const processorReady = new Promise<void>((resolve, reject) => {
      this.processorReadyResolve = resolve;
      this.processorReadyReject = reject;
    });
    this.node = new AudioWorkletNode(this.context, 'apg-wasm-processor', {
      numberOfInputs: 1,
      numberOfOutputs: 1,
      outputChannelCount: [1],
      processorOptions: { moduleUrl: this.options.processorModuleUrl, wasmBinary },
    });
    this.node.port.onmessage = event => this.handleProcessorResponse(event.data as ProcessorResponse);
    await processorReady;
    options.input?.connect(this.node);
    this.node.connect(options.output ?? this.context.destination);
    if (this.prepared) await this.commitPrepared(this.prepared.revision);
    this.state = { ...this.state, phase: 'running' };
  }

  async stop(): Promise<void> {
    if (!this.node) return;
    await this.processorRequest({ type: 'dispose' }).catch(() => undefined);
    this.node.disconnect();
    this.node = null;
    this.state = { ...this.state, phase: this.prepared ? 'ready' : 'idle', activeRevision: 0 };
  }

  async setParam(name: string, value: number): Promise<void> {
    if (!this.prepared) throw new Error('No prepared runtime');
    if (!Number.isFinite(value)) throw new Error(`Parameter ${name} must be finite`);
    const index = this.prepared.params.indexOf(name);
    if (index < 0) throw new Error(`Unknown parameter: ${name}`);
    await this.processorRequest({ type: 'setParam', index, value });
  }

  async setBypass(instanceId: string, enabled: boolean): Promise<void> {
    if (!this.prepared) throw new Error('No prepared runtime');
    const index = this.prepared.bypassInstances.indexOf(instanceId);
    if (index < 0) throw new Error(`Unknown bypass instance: ${instanceId}`);
    await this.processorRequest({ type: 'setBypass', index, enabled });
    this.bypassShadows.set(instanceId, enabled);
  }

  async setMute(enabled: boolean): Promise<void> {
    await this.processorRequest({ type: 'setMute', enabled });
    this.muteShadow = enabled;
  }

  async reset(): Promise<void> {
    await this.processorRequest({ type: 'reset' });
  }

  getMeters(): MeterSnapshot {
    return { ...this.meter };
  }

  async pollMeters(): Promise<MeterSnapshot> {
    const response = await this.processorRequest({ type: 'pollMeters' });
    if (response.type !== 'meter') throw new Error(`Unexpected processor response: ${response.type}`);
    this.meter = response.meter;
    return this.getMeters();
  }

  getState(): BackendState {
    return { ...this.state };
  }

  getLastError(): WasmDiagnostic | null {
    return this.state.lastError;
  }

  async destroy(): Promise<void> {
    await this.stop();
    await this.controlRequest({ type: 'dispose' }).catch(() => undefined);
    this.worker.terminate();
    if (!this.options.audioContext) await this.context?.close();
    this.context = null;
  }
}
