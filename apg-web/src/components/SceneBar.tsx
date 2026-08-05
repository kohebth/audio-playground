import { useEffect, useState, type FormEvent } from 'react';

import type { ProjectScene } from '../lib/backendSamples';

type Props = {
  scenes: ProjectScene[];
  activeScene: string | null;
  onApply: (name: string) => void;
  onDelete: (name: string) => void;
  onRename: (name: string, nextName: string) => void;
  onSave: (name: string) => void;
};

export function SceneBar({ scenes, activeScene, onApply, onDelete, onRename, onSave }: Props) {
  const [createOpen, setCreateOpen] = useState(false);
  const [sceneName, setSceneName] = useState('');
  const [editing, setEditing] = useState<string | null>(null);
  const [renameDraft, setRenameDraft] = useState('');

  useEffect(() => {
    setEditing(null);
    setRenameDraft('');
  }, [scenes.length]);

  const saveScene = (event: FormEvent) => {
    event.preventDefault();
    const name = sceneName.trim();
    if (!name) return;
    onSave(name);
    setSceneName('');
    setCreateOpen(false);
  };

  return (
    <footer className="scene-bar" data-testid="scene-bar">
      <div className="scene-bar__label">
        <i className="fa-solid fa-bolt" aria-hidden="true" />
        <span><strong>Scenes</strong><small>Recall a complete sound</small></span>
      </div>
      <div className="scene-bar__list" aria-label="Project scenes">
        {scenes.map(scene => (
          <div className={`scene-chip ${activeScene === scene.name ? 'active' : ''}`} key={scene.name}>
            {editing === scene.name ? (
              <form
                onSubmit={event => {
                  event.preventDefault();
                  if (renameDraft.trim()) onRename(scene.name, renameDraft.trim());
                  setEditing(null);
                }}
              >
                <input aria-label={`Rename ${scene.name}`} autoFocus onChange={event => setRenameDraft(event.target.value)} value={renameDraft} />
              </form>
            ) : (
              <button data-testid={`scene-apply-${scene.name}`} onClick={() => onApply(scene.name)} type="button">
                <span>{scene.name}</span><small>{Object.keys(scene.bypass).length} switches</small>
              </button>
            )}
            <button
              aria-label={`Edit ${scene.name}`}
              className="scene-chip__edit"
              onClick={() => {
                setEditing(scene.name);
                setRenameDraft(scene.name);
              }}
              type="button"
            >•••</button>
            {editing === scene.name ? (
              <button aria-label={`Delete ${scene.name}`} className="scene-chip__delete" onClick={() => onDelete(scene.name)} type="button">×</button>
            ) : null}
          </div>
        ))}
        {scenes.length === 0 ? <span className="scene-bar__empty">No scenes yet</span> : null}
      </div>
      {createOpen ? (
        <form className="scene-bar__create" onSubmit={saveScene}>
          <input aria-label="Scene name" autoFocus onChange={event => setSceneName(event.target.value)} placeholder="Scene name" value={sceneName} />
          <button disabled={!sceneName.trim()} type="submit">Save</button>
          <button aria-label="Cancel scene" onClick={() => setCreateOpen(false)} type="button">×</button>
        </form>
      ) : (
        <button className="scene-bar__save" data-testid="scene-save-open" onClick={() => setCreateOpen(true)} type="button">
          <i className="fa-solid fa-plus" aria-hidden="true" /> Save current
        </button>
      )}
    </footer>
  );
}
