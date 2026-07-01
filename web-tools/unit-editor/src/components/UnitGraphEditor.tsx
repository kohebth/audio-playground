import { useState } from 'react';

import type { AtomCatalog, AtomCatalogAtom, UnitInspect, WorkspaceFile } from '../lib/backendSamples';

type Props = {
  unit: UnitInspect;
  catalog: AtomCatalog;
  workspaceFile: WorkspaceFile;
  onWorkspaceFileChange: (path: string, content: string) => void;
};

function fieldLines(title: string, fields: AtomCatalogAtom['inputs']): string[] {
  if (fields.length === 0) return [];
  return [`      ${title}:`, ...fields.map(field => `        ${field.name}: TODO_${field.type.toUpperCase()}`)];
}

function atomSnippet(atom: AtomCatalogAtom): string {
  return [
    `    - id: ${atom.name}_draft`,
    `      atom: ${atom.name}`,
    ...fieldLines('in', atom.inputs),
    ...fieldLines('out', atom.outputs),
    ...fieldLines('config', atom.config),
  ].join('\n');
}

function insertAtomSnippet(content: string, atom: AtomCatalogAtom): string {
  const snippet = `${atomSnippet(atom)}\n`;
  const marker = '\n  nodes:\n';
  const index = content.indexOf(marker);
  if (index < 0) return `${content.trimEnd()}\n\n# Draft atom node\n${snippet}`;

  const insertAt = index + marker.length;
  return `${content.slice(0, insertAt)}${snippet}${content.slice(insertAt)}`;
}

export function UnitGraphEditor({ unit, catalog, workspaceFile, onWorkspaceFileChange }: Props) {
  const [selectedAtomName, setSelectedAtomName] = useState(catalog.atoms[0]?.name ?? '');
  const selectedAtom = catalog.atoms.find(atom => atom.name === selectedAtomName) ?? catalog.atoms[0];
  const canEdit = workspaceFile.role === 'unit' && Boolean(selectedAtom);
  const warning = workspaceFile.role !== 'unit'
    ? 'Select a unit YAML draft before inserting atoms.'
    : selectedAtom && selectedAtom.outputs.length === 0
      ? 'Selected atom lacks backend output binding metadata.'
      : 'Draft insertion uses backend atom fields and TODO bindings.';

  return (
    <section className="unit-graph">
      <div className="unit-graph__summary">
        <span>{unit.graph.signals.length} signals</span>
        <span>{unit.graph.nodes.length} nodes</span>
        <span>{unit.params.length} params</span>
      </div>

      <div className="unit-graph__nodes">
        {unit.graph.nodes.map(node => (
          <div key={node.id}>
            <span>{node.id}</span>
            <strong>{node.atom}</strong>
          </div>
        ))}
      </div>

      <label className="unit-graph__insert">
        <span>Insert atom</span>
        <select onChange={event => setSelectedAtomName(event.target.value)} value={selectedAtomName}>
          {catalog.atoms.map(atom => (
            <option key={atom.name} value={atom.name}>
              {atom.name}
            </option>
          ))}
        </select>
      </label>

      <button
        className="btn btn--ghost"
        disabled={!canEdit}
        onClick={() => selectedAtom && onWorkspaceFileChange(workspaceFile.path, insertAtomSnippet(workspaceFile.content, selectedAtom))}
        type="button"
      >
        Add atom draft
      </button>
      <p>{warning}</p>
    </section>
  );
}
