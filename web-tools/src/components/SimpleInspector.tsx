import type { ProjectNodeData } from '../lib/projectGraph';

type Props = {
  selectedNode: ProjectNodeData | null;
  onDuplicate: (instanceId: string) => void;
  onRemove: (instanceId: string) => void;
  onOpenPro: () => void;
};

export function SimpleInspector({ selectedNode, onDuplicate, onRemove, onOpenPro }: Props) {
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
      <div className="simple-inspector__actions">
        <button onClick={() => onDuplicate(instance.id)} type="button">Duplicate</button>
        <button onClick={onOpenPro} type="button">Edit details</button>
        <button className="danger" onClick={() => onRemove(instance.id)} type="button">Remove</button>
      </div>
    </aside>
  );
}
