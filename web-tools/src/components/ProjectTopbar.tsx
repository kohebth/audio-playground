import { useCallback, useState } from 'react';

import { AppLogo } from './AppLogo';
import { PreviewPanel } from './PreviewPanel';
import type { ParamOverride } from '../lib/projectParams';
import type { WorkspaceFile } from '../lib/backendSamples';
import type {
  ApgAudioAsset,
  ProjectReadinessSnapshot,
  StudioMode,
} from '../lib/projectPackage';
import { ModeToggle } from './ModeToggle';
import { ProjectReadinessPanel } from './ProjectReadinessPanel';
import { AudioIoDrawer } from './AudioIoDrawer';
import { useLiveBypass } from '../lib/liveBypass';

type Props = {
  projectName: string;
  dirtyParamCount: number;
  hasDirtyParamDrafts: boolean;
  hasWorkspaceDrafts: boolean;
  workspaceSaveError: string | null;
  workspaceFileCount: number;
  onExportWorkspace: () => void;
  onImportWorkspace: (file: File) => void;
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
  mode: StudioMode;
  onModeChange: (mode: StudioMode) => void;
  onHome: () => void;
  onTour: () => void;
  packagedAudio: ApgAudioAsset[];
  readiness: ProjectReadinessSnapshot;
  onAudioAssetChange: (asset: ApgAudioAsset | null) => void;
  onReadinessUpdate: (update: Partial<ProjectReadinessSnapshot>) => void;
  editorError: string | null;
  onDismissEditorError: () => void;
};

export function ProjectTopbar({
  projectName,
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
  mode,
  onModeChange,
  onHome,
  onTour,
  packagedAudio,
  readiness,
  onAudioAssetChange,
  onReadinessUpdate,
  editorError,
  onDismissEditorError,
}: Props) {
  const { controller: liveAudio } = useLiveBypass();
  const [readinessOpen, setReadinessOpen] = useState(false);
  const [audioIoOpen, setAudioIoOpen] = useState(false);
  const closeAudioIo = useCallback(() => setAudioIoOpen(false), []);
  const draftStateClass = workspaceSaveError ? 'status-pill--bad' : hasDirtyParamDrafts ? 'status-pill--warn' : 'status-pill--ok';
  const readinessOk = readiness.validation === 'ready' && readiness.preview !== 'blocked';
  const readinessDiagnostic = [...readiness.diagnostics].reverse()
    .find(diagnostic => !diagnostic.code?.startsWith('APG_UI_'))
    ?? readiness.diagnostics[0];
  const readinessBlocked = readiness.validation === 'blocked' || readiness.preview === 'blocked';
  const transientAudioIssue = liveAudio?.audioIssue ?? null;
  const visibleTransientAudioIssue = transientAudioIssue?.source === 'microphone'
    && liveAudio?.inputMode !== 'microphone'
    ? null
    : transientAudioIssue;
  const audioIssue = visibleTransientAudioIssue ?? (
    liveAudio?.inputMode === 'microphone' ? liveAudio.microphoneCapability.issue : null
  );
  const audioIssueDismissible = audioIssue !== null && audioIssue === transientAudioIssue;
  const saveLabel = workspaceSaveError ? 'Retry' : hasDirtyParamDrafts ? 'Save' : 'Saved';

  return (
    <>
      <header className="topbar topbar--project app-header">
      <div className="header-left">
        <button className="topbar__home" onClick={onHome} title="All projects" type="button">
          <i className="fa-solid fa-chevron-left" aria-hidden="true" />
        </button>
        <div className="header-brand">
          <span className="topbar__logo" aria-hidden="true">
            <AppLogo />
          </span>
          <span className="header-brand-name">APG</span>
        </div>
        <span className="header-divider" />
        <div className="header-project">
          <span className="header-project-label">Active Project</span>
          <div className="header-project-name">
            <strong>{projectName}</strong>
            <span className="tag">{mode === 'simple' ? 'Pipeline' : 'Contract'}</span>
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
        studioMode="pro"
        packagedAudio={packagedAudio}
        onAudioAssetChange={onAudioAssetChange}
        onReadinessUpdate={onReadinessUpdate}
      />

      <div className="header-right">
        <ModeToggle compact mode={mode} onChange={onModeChange} />
        <button
          aria-expanded={audioIoOpen}
          aria-label="Audio I/O"
          className="topbar__audio-io"
          data-testid="audio-io-open"
          onClick={() => {
            setReadinessOpen(false);
            setAudioIoOpen(open => !open);
          }}
          title="Audio I/O"
          type="button"
        >
          <i className="fa-solid fa-sliders" aria-hidden="true" />
        </button>
        <button className="topbar__help" onClick={onTour} title="Show guided tour" type="button">?</button>
        <div className="topbar__status" aria-label="Project status">
          <button
            className={`status-pill ${readinessOk ? 'status-pill--ok' : 'status-pill--bad'}`}
            onClick={() => {
              setAudioIoOpen(false);
              setReadinessOpen(open => !open);
            }}
            type="button"
          >
            {readinessOk ? 'Ready' : readiness.validation === 'unknown' ? 'Checking' : 'Blocked'}
          </button>
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
        <button
          aria-label={workspaceSaveError ? 'Retry saving project' : hasDirtyParamDrafts ? 'Save project' : 'Project saved'}
          className="btn btn--ghost topbar__save"
          data-testid="topbar-save"
          disabled={!workspaceSaveError && !hasDirtyParamDrafts}
          onClick={onSaveWorkspace}
          title={workspaceSaveError ?? saveLabel}
          type="button"
        >
          <i className={`fa-solid ${workspaceSaveError ? 'fa-rotate' : hasDirtyParamDrafts ? 'fa-floppy-disk' : 'fa-check'}`} aria-hidden="true" />
          <span>{saveLabel}</span>
        </button>
        <label className="btn btn--ghost">
          Import
          <input
            data-testid="topbar-import-input"
            accept=".apg,application/json"
            onChange={event => {
              const file = event.target.files?.[0];
              if (file) onImportWorkspace(file);
              event.target.value = '';
            }}
            type="file"
          />
        </label>
        <button className="btn btn--ghost" data-testid="topbar-export" onClick={onExportWorkspace} type="button">
          Export .apg
        </button>
        <button className="btn btn--ghost" data-testid="topbar-reset" disabled={!hasWorkspaceDrafts} onClick={onResetWorkspace} type="button">
          Reset
        </button>
        </div>
      </div>
      <AudioIoDrawer onClose={closeAudioIo} open={audioIoOpen} />
      <ProjectReadinessPanel onClose={() => setReadinessOpen(false)} open={readinessOpen} readiness={readiness} />
      </header>
      {workspaceSaveError || editorError || readinessBlocked || audioIssue ? (
        <section aria-label="Project issues" className="project-issue-banner" data-testid="project-issue-banner">
          {workspaceSaveError ? (
            <div className="project-issue" role="alert">
              <strong>Save</strong>
              <span>{workspaceSaveError}</span>
              <button onClick={onSaveWorkspace} type="button">Retry</button>
            </div>
          ) : null}
          {editorError ? (
            <div className="project-issue" role="alert">
              <strong>Edit</strong>
              <span>{editorError}</span>
              <button aria-label="Dismiss edit error" onClick={onDismissEditorError} type="button">Dismiss</button>
            </div>
          ) : null}
          {readinessBlocked ? (
            <div className="project-issue" role="alert">
              <strong>{readiness.validation === 'blocked' ? 'Validation' : 'Preview'}</strong>
              <span>
                {readinessDiagnostic?.message ?? 'The current project cannot be prepared.'}
                {readinessDiagnostic?.code ? <small>{readinessDiagnostic.code}{readinessDiagnostic.path ? ` · ${readinessDiagnostic.path}` : ''}</small> : null}
              </span>
              <button
                onClick={() => {
                  setAudioIoOpen(false);
                  setReadinessOpen(true);
                }}
                type="button"
              >
                Details
              </button>
            </div>
          ) : null}
          {audioIssue ? (
            <div className="project-issue" role="alert">
              <strong>{audioIssue.source === 'microphone' ? 'Microphone' : 'Audio engine'}</strong>
              <span>
                {audioIssue.message}
                <small>{audioIssue.code}{audioIssue.detail ? ` · ${audioIssue.detail}` : ''}</small>
              </span>
              <button
                onClick={() => {
                  setReadinessOpen(false);
                  setAudioIoOpen(true);
                }}
                type="button"
              >
                Audio I/O
              </button>
              {audioIssueDismissible ? (
                <button aria-label="Dismiss audio error" onClick={liveAudio?.clearAudioIssue} type="button">Dismiss</button>
              ) : null}
            </div>
          ) : null}
        </section>
      ) : null}
    </>
  );
}
