import type { ProjectInspect, ValidationResult } from '../lib/backendSamples';

type Props = {
  project: ProjectInspect;
  validation: ValidationResult;
  dirtyParamCount: number;
};

export function ProjectTopbar({ project, validation, dirtyParamCount }: Props) {
  return (
    <header className="topbar topbar--project">
      <div className="topbar__brand">
        <span className="topbar__logo">APG</span>
        <div>
          <div className="topbar__title">Audio Playground</div>
          <div className="topbar__subtitle">v2 project workbench</div>
        </div>
      </div>

      <div className="project-summary" aria-label="Project summary">
        <div>
          <span className="project-summary__label">Project</span>
          <strong>{project.name}</strong>
        </div>
        <div>
          <span className="project-summary__label">Target</span>
          <strong>{project.targets.default}</strong>
        </div>
        <div>
          <span className="project-summary__label">Compiled</span>
          <strong>{project.compiled.nodes} nodes / {project.compiled.signals} signals</strong>
        </div>
        <div>
          <span className="project-summary__label">Draft Edits</span>
          <strong>{dirtyParamCount}</strong>
        </div>
      </div>

      <div className={`status-pill ${validation.ok ? 'status-pill--ok' : 'status-pill--bad'}`}>
        {validation.ok ? 'Valid' : 'Invalid'}
      </div>
    </header>
  );
}
