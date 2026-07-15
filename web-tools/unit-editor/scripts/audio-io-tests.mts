import assert from 'node:assert/strict';

import {
  AUDIO_IO_STORAGE_KEY,
  calibrationCandidate,
  collectAudioDevices,
  loadAudioIoPreference,
  microphoneConstraints,
  recommendedOutputDeviceId,
  resolveAudioIoPreference,
  saveAudioIoPreference,
  selectCalibrationCandidate,
  type AudioRuntimeSettings,
} from '../src/lib/audioIo.ts';
import type { AudioTraceSnapshot, AudioTraceStageStats } from '@audio-playground/wasm-tools';

const rawDevices = [
  { deviceId: 'default', groupId: '', kind: 'audioinput', label: 'Default input' },
  { deviceId: 'interface-in', groupId: 'interface', kind: 'audioinput', label: 'Interface In' },
  { deviceId: 'default', groupId: '', kind: 'audiooutput', label: 'Default output' },
  { deviceId: 'interface-out', groupId: 'interface', kind: 'audiooutput', label: 'Interface Out' },
  { deviceId: 'camera', groupId: 'camera', kind: 'videoinput', label: 'Camera' },
] as MediaDeviceInfo[];
const devices = collectAudioDevices(rawDevices);
assert.equal(devices.length, 4);
assert.equal(recommendedOutputDeviceId(devices, 'interface-in', 'default'), 'interface-out');
assert.equal(recommendedOutputDeviceId(devices, 'interface-in', 'manual-output'), 'manual-output');

assert.deepEqual(resolveAudioIoPreference({
  inputDeviceId: 'missing',
  outputDeviceId: 'missing',
  latencyHint: 0.0053,
}, devices), {
  inputDeviceId: 'default',
  outputDeviceId: 'default',
  latencyHint: 0.0053,
});

const constraints = microphoneConstraints({
  inputDeviceId: 'interface-in',
  outputDeviceId: 'interface-out',
  latencyHint: 0.0027,
}, 48_000);
assert.deepEqual(constraints.deviceId, { exact: 'interface-in' });
assert.deepEqual(constraints.sampleRate, { ideal: 48_000 });
assert.equal(constraints.echoCancellation, false);

const storageValues = new Map<string, string>();
const storage = {
  getItem: (key: string) => storageValues.get(key) ?? null,
  setItem: (key: string, value: string) => storageValues.set(key, value),
};
saveAudioIoPreference(storage, {
  inputDeviceId: 'interface-in',
  outputDeviceId: 'interface-out',
  latencyHint: 0.0053,
});
assert(storageValues.has(AUDIO_IO_STORAGE_KEY));
assert.equal(loadAudioIoPreference(storage).latencyHint, 0.0053);
storageValues.set(AUDIO_IO_STORAGE_KEY, '{broken');
assert.equal(loadAudioIoPreference(storage).latencyHint, 'interactive');

const stage = (maxMs = 0.4): AudioTraceStageStats => ({
  sampleCount: 10,
  meanMs: 0.1,
  p95Ms: 0.2,
  maxMs,
  deadlineUtilization: 7.5,
});
const trace = (changes: Partial<AudioTraceSnapshot> = {}): AudioTraceSnapshot => ({
  status: 'complete',
  sampleRate: 48_000,
  quantumFrames: 128,
  deadlineMs: 128 / 48,
  elapsedMs: 5_000,
  durationMs: 5_000,
  callbackCount: 1_875,
  sampleCount: 235,
  underrunsDelta: 0,
  callbackDeadlineMissesDelta: 0,
  stages: {
    schedulingJitter: stage(20),
    inputCopy: stage(),
    wasmProcess: stage(),
    outputCopy: stage(),
    latencyProbe: stage(),
    channelCopy: stage(),
    callbackTotal: stage(1),
  },
  ...changes,
});
const runtime = (estimatedPathLatencyMs: number): AudioRuntimeSettings => ({
  contextSampleRate: 48_000,
  inputSampleRate: 48_000,
  inputChannelCount: 1,
  captureLatencyMs: 2,
  baseLatencyMs: 2.7,
  outputLatencyMs: estimatedPathLatencyMs - 4.7,
  estimatedPathLatencyMs,
  sampleRateMismatch: false,
  outputRouting: 'selected',
  outputRoutingWarning: null,
});

const fast = calibrationCandidate(0.0027, runtime(12), trace());
assert.equal(fast.stable, true, 'cadence gaps alone must not reject a candidate');
const missed = calibrationCandidate(0.0053, runtime(9), trace({ callbackDeadlineMissesDelta: 1 }));
assert.equal(missed.stable, false);
assert.match(missed.rejectionReason ?? '', /deadline miss/);
const slow = calibrationCandidate('interactive', runtime(18), trace());
assert.equal(selectCalibrationCandidate([slow, missed, fast]), fast);
assert.equal(selectCalibrationCandidate([missed]), null);

console.log('audio I/O tests passed');
