import type { ProjectInspect } from '../lib/backendSamples';

type Props = {
  project: ProjectInspect;
};

const PROFILES = ['desktop_full', 'wasm_realtime', 'm7_static', 'offline_render'];

function activeUnits(project: ProjectInspect) {
  const activeUnitIds = new Set(project.nodes.map(node => node.unit));
  return project.units.filter(unit => activeUnitIds.has(unit.id));
}

function profileSupported(project: ProjectInspect, profile: string): boolean {
  return activeUnits(project).every(unit => unit.compatibility[profile]);
}

export function CompatibilityExportPanel({ project }: Props) {
  const unavailableReason = 'Not available in this build.';
  const units = activeUnits(project);
  return (
    <details className="inspector-block" open>
      <summary className="inspector-block__label">Compatibility</summary>
      <div className="compat-matrix">
        <div className="compat-matrix__header">
          <span>Unit</span>
          {PROFILES.map(profile => <strong key={profile}>{profile}</strong>)}
        </div>
        {units.map(unit => (
          <div key={unit.id} className="compat-matrix__row">
            <span>{unit.name}</span>
            {PROFILES.map(profile => (
              <strong key={profile} className={unit.compatibility[profile] ? 'compat-matrix__ok' : 'compat-matrix__bad'}>
                {unit.compatibility[profile] ? 'yes' : 'no'}
              </strong>
            ))}
          </div>
        ))}
        {units.length === 0 ? <p className="compat-matrix__empty">No units placed in the Pipeline.</p> : null}
      </div>

      <div className="export-readiness">
        <div>
          <span>Desktop</span>
          <strong>{profileSupported(project, 'desktop_full') ? 'ready' : 'blocked'}</strong>
          {profileSupported(project, 'desktop_full') ? null : <small>{unavailableReason}</small>}
        </div>
        <div>
          <span>Web/WASM</span>
          <strong>blocked</strong>
          <small>{unavailableReason}</small>
        </div>
        <div>
          <span>M7 Static</span>
          <strong>blocked</strong>
          <small>{unavailableReason}</small>
        </div>
        <div>
          <span>Benchmark</span>
          <strong>blocked</strong>
          <small>{unavailableReason}</small>
        </div>
      </div>
    </details>
  );
}
