import { useLiveBypass } from '../lib/liveBypass';

export function LiveLatencyBadge() {
  const { controller } = useLiveBypass();
  if (!controller?.running || controller.latencyMs === null) return null;

  return (
    <output
      className="live-latency-badge"
      title="Browser-estimated render and output latency. Microphone capture latency is not exposed by the browser."
    >
      <span>Output est.</span>
      <strong>{controller.latencyMs.toFixed(1)} ms</strong>
    </output>
  );
}
