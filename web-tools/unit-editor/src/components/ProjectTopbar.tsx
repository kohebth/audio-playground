import type { ProjectInspect, ValidationResult } from '../lib/backendSamples';
import { PreviewPanel } from './PreviewPanel';
import type { ParamOverride } from '../lib/projectParams';
import type { WorkspaceFile } from '../lib/backendSamples';

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
  entryProject: string;
  workspaceFiles: WorkspaceFile[];
  paramOverrides: ParamOverride[];
  onRuntimeReady: () => void;
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
  entryProject,
  workspaceFiles,
  paramOverrides,
  onRuntimeReady,
}: Props) {
  const draftStateClass = hasDirtyParamDrafts ? 'status-pill--warn' : 'status-pill--ok';

  return (
    <header className="topbar topbar--project app-header">
      <div className="header-left">
        <div className="header-brand">
          <span className="topbar__logo" aria-hidden="true">
            <i className="fa-solid fa-wave-square" />
          </span>
          <span className="header-brand-name">APG</span>
        </div>
        <span className="header-divider" />
        <div className="header-project">
          <span className="header-project-label">Active Project</span>
          <div className="header-project-name">
            <strong>{project.name}</strong>
            <span className="tag">v2.yaml</span>
          </div>
        </div>
      </div>

      <PreviewPanel
        compact
        entryProject={entryProject}
        onRuntimeReady={onRuntimeReady}
        paramOverrides={paramOverrides}
        workspaceFiles={workspaceFiles}
      />

      <div className="header-right">
        <div className="topbar__status" aria-label="Project status">
          <div className={`status-pill ${validation.ok ? 'status-pill--ok' : 'status-pill--bad'}`}>
            {validation.ok ? 'Valid' : 'Invalid'}
          </div>
          <div className={`status-pill ${draftStateClass}`} title={`${workspaceFileCount} workspace files`}>
            {hasDirtyParamDrafts ? `Unsaved edits (${dirtyParamCount})` : 'Saved locally'}
          </div>
        </div>
        <span className="header-divider" />
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
      </div>
    </header>
  );
}
