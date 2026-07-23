import assert from 'node:assert/strict';

import {
  AUDIO_IO_STORAGE_KEY,
  MICROPHONE_INSECURE_CONTEXT_CODE,
  MICROPHONE_UNAVAILABLE_CODE,
  calibrationCandidate,
  collectAudioDevices,
  describeAudioIssue,
  inspectMicrophoneCapability,
  loadAudioIoPreference,
  micPathLatencySeverity,
  microphoneConstraints,
  recommendedOutputDeviceId,
  resolveAudioIoPreference,
  saveAudioIoPreference,
  selectCalibrationCandidate,
  type AudioRuntimeSettings,
} from '../src/lib/audioIo.ts';
import type { AudioTraceSnapshot, AudioTraceStageStats } from '@audio-playground/wasm-tools';

assert.equal(micPathLatencySeverity(20), 'normal');
assert.equal(micPathLatencySeverity(20.001), 'warning');
assert.equal(micPathLatencySeverity(30), 'warning');
assert.equal(micPathLatencySeverity(30.001), 'danger');

const supportedMicrophone = inspectMicrophoneCapability({
  isSecureContext: true,
  origin: 'https://192.168.1.20:5173',
  hasGetUserMedia: true,
});
assert.equal(supportedMicrophone.available, true);
assert.equal(supportedMicrophone.issue, null);

const insecureMicrophone = inspectMicrophoneCapability({
  isSecureContext: false,
  origin: 'http://192.168.1.20:5173',
  hasGetUserMedia: false,
});
assert.equal(insecureMicrophone.available, false);
assert.equal(insecureMicrophone.issue?.code, MICROPHONE_INSECURE_CONTEXT_CODE);
assert.equal(insecureMicrophone.issue?.phase, 'capability');
assert.match(insecureMicrophone.issue?.message ?? '', /http:\/\/192\.168\.1\.20:5173/);
assert.match(insecureMicrophone.issue?.message ?? '', /trusted HTTPS/);
assert.match(insecureMicrophone.issue?.detail ?? '', /window\.isSecureContext=false/);

const unavailableMicrophone = inspectMicrophoneCapability({
  isSecureContext: true,
  origin: 'https://audio.example.test',
  hasGetUserMedia: false,
});
assert.equal(unavailableMicrophone.available, false);
assert.equal(unavailableMicrophone.issue?.code, MICROPHONE_UNAVAILABLE_CODE);
assert.match(unavailableMicrophone.issue?.message ?? '', /current Chrome, Safari, or Firefox/);
assert.match(unavailableMicrophone.issue?.detail ?? '', /navigator\.mediaDevices\.getUserMedia/);

const deniedIssue = describeAudioIssue(
  new DOMException('Injected microphone permission denial', 'NotAllowedError'),
  'start',
  'microphone',
  'audio-1',
);
assert.equal(deniedIssue.code, 'NotAllowedError');
assert.match(deniedIssue.message, /Microphone access was denied/);
assert.equal(deniedIssue.detail, 'Injected microphone permission denial');

const engineIssue = describeAudioIssue(new Error('Processor failed'), 'control', 'audio-engine', 'audio-2');
assert.equal(engineIssue.message, 'Processor failed');
assert.equal(engineIssue.detail, null);

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

const anonymousDevices = collectAudioDevices([
  { deviceId: '', groupId: '', kind: 'audioinput', label: '' },
  { deviceId: '', groupId: '', kind: 'audioinput', label: '' },
  { deviceId: '', groupId: '', kind: 'audiooutput', label: '' },
] as MediaDeviceInfo[]);
assert.deepEqual(anonymousDevices.map(device => `${device.kind}:${device.deviceId}`), [
  'audioinput:default',
  'audiooutput:default',
]);

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
assert.equal(microphoneConstraints({
  inputDeviceId: '',
  outputDeviceId: '',
  latencyHint: 'interactive',
}, 48_000).deviceId, undefined);

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
