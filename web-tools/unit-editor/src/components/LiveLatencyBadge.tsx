import { useLiveBypass } from '../lib/liveBypass';

export function LiveLatencyBadge() {
  const { controller } = useLiveBypass();
  if (!controller?.running || controller.latencyMs === null) return null;
  if (controller.measuredLatencyMs !== null) {
    const passing = controller.measuredLatencyMs < 15;
    return (
      <output
        className={`live-latency-badge live-latency-badge--${passing ? 'passing' : 'high'}`}
        title="Acoustic loopback result from the test chirp. It includes speaker-to-microphone air travel."
      >
        <span>{passing ? 'Loopback ready' : 'Loopback high'}</span>
        <strong>{controller.measuredLatencyMs.toFixed(1)} ms</strong>
      </output>
    );
  }
  const microphonePath = controller.captureLatencyMs !== null;
  const totalLatencyMs = controller.latencyMs + (controller.captureLatencyMs ?? 0);

  return (
    <output
      className="live-latency-badge"
      title={
        microphonePath
          ? 'Estimated microphone capture, browser render, and output latency. Values can vary with the browser and device.'
          : 'Browser-estimated render and output latency. Microphone capture latency is unavailable from this browser.'
      }
    >
      <span>{microphonePath ? 'Mic path est.' : 'Output path'}</span>
      <strong>{totalLatencyMs.toFixed(1)} ms</strong>
    </output>
  );
}
