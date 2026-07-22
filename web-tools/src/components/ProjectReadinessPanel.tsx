import type { ProjectReadinessSnapshot, ReadinessStatus } from '../lib/projectPackage';
import { buildInfo } from '../lib/buildInfo';

type Props = {
  open: boolean;
  readiness: ProjectReadinessSnapshot;
  onClose: () => void;
};

function Status({ value }: { value: ReadinessStatus }) {
  return <span className={`readiness-status readiness-status--${value}`}>{value}</span>;
}

export function ProjectReadinessPanel({ open, readiness, onClose }: Props) {
  if (!open) return null;
  const targets = Object.entries(readiness.targets);
  return (
    <div className="readiness-popover" data-testid="readiness-panel" role="dialog" aria-label="Project readiness">
      <header>
        <div><span>Project readiness</span><strong>{readiness.validation === 'ready' && readiness.preview !== 'blocked' ? 'Good to go' : 'Needs attention'}</strong></div>
        <button aria-label="Close readiness" onClick={onClose} type="button">×</button>
      </header>
      <div className="readiness-popover__checks">
        <div><span>Project validation</span><Status value={readiness.validation} /></div>
        <div><span>Live preview</span><Status value={readiness.preview} /></div>
        {targets.map(([target, status]) => <div key={target}><span>{target.replace(/_/g, ' ')}</span><Status value={status} /></div>)}
      </div>
      {readiness.diagnostics.length > 0 ? (
        <div className="readiness-popover__diagnostics">
          {readiness.diagnostics.map((diagnostic, index) => (
            <p key={`${diagnostic.code ?? 'diagnostic'}-${index}`}>
              <strong>{diagnostic.code ?? 'Check'}</strong>
              <span>{diagnostic.message}</span>
            </p>
          ))}
        </div>
      ) : <p className="readiness-popover__empty">No readiness issues found.</p>}
      <div className="readiness-popover__build" data-testid="build-diagnostics">
        <div><span>Build</span><code data-testid="build-commit-sha">{buildInfo.commitSha}</code></div>
        <div><span>Base</span><code data-testid="build-base-path">{buildInfo.basePath}</code></div>
        <div><span>Mode</span><code>{buildInfo.mode}</code></div>
      </div>
      <small>{readiness.checkedAt ? `Checked ${new Date(readiness.checkedAt).toLocaleTimeString()}` : 'Waiting for the audio engine'}</small>
    </div>
  );
}
