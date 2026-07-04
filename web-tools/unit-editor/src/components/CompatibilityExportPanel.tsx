import type { BackendCommands, ProjectInspect } from '../lib/backendSamples';

type Props = {
  project: ProjectInspect;
  commands: BackendCommands;
};

const PROFILES = ['desktop_full', 'wasm_realtime', 'm7_static', 'offline_render'];

function profileSupported(project: ProjectInspect, profile: string): boolean {
  return project.units.every(unit => unit.compatibility[profile]);
}

export function CompatibilityExportPanel({ project, commands }: Props) {
  const blockedReason = 'Blocked: command backend not wired in this web build.';
  return (
    <section className="inspector-block">
      <div className="inspector-block__label">Compatibility</div>
      <div className="compat-matrix">
        <div className="compat-matrix__header">
          <span>Unit</span>
          {PROFILES.map(profile => <strong key={profile}>{profile}</strong>)}
        </div>
        {project.units.map(unit => (
          <div key={unit.id} className="compat-matrix__row">
            <span>{unit.name}</span>
            {PROFILES.map(profile => (
              <strong key={profile} className={unit.compatibility[profile] ? 'compat-matrix__ok' : 'compat-matrix__bad'}>
                {unit.compatibility[profile] ? 'yes' : 'no'}
              </strong>
            ))}
          </div>
        ))}
      </div>

      <div className="export-readiness">
        <div>
          <span>Desktop</span>
          <strong>{profileSupported(project, 'desktop_full') ? 'ready' : 'blocked'}</strong>
          <code>{commands.renderProject}</code>
          {profileSupported(project, 'desktop_full') ? null : <small>{blockedReason}</small>}
        </div>
        <div>
          <span>Web/WASM</span>
          <strong>blocked</strong>
          <code>{commands.exportWasm}</code>
          <small>{blockedReason}</small>
        </div>
        <div>
          <span>M7 Static</span>
          <strong>blocked</strong>
          <code>{commands.exportM7}</code>
          <small>{blockedReason}</small>
        </div>
        <div>
          <span>Benchmark</span>
          <strong>blocked</strong>
          <code>{commands.benchmarkProject}</code>
          <small>{blockedReason}</small>
        </div>
      </div>
    </section>
  );
}
