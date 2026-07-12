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
  private processorReadyResolve: (() => void) | null = null;
  private processorReadyReject: ((error: Error) => void) | null = null;
  private meter: MeterSnapshot = { peak: 0, rms: 0, frames: 0, valid: false };
  private state: BackendState = {
    phase: 'idle',
    workspaceRevision: 0,
    preparedRevision: 0,
    activeRevision: 0,
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
    if (response.type === 'meter') {
      this.meter = response.meter;
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
      if (response.type === 'staged') {
        this.state = { ...this.state, phase: 'running', activeRevision: response.revision };
      }
      pending.resolve(response);
    } else {
      pending.reject(new Error(response.message));
    }
  }

  async replaceWorkspace(snapshot: WorkspaceSnapshot): Promise<ValidationResult> {
    this.state = { ...this.state, phase: 'validating', workspaceRevision: snapshot.revision, lastError: null };
    const response = await this.controlRequest({ type: 'replaceWorkspace', snapshot });
    if (response.type !== 'validated') throw new Error(`Unexpected control response: ${response.type}`);
    this.state = {
      ...this.state,
      phase: response.result.ok ? 'idle' : 'error',
      lastError: response.result.ok ? null : response.result.diagnostic,
    };
    if (response.result.ok) this.validatedRevision = snapshot.revision;
    return response.result;
  }

  async prepare(revision: number, audio: AudioConfig): Promise<PreparedRuntime> {
    this.state = { ...this.state, phase: 'preparing', lastError: null };
    const response = await this.controlRequest({ type: 'prepare', revision, audio });
    if (response.type !== 'prepared') throw new Error(`Unexpected control response: ${response.type}`);
    this.prepared = response.runtime;
    this.preparedImage = response.image;
    this.state = { ...this.state, phase: 'ready', preparedRevision: revision };
    if (this.node) await this.stagePrepared();
    return response.runtime;
  }

  async compile(snapshot: WorkspaceSnapshot, audio: AudioConfig): Promise<PreparedRuntime> {
    if (this.validatedRevision !== snapshot.revision) {
      const validation = await this.replaceWorkspace(snapshot);
      if (!validation.ok) throw new Error(validation.diagnostic.message || validation.diagnostic.code);
    }
    return this.prepare(snapshot.revision, audio);
  }

  private async stagePrepared() {
    if (!this.prepared || !this.preparedImage) return;
    const image = this.preparedImage.slice(0);
    await this.processorRequest({ type: 'stage', revision: this.prepared.revision, image }, [image]);
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
    if (this.prepared) await this.stagePrepared();
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
    const index = this.prepared.params.indexOf(name);
    if (index < 0) throw new Error(`Unknown parameter: ${name}`);
    await this.processorRequest({ type: 'setParam', index, value });
  }

  async setBypass(instanceId: string, enabled: boolean): Promise<void> {
    if (!this.prepared) throw new Error('No prepared runtime');
    const index = this.prepared.bypassInstances.indexOf(instanceId);
    if (index < 0) throw new Error(`Unknown bypass instance: ${instanceId}`);
    await this.processorRequest({ type: 'setBypass', index, enabled });
  }

  async setMute(enabled: boolean): Promise<void> {
    await this.processorRequest({ type: 'setMute', enabled });
  }

  async reset(): Promise<void> {
    await this.processorRequest({ type: 'reset' });
  }

  getMeters(): MeterSnapshot {
    return { ...this.meter };
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
