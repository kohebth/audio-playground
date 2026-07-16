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
  workspaceSaveError: string | null;
  workspaceFileCount: number;
  onExportWorkspace: () => void;
  onImportWorkspace: (file: File | null) => void;
  onResetWorkspace: () => void;
  onSaveWorkspace: () => void;
  onUndo: () => void;
  onRedo: () => void;
  canUndo: boolean;
  canRedo: boolean;
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
  workspaceSaveError,
  workspaceFileCount,
  onExportWorkspace,
  onImportWorkspace,
  onResetWorkspace,
  onSaveWorkspace,
  onUndo,
  onRedo,
  canUndo,
  canRedo,
  entryProject,
  workspaceFiles,
  paramOverrides,
  onRuntimeReady,
}: Props) {
  const draftStateClass = workspaceSaveError ? 'status-pill--bad' : hasDirtyParamDrafts ? 'status-pill--warn' : 'status-pill--ok';

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
        onSaveWorkspace={onSaveWorkspace}
      />

      <div className="header-right">
        <div className="topbar__status" aria-label="Project status">
          <div className={`status-pill ${validation.ok ? 'status-pill--ok' : 'status-pill--bad'}`}>
            {validation.ok ? 'Valid' : 'Invalid'}
          </div>
          <div
            className={`status-pill ${draftStateClass}`}
            data-testid="workspace-save-status"
            title={workspaceSaveError ?? `${workspaceFileCount} workspace files`}
          >
            {workspaceSaveError ? 'Save failed' : hasDirtyParamDrafts ? `Unsaved edits (${dirtyParamCount})` : 'Saved locally'}
          </div>
        </div>
        <span className="header-divider" />
        <div className="topbar__workspace-actions">
        <button
          className="btn btn--ghost topbar__icon-btn"
          data-testid="topbar-undo"
          disabled={!canUndo}
          onClick={onUndo}
          title="Undo"
          type="button"
        >
          <i className="fa-solid fa-rotate-left" aria-hidden="true" />
        </button>
        <button
          className="btn btn--ghost topbar__icon-btn"
          data-testid="topbar-redo"
          disabled={!canRedo}
          onClick={onRedo}
          title="Redo"
          type="button"
        >
          <i className="fa-solid fa-rotate-right" aria-hidden="true" />
        </button>
        <label className="btn btn--ghost">
          Import
          <input
            data-testid="topbar-import-input"
            accept="application/json"
            onChange={event => {
              void onImportWorkspace(event.target.files?.[0] ?? null);
              event.target.value = '';
            }}
            type="file"
          />
        </label>
        <button className="btn btn--ghost" data-testid="topbar-export" onClick={onExportWorkspace} type="button">
          Export
        </button>
        <button className="btn btn--ghost" data-testid="topbar-reset" disabled={!hasWorkspaceDrafts} onClick={onResetWorkspace} type="button">
          Reset
        </button>
        </div>
      </div>
    </header>
  );
}
