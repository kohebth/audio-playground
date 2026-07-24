import { useLiveBypass } from '../lib/liveBypass';
import { micPathLatencySeverity } from '../lib/audioIo';

export function LiveLatencyBadge() {
  const { controller } = useLiveBypass();
  if (!controller?.running || controller.latencyMs === null) return null;
  if (controller.measuredLatencyMs !== null) {
    const severity = micPathLatencySeverity(controller.measuredLatencyMs);
    return (
      <output
        className={`live-latency-badge ${severity === 'normal' ? 'live-latency-badge--passing' : `live-latency-badge--${severity}`}`}
        title="Acoustic loopback result from the test chirp. It includes speaker-to-microphone air travel."
      >
        <strong>{controller.measuredLatencyMs.toFixed(1)} ms</strong>
      </output>
    );
  }
  const microphonePath = controller.captureLatencyMs !== null;
  const totalLatencyMs = controller.latencyMs + (controller.captureLatencyMs ?? 0);
  const severityClass = microphonePath
    ? ` live-latency-badge--${micPathLatencySeverity(totalLatencyMs)}`
    : '';

  return (
    <output
      className={`live-latency-badge${severityClass}`}
      title={
        microphonePath
          ? 'Estimated microphone capture, browser render, and output latency. Values can vary with the browser and device.'
          : 'Browser-estimated render and output latency. Microphone capture latency is unavailable from this browser.'
      }
    >
      <strong>{totalLatencyMs.toFixed(1)} ms</strong>
    </output>
  );
}
