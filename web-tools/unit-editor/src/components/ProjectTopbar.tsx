import type { ProjectInspect, ValidationResult } from '../lib/backendSamples';

type Props = {
  project: ProjectInspect;
  validation: ValidationResult;
  dirtyParamCount: number;
  hasDirtyParamDrafts: boolean;
  hasWorkspaceDrafts: boolean;
  workspaceFileCount: number;
  onExportWorkspace: () => void;
  onImportWorkspace: (file: File | null) => void;
  onResetWorkspace: () => void;
};

export function ProjectTopbar({
  project,
  validation,
  dirtyParamCount,
  hasDirtyParamDrafts,
  hasWorkspaceDrafts,
  workspaceFileCount,
  onExportWorkspace,
  onImportWorkspace,
  onResetWorkspace,
}: Props) {
  const draftStateClass = hasDirtyParamDrafts ? 'status-pill--warn' : 'status-pill--ok';

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
        <div>
          <span className="project-summary__label">Workspace</span>
          <strong>{workspaceFileCount} files</strong>
        </div>
      </div>

      <div className="topbar__workspace-actions">
        <label className="btn btn--ghost">
          Import
          <input
            accept="application/json"
            onChange={event => {
              void onImportWorkspace(event.target.files?.[0] ?? null);
              event.target.value = '';
            }}
            type="file"
          />
        </label>
        <button className="btn btn--ghost" onClick={onExportWorkspace} type="button">
          Export
        </button>
        <button className="btn btn--ghost" disabled={!hasWorkspaceDrafts} onClick={onResetWorkspace} type="button">
          Reset
        </button>
      </div>

      <div className="topbar__status" aria-label="Project status">
        <div className={`status-pill ${validation.ok ? 'status-pill--ok' : 'status-pill--bad'}`}>
          {validation.ok ? 'Valid' : 'Invalid'}
        </div>
        <div className={`status-pill ${draftStateClass}`}>
          {hasDirtyParamDrafts ? 'Drafts pending' : 'Backend synced'}
        </div>
      </div>
    </header>
  );
}
