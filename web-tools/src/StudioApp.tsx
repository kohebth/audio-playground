import { useCallback, useEffect, useMemo, useState } from 'react';
import { useLocation, useNavigate } from 'react-router-dom';

import { EditorWorkspace } from './App';
import { ProjectHome } from './components/ProjectHome';
import { backendSamples, initialWorkspaceFiles } from './lib/backendSamples';
import {
  apgPackageFileName,
  createApgProjectPackageFromFiles,
  parseApgProjectPackage,
  serializeApgProjectPackage,
  type ApgProjectPackage,
  type StudioMode,
} from './lib/projectPackage';
import { createEmptyProjectPackage } from './lib/projectTemplates';
import { evaluateWorkspaceReadiness } from './lib/projectReadiness';
import type { PersonalUnitRecord, UnitPreset } from './lib/presetLibrary';
import { createStudioRepository } from './lib/studioRepository';
import { findMigratableBrowserWorkspace } from './lib/workspaceMigrations';
import type { WorkspacePayload } from './lib/workspacePersistence';

const MODE_STORAGE_KEY = 'apg.studio.mode.v1';
const LAST_PROJECT_STORAGE_KEY = 'apg.studio.last-project.v1';
const LEGACY_KEYS = ['apg.unit-editor.workspace.v2', 'apg.unit-editor.workspace.v1'];

function newId(prefix = 'project'): string {
  return globalThis.crypto?.randomUUID?.() ?? `${prefix}-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

function storedMode(): StudioMode {
  if (typeof window === 'undefined') return 'simple';
  return window.localStorage.getItem(MODE_STORAGE_KEY) === 'pro' ? 'pro' : 'simple';
}

function starterProject(now = new Date().toISOString()): ApgProjectPackage {
  const project = createApgProjectPackageFromFiles(backendSamples.project.file, initialWorkspaceFiles, {
    id: 'guitar-pedalboard-starter',
    name: 'Guitar Pedalboard',
    description: 'A complete live guitar signal chain.',
    createdAt: now,
    updatedAt: now,
  });
  return { ...project, readiness: evaluateWorkspaceReadiness(project.workspace, project.readiness) };
}

function downloadProject(project: ApgProjectPackage): void {
  const url = URL.createObjectURL(new Blob([serializeApgProjectPackage(project)], { type: 'application/json' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = apgPackageFileName(project);
  link.click();
  URL.revokeObjectURL(url);
}

export default function StudioApp() {
  const location = useLocation();
  const navigate = useNavigate();
  const repository = useMemo(() => createStudioRepository(), []);
  const [projects, setProjects] = useState<ApgProjectPackage[]>([]);
  const [activeProject, setActiveProject] = useState<ApgProjectPackage | null>(null);
  const [personalPresets, setPersonalPresets] = useState<UnitPreset[]>([]);
  const [personalUnits, setPersonalUnits] = useState<PersonalUnitRecord[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [mode, setModeState] = useState<StudioMode>(storedMode);
  const isHome = location.pathname === '/' || location.pathname === '/home';

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      try {
        let stored = await repository.listProjects();
        if (stored.length === 0) {
          const migrated = typeof window === 'undefined' ? null : findMigratableBrowserWorkspace(window.localStorage, {
            id: newId('recovered'),
            name: 'Recovered Project',
            mode: storedMode(),
          });
          const initial = migrated?.project ?? starterProject();
          await repository.saveProject(initial);
          if (migrated && typeof window !== 'undefined') {
            for (const key of LEGACY_KEYS) window.localStorage.removeItem(key);
          }
          stored = [initial];
        }
        if (cancelled) return;
        setProjects(stored);
        const [presets, units] = await Promise.all([
          repository.listPersonalPresets(),
          repository.listPersonalUnits(),
        ]);
        if (cancelled) return;
        setPersonalPresets(presets);
        setPersonalUnits(units);
        const lastId = typeof window === 'undefined' ? null : window.localStorage.getItem(LAST_PROJECT_STORAGE_KEY);
        setActiveProject(stored.find(project => project.manifest.id === lastId) ?? stored[0] ?? null);
      } catch (caught) {
        if (!cancelled) setError(caught instanceof Error ? caught.message : 'Unable to open local projects.');
      } finally {
        if (!cancelled) setLoading(false);
      }
    })();
    return () => { cancelled = true; };
  }, [repository]);

  const setMode = useCallback((nextMode: StudioMode) => {
    setModeState(nextMode);
    window.localStorage.setItem(MODE_STORAGE_KEY, nextMode);
    setActiveProject(current => {
      if (!current) return current;
      const next = { ...current, manifest: { ...current.manifest, lastMode: nextMode } };
      void repository.saveProject(next);
      return next;
    });
  }, [repository]);

  const openProject = useCallback((project: ApgProjectPackage) => {
    setActiveProject(project);
    window.localStorage.setItem(LAST_PROJECT_STORAGE_KEY, project.manifest.id);
    navigate('/projects');
  }, [navigate]);

  const createProject = useCallback((name: string) => {
    const created = createEmptyProjectPackage({ id: newId(), name, mode });
    const project = { ...created, readiness: evaluateWorkspaceReadiness(created.workspace, created.readiness) };
    void repository.saveProject(project).then(() => {
      setProjects(current => [project, ...current]);
      openProject(project);
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to create the project.'));
  }, [mode, openProject, repository]);

  const duplicateProject = useCallback((source: ApgProjectPackage) => {
    const now = new Date().toISOString();
    const copy: ApgProjectPackage = {
      ...structuredClone(source),
      manifest: {
        ...source.manifest,
        id: newId('copy'),
        name: `${source.manifest.name} Copy`,
        createdAt: now,
        updatedAt: now,
      },
    };
    void repository.saveProject(copy).then(() => setProjects(current => [copy, ...current]))
      .catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to duplicate the project.'));
  }, [repository]);

  const deleteProject = useCallback((project: ApgProjectPackage) => {
    if (!window.confirm(`Delete “${project.manifest.name}” from this browser? This cannot be undone.`)) return;
    void repository.deleteProject(project.manifest.id).then(() => {
      setProjects(current => current.filter(item => item.manifest.id !== project.manifest.id));
      setActiveProject(current => current?.manifest.id === project.manifest.id ? null : current);
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to delete the project.'));
  }, [repository]);

  const importProject = useCallback((file: File) => {
    void file.text().then(text => {
      const imported = parseApgProjectPackage(text);
      const conflict = projects.some(project => project.manifest.id === imported.manifest.id);
      const now = new Date().toISOString();
      const project = conflict ? {
        ...imported,
        manifest: { ...imported.manifest, id: newId('import'), name: `${imported.manifest.name} Imported`, updatedAt: now },
      } : imported;
      return repository.saveProject(project).then(() => setProjects(current => [
        project,
        ...current.filter(item => item.manifest.id !== project.manifest.id),
      ]));
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'That .apg project could not be imported.'));
  }, [projects, repository]);

  const updateWorkspace = useCallback((workspace: WorkspacePayload) => {
    setActiveProject(current => {
      if (!current) return current;
      const updated: ApgProjectPackage = {
        ...current,
        manifest: { ...current.manifest, updatedAt: new Date().toISOString(), lastMode: mode },
        workspace,
        readiness: evaluateWorkspaceReadiness(workspace, { ...current.readiness, preview: 'unknown' }),
      };
      void repository.saveProject(updated).then(() => {
        setProjects(items => [updated, ...items.filter(item => item.manifest.id !== updated.manifest.id)]);
      }).catch(caught => setError(caught instanceof Error ? caught.message : 'Local project save failed.'));
      return updated;
    });
  }, [mode, repository]);

  const updateActivePackage = useCallback((update: (project: ApgProjectPackage) => ApgProjectPackage) => {
    setActiveProject(current => {
      if (!current) return current;
      const updated = update(current);
      if (updated === current) return current;
      void repository.saveProject(updated).then(() => {
        setProjects(items => [updated, ...items.filter(item => item.manifest.id !== updated.manifest.id)]);
      }).catch(caught => setError(caught instanceof Error ? caught.message : 'Local project save failed.'));
      return updated;
    });
  }, [repository]);

  const savePersonalPreset = useCallback((preset: UnitPreset) => {
    void repository.savePersonalPreset(preset).then(async () => {
      setPersonalPresets(await repository.listPersonalPresets());
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to save that preset.'));
  }, [repository]);

  const deletePersonalPreset = useCallback((id: string) => {
    void repository.deletePersonalPreset(id).then(() => {
      setPersonalPresets(items => items.filter(item => item.id !== id));
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to delete that preset.'));
  }, [repository]);

  const savePersonalUnit = useCallback((unit: PersonalUnitRecord) => {
    void repository.savePersonalUnit(unit).then(async () => {
      setPersonalUnits(await repository.listPersonalUnits());
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to save that personal unit.'));
  }, [repository]);

  const deletePersonalUnit = useCallback((id: string) => {
    void repository.deletePersonalUnit(id).then(() => {
      setPersonalUnits(items => items.filter(item => item.id !== id));
    }).catch(caught => setError(caught instanceof Error ? caught.message : 'Unable to delete that personal unit.'));
  }, [repository]);

  const exportActiveProject = useCallback((workspace: WorkspacePayload) => {
    if (!activeProject) return;
    const project = {
      ...activeProject,
      manifest: { ...activeProject.manifest, updatedAt: new Date().toISOString(), lastMode: mode },
      workspace,
      readiness: evaluateWorkspaceReadiness(workspace, activeProject.readiness),
    };
    downloadProject(project);
    updateActivePackage(() => project);
  }, [activeProject, mode, updateActivePackage]);

  if (isHome) {
    return (
      <ProjectHome
        error={error}
        loading={loading}
        mode={mode}
        onCreate={createProject}
        onDelete={deleteProject}
        onDuplicate={duplicateProject}
        onExport={downloadProject}
        onImport={importProject}
        onModeChange={setMode}
        onOpen={openProject}
        projects={projects}
      />
    );
  }

  if (loading || !activeProject) {
    return (
      <main className="studio-loading">
        <span>Opening project…</span>
        {error ? <p>{error}</p> : null}
        {!loading ? <button className="btn btn--primary" onClick={() => navigate('/')} type="button">Back to projects</button> : null}
      </main>
    );
  }

  return (
    <EditorWorkspace
      key={activeProject.manifest.id}
      mode={mode}
      onHome={() => navigate('/')}
      onDeletePersonalUnit={deletePersonalUnit}
      onDeletePreset={deletePersonalPreset}
      onExportProject={exportActiveProject}
      onModeChange={setMode}
      onProjectPackageChange={updateActivePackage}
      onSavePersonalUnit={savePersonalUnit}
      onSavePreset={savePersonalPreset}
      onWorkspaceChange={updateWorkspace}
      personalPresets={personalPresets}
      personalUnits={personalUnits}
      projectPackage={activeProject}
    />
  );
}
