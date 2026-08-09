import { useMemo, useState, type FormEvent } from 'react';

import type { ApgProjectPackage } from '../lib/projectPackage';
import type { ProjectTemplateId } from '../lib/projectTemplates';
import { parseProjectGraphDraft } from '../lib/projectV2Graph';
import { AppLogo } from './AppLogo';

type Props = {
  projects: ApgProjectPackage[];
  loading: boolean;
  error: string | null;
  onCreate: (name: string, template: ProjectTemplateId) => void;
  onDelete: (project: ApgProjectPackage) => void;
  onDuplicate: (project: ApgProjectPackage) => void;
  onExport: (project: ApgProjectPackage) => void;
  onImport: (file: File) => void;
  onOpen: (project: ApgProjectPackage) => void;
};

function projectEffectCount(project: ApgProjectPackage): number {
  try {
    const entry = project.workspace.files.find(file => file.path === project.workspace.entryProject);
    return entry ? parseProjectGraphDraft(entry.content).nodes.length : 0;
  } catch {
    return 0;
  }
}

function relativeDate(value: string): string {
  const date = new Date(value);
  if (Number.isNaN(date.valueOf())) return 'Recently';
  return new Intl.DateTimeFormat(undefined, { month: 'short', day: 'numeric', year: 'numeric' }).format(date);
}

export function ProjectHome({
  projects,
  loading,
  error,
  onCreate,
  onDelete,
  onDuplicate,
  onExport,
  onImport,
  onOpen,
}: Props) {
  const [creating, setCreating] = useState(false);
  const [name, setName] = useState('');
  const [template, setTemplate] = useState<ProjectTemplateId>('empty');
  const sorted = useMemo(
    () => [...projects].sort((left, right) => right.manifest.updatedAt.localeCompare(left.manifest.updatedAt)),
    [projects],
  );

  const submit = (event: FormEvent) => {
    event.preventDefault();
    const nextName = name.trim();
    if (!nextName) return;
    onCreate(nextName, template);
    setName('');
    setCreating(false);
  };
  const openCreate = () => {
    setTemplate('empty');
    setCreating(true);
  };

  return (
    <main className="project-home">
      <header className="project-home__header">
        <div className="project-home__brand">
          <span><AppLogo /></span>
          <div><strong>Audio Playground</strong><small>Local studio</small></div>
        </div>
        <div className="project-home__header-actions">
          <label className="btn btn--ghost project-home__import">
            Import
            <input
              accept=".apg,.yaml,.yml,application/json"
              onChange={event => {
                const file = event.target.files?.[0];
                if (file) onImport(file);
                event.target.value = '';
              }}
              type="file"
            />
          </label>
          <button className="btn btn--primary" onClick={openCreate} type="button">New project</button>
        </div>
      </header>

      <section className="project-home__intro">
        <span className="project-home__eyebrow">Your sound, kept on this device</span>
        <h1>Pick up where you left off.</h1>
        <p>Build a pedalboard, hear it live, and package the whole project when it is ready to move.</p>
      </section>

      {error ? <p className="project-home__error" role="alert">{error}</p> : null}

      <section className="project-home__projects" aria-busy={loading}>
        <div className="project-home__section-title">
          <div><span>Projects</span><strong>{projects.length}</strong></div>
          <small>Press Save to keep changes on this device</small>
        </div>

        {loading ? (
          <div className="project-home__loading"><i /><span>Opening your studio…</span></div>
        ) : sorted.length === 0 ? (
          <button className="project-home__empty" onClick={openCreate} type="button">
            <span>+</span><strong>Create your first project</strong><small>Start with a clean, silent-free pass-through board.</small>
          </button>
        ) : (
          <div className="project-grid">
            {sorted.map((project, index) => {
              const effects = projectEffectCount(project);
              const ready = project.readiness.validation;
              return (
                <article className="project-card" key={project.manifest.id}>
                  <button className="project-card__open" onClick={() => onOpen(project)} type="button">
                    <span className={`project-card__art project-card__art--${index % 4}`} aria-hidden="true">
                      <i /><i /><i />
                    </span>
                    <span className="project-card__body">
                      <span className="project-card__meta">
                        <small>{effects} {effects === 1 ? 'effect' : 'effects'}</small>
                        <em className={`readiness-dot readiness-dot--${ready}`}>{ready === 'ready' ? 'Ready' : 'Local'}</em>
                      </span>
                      <strong>{project.manifest.name}</strong>
                      <small>Edited {relativeDate(project.manifest.updatedAt)}</small>
                    </span>
                  </button>
                  <footer>
                    <button onClick={() => onDuplicate(project)} type="button">Duplicate</button>
                    <button onClick={() => onExport(project)} type="button">Export</button>
                    <button className="danger" onClick={() => onDelete(project)} type="button">Delete</button>
                  </footer>
                </article>
              );
            })}
            <button className="project-card project-card--new" onClick={openCreate} type="button">
              <span>+</span><strong>New project</strong><small>Choose a blank rail or 8 effects</small>
            </button>
          </div>
        )}
      </section>

      {creating ? (
        <div className="dialog-backdrop" role="presentation" onMouseDown={() => setCreating(false)}>
          <form className="new-project-dialog" onMouseDown={event => event.stopPropagation()} onSubmit={submit}>
            <span className="project-home__eyebrow">New project</span>
            <h2>Name this board</h2>
            <p>Choose a starting rail. You can change the project name later.</p>
            <input
              autoFocus
              maxLength={64}
              onChange={event => setName(event.target.value)}
              placeholder="Midnight pedalboard"
              value={name}
            />
            <fieldset className="project-template-picker">
              <legend>Start from</legend>
              <div>
                <label className={template === 'empty' ? 'active' : ''}>
                  <input
                    checked={template === 'empty'}
                    name="project-template"
                    onChange={() => setTemplate('empty')}
                    type="radio"
                    value="empty"
                  />
                  <span><strong>Blank rail</strong><small>Input → Output, ready for your first effect.</small></span>
                </label>
                <label className={template === 'eight-effects' ? 'active' : ''}>
                  <input
                    checked={template === 'eight-effects'}
                    name="project-template"
                    onChange={() => setTemplate('eight-effects')}
                    type="radio"
                    value="eight-effects"
                  />
                  <span><strong>8 effects</strong><small>Gate, Phaser, Drive, Amp, Tremolo, Chorus, Delay, Reverb.</small></span>
                </label>
              </div>
            </fieldset>
            <footer>
              <button className="btn btn--ghost" onClick={() => setCreating(false)} type="button">Cancel</button>
              <button className="btn btn--primary" disabled={!name.trim()} type="submit">Create project</button>
            </footer>
          </form>
        </div>
      ) : null}
    </main>
  );
}
