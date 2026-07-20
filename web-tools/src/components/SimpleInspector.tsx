import { useState, type FormEvent } from 'react';

import type { ProjectNodeData } from '../lib/projectGraph';
import type { UnitPreset } from '../lib/presetLibrary';

type Props = {
  selectedNode: ProjectNodeData | null;
  onDuplicate: (instanceId: string) => void;
  onRemove: (instanceId: string) => void;
  onOpenPro: () => void;
  presets: UnitPreset[];
  onApplyPreset: (preset: UnitPreset) => void;
  onDeletePreset: (id: string) => void;
  onSavePreset: (name: string) => void;
  onSaveToLibrary: () => void;
};

export function SimpleInspector({
  selectedNode,
  onDuplicate,
  onRemove,
  onOpenPro,
  presets,
  onApplyPreset,
  onDeletePreset,
  onSavePreset,
  onSaveToLibrary,
}: Props) {
  const [presetName, setPresetName] = useState('');
  const savePreset = (event: FormEvent) => {
    event.preventDefault();
    const name = presetName.trim();
    if (!name) return;
    onSavePreset(name);
    setPresetName('');
  };
  if (!selectedNode || selectedNode.kind !== 'unit') {
    return (
      <aside className="simple-inspector" data-tour="inspector">
        <span className="simple-inspector__eyebrow">Board tips</span>
        <h2>Choose an effect</h2>
        <p>Select a pedal on the board to see its details, or add one from the library.</p>
        <ol>
          <li><i>1</i><span>Add an effect</span></li>
          <li><i>2</i><span>Press Play</span></li>
          <li><i>3</i><span>Shape your sound</span></li>
        </ol>
      </aside>
    );
  }

  const { instance, unit, paramControls } = selectedNode;
  return (
    <aside className="simple-inspector" data-tour="inspector">
      <span className="simple-inspector__eyebrow">Selected effect</span>
      <h2>{unit.name.replace(/_/g, ' ')}</h2>
      <p className="simple-inspector__instance">{instance.id}</p>
      <div className="simple-inspector__summary">
        <div><strong>{paramControls?.length ?? 0}</strong><span>controls</span></div>
        <div><strong>{unit.compatibility.wasm_realtime ? 'Live' : 'Offline'}</strong><span>preview</span></div>
      </div>
      <p>Use the knobs directly on the pedal. Changes are heard immediately and saved to this project.</p>
      <section className="simple-presets">
        <div className="simple-presets__title"><strong>Presets</strong><span>{presets.length}</span></div>
        <div className="simple-presets__list">
          {presets.map(preset => (
            <div className="simple-preset" key={preset.id}>
              <button onClick={() => onApplyPreset(preset)} title={preset.description} type="button">
                <span>{preset.name}</span><small>{preset.scope === 'built-in' ? 'Factory' : 'Yours'}</small>
              </button>
              {preset.scope === 'personal' ? (
                <button aria-label={`Delete ${preset.name}`} onClick={() => onDeletePreset(preset.id)} type="button">×</button>
              ) : null}
            </div>
          ))}
          {presets.length === 0 ? <p>No presets for this effect yet.</p> : null}
        </div>
        <form onSubmit={savePreset}>
          <input aria-label="Preset name" onChange={event => setPresetName(event.target.value)} placeholder="Save this sound as…" value={presetName} />
          <button disabled={!presetName.trim()} type="submit">Save</button>
        </form>
      </section>
      <div className="simple-inspector__actions">
        <button onClick={() => onDuplicate(instance.id)} type="button">Duplicate</button>
        <button onClick={onOpenPro} type="button">Edit details</button>
        <button onClick={onSaveToLibrary} type="button">Save to library</button>
        <button className="danger" onClick={() => onRemove(instance.id)} type="button">Remove</button>
      </div>
    </aside>
  );
}
