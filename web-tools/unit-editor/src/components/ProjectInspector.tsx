import { AtomCatalogPanel } from './AtomCatalogPanel';
import { CompatibilityExportPanel } from './CompatibilityExportPanel';
import { DraftExportPanel } from './DraftExportPanel';
import { PreviewPanel } from './PreviewPanel';
import type { ProjectNodeData } from '../lib/projectGraph';
import { useEffect, useState } from 'react';
import type {
  AtomCatalog,
  BackendCommands,
  ProjectInspect,
  ProjectRoute,
  RenderResult,
  UnitInspect,
  ValidationResult,
  WorkspaceFile,
} from '../lib/backendSamples';
import { type ParamOverride } from '../lib/projectParams';
import { findAtom, type UnitGraphDraft, type UnitGraphNode } from '../lib/unitV2Graph';

type Props = {
  validation: ValidationResult;
  render: RenderResult;
  commands: BackendCommands;
  project: ProjectInspect;
  inspectorView: 'project' | 'atom' | 'contract';
  onInspectorViewChange: (next: 'project' | 'atom' | 'contract') => void;
  selectedNode: ProjectNodeData | null;
  selectedRoute: ProjectRoute | null;
  selectedRouteIndex: number | null;
  unit: UnitInspect;
  atomCatalog: AtomCatalog;
  atomCatalogManifest: Record<string, string>;
  projectFile: string;
  workspaceFiles: WorkspaceFile[];
  hasDirtyParamDrafts: boolean;
  selectedUnitFile: WorkspaceFile;
  selectedUnitGraph: UnitGraphDraft | null;
  selectedAtom: UnitGraphNode | null;
  atomClipboard: UnitGraphNode | null;
  graphEditError: string | null;
  paramOverrides: ParamOverride[];
  onAddAtom: (atomName: string) => void;
  onDuplicateInstance: (instanceId: string) => void;
  onRemoveInstance: (instanceId: string) => void;
  onRenameInstance: (instanceId: string, nextId: string) => void;
  onReorderInstance: (instanceId: string, nextIndex: number) => void;
  onUpdateRoute: (index: number, route: ProjectRoute) => void;
  onRemoveRoute: (index: number) => void;
  onReorderRoute: (index: number, nextIndex: number) => void;
  routeSources: string[];
  routeTargets: string[];
  onCopyAtom: () => void;
  onCutAtom: () => void;
  onPasteAtom: () => void;
  onRemoveAtom: () => void;
  onResetUnitParams: (instanceId: string) => void;
  onSelectAtom: (id: string) => void;
  onSelectedAtomChange: (node: UnitGraphNode, originalId?: string) => void;
  onWorkspaceFileChange: (path: string, content: string) => void;
};

function compatibilityLabel(flags: Record<string, boolean>): string {
  const enabled = Object.entries(flags)
    .filter(([, value]) => value)
    .map(([key]) => key);
  return enabled.length > 0 ? enabled.join(', ') : 'none';
}

function formatNumber(value: number): string {
  return value.toFixed(6).replace(/0+$/, '').replace(/\.$/, '');
}

function updateMapValue(
  node: UnitGraphNode,
  section: 'in' | 'out' | 'config',
  key: string,
  value: string,
): UnitGraphNode {
  return { ...node, [section]: { ...node[section], [key]: value } };
}

function removeMapValue(node: UnitGraphNode, section: 'in' | 'out' | 'config', key: string): UnitGraphNode {
  const next = { ...node[section] };
  delete next[key];
  return { ...node, [section]: next };
}

function uniqueBindingKey(values: Record<string, string>, base: string): string {
  let index = 1;
  let key = base;
  while (key in values) {
    index += 1;
    key = `${base}_${index}`;
  }
  return key;
}

export function ProjectInspector({
  validation,
  render,
  commands,
  project,
  inspectorView,
  onInspectorViewChange,
  selectedNode,
  selectedRoute,
  selectedRouteIndex,
  unit,
  atomCatalog,
  atomCatalogManifest,
  projectFile,
  workspaceFiles,
  hasDirtyParamDrafts,
  selectedUnitFile,
  selectedUnitGraph,
  selectedAtom,
  atomClipboard,
  graphEditError,
  paramOverrides,
  onAddAtom,
  onDuplicateInstance,
  onRemoveInstance,
  onRenameInstance,
  onReorderInstance,
  onUpdateRoute,
  onRemoveRoute,
  onReorderRoute,
  routeSources,
  routeTargets,
  onCopyAtom,
  onCutAtom,
  onPasteAtom,
  onRemoveAtom,
  onResetUnitParams,
  onSelectAtom,
  onSelectedAtomChange,
  onWorkspaceFileChange,
}: Props) {
  const [atomToAdd, setAtomToAdd] = useState(atomCatalog.atoms[0]?.name ?? '');
  const [renameDraft, setRenameDraft] = useState('');
  const [routeFrom, setRouteFrom] = useState('');
  const [routeTo, setRouteTo] = useState('');
  const selectedDirtyCount = selectedNode?.kind === 'unit'
    ? paramOverrides.filter(override => override.instanceId === selectedNode.instance.id).length
    : 0;
  const readinessMessage = hasDirtyParamDrafts ? 'Out of sync with local edits' : 'Synchronized with local draft state';
  const commandState = hasDirtyParamDrafts ? 'frozen' : 'current';
  const isProjectView = inspectorView === 'project';
  const isAtomView = inspectorView === 'atom';
  const isContractView = inspectorView === 'contract';
  const selectedAtomContract = selectedAtom ? findAtom(atomCatalog, selectedAtom.atom) : null;
  const unitRoutes =
    selectedNode?.kind === 'unit'
      ? project.routes.filter(route => route.from.startsWith(`${selectedNode.instance.id}.`) || route.to.startsWith(`${selectedNode.instance.id}.`))
      : [];

  useEffect(() => {
    setRenameDraft(selectedNode?.kind === 'unit' ? selectedNode.instance.id : '');
  }, [selectedNode]);

  useEffect(() => {
    setRouteFrom(selectedRoute?.from ?? '');
    setRouteTo(selectedRoute?.to ?? '');
  }, [selectedRoute]);

  return (
    <aside className="project-inspector">
      <div className="inspector-switcher">
        <button
          className={`btn ${isProjectView ? 'btn--primary' : 'btn--ghost'}`}
          onClick={() => onInspectorViewChange('project')}
          type="button"
        >
          Project
        </button>
        <button
          className={`btn ${isAtomView ? 'btn--primary' : 'btn--ghost'}`}
          onClick={() => onInspectorViewChange('atom')}
          type="button"
        >
          Atom
        </button>
        <button
          className={`btn ${isContractView ? 'btn--primary' : 'btn--ghost'}`}
          onClick={() => onInspectorViewChange('contract')}
          type="button"
        >
          Contract
        </button>
      </div>
      <PreviewPanel
        entryProject={projectFile}
        paramOverrides={paramOverrides}
        selectedInstanceId={selectedNode?.kind === 'unit' ? selectedNode.instance.id : null}
        workspaceFiles={workspaceFiles}
      />
      {graphEditError ? (
        <div className="diagnostic-list__item project-edit-error">
          <strong>Edit blocked</strong>
          <p>{graphEditError}</p>
        </div>
      ) : null}
      {isProjectView && (
        <>
          <section className="inspector-block">
            <div className="inspector-block__label">Validation</div>
            <div className="inspector-block__meta">Readiness: {readinessMessage}</div>
            <div className="validation-line">
              <span className={`validation-dot ${validation.ok ? 'validation-dot--ok' : 'validation-dot--bad'}`} />
              <strong>{validation.ok ? 'Project is valid' : 'Project has errors'}</strong>
            </div>
            <div className="inspector-block__meta">
              {validation.errors.length} errors / {validation.warnings.length} warnings
            </div>
            {validation.errors.length === 0 && validation.warnings.length === 0 ? (
              <div className="diagnostic-empty">No diagnostics in the frozen validation sample.</div>
            ) : (
              <div className="diagnostic-list">
                {[...validation.errors, ...validation.warnings].map((diagnostic, index) => (
                  <div key={`${diagnostic.code ?? 'diagnostic'}-${index}`} className="diagnostic-list__item">
                    <strong>{diagnostic.code ?? 'diagnostic'}</strong>
                    <span>{diagnostic.path ?? diagnostic.file ?? 'project'}</span>
                    <p>{diagnostic.message ?? 'No message'}</p>
                  </div>
                ))}
              </div>
            )}
            <div className="command-panel">
              <span>Backend command ({commandState})</span>
              <code>{commands.validateProject}</code>
            </div>
          </section>

          <section className="inspector-block">
            <div className="inspector-block__label">Render Preview</div>
            <div className="inspector-block__meta">Readiness: {readinessMessage}</div>
            <div className="meter-grid">
              <div>
                <span>Peak</span>
                <strong>{formatNumber(render.output.peak)}</strong>
              </div>
              <div>
                <span>RMS</span>
                <strong>{formatNumber(render.output.rms)}</strong>
              </div>
              <div>
                <span>Frames</span>
                <strong>{render.frames}</strong>
              </div>
            </div>
            <div className="waveform" aria-label="Deterministic render samples">
              {render.output.samples.map((sample, index) => (
                <span
                  key={`${sample}-${index}`}
                  className="waveform__bar"
                  style={{ height: `${Math.max(8, Math.abs(sample) * 120)}px` }}
                />
              ))}
            </div>
            <div className="command-panel">
              <span>Backend command ({commandState})</span>
              <code>{commands.renderProject}</code>
            </div>
          </section>

          <CompatibilityExportPanel project={project} commands={commands} />

          <DraftExportPanel projectFile={projectFile} overrides={paramOverrides} />
        </>
      )}

      {isAtomView && (
        <section className="inspector-block inspector-block--selected">
          <div className="inspector-block__label">Atom Inspector</div>
          {selectedRoute ? (
            <>
              <div className="inspector-block__meta">Selected Route</div>
              <label className="project-edit-field">
                <span>From</span>
                <select aria-label="Route source" onChange={event => setRouteFrom(event.target.value)} value={routeFrom}>
                  {routeSources.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
                </select>
              </label>
              <label className="project-edit-field">
                <span>To</span>
                <select aria-label="Route target" onChange={event => setRouteTo(event.target.value)} value={routeTo}>
                  {routeTargets.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
                </select>
              </label>
              <div className="project-edit-actions">
                <button
                  disabled={selectedRouteIndex === null || !routeFrom || !routeTo}
                  onClick={() => selectedRouteIndex !== null && onUpdateRoute(selectedRouteIndex, { from: routeFrom, to: routeTo })}
                  type="button"
                >Replace</button>
                <button
                  disabled={selectedRouteIndex === null || selectedRouteIndex === 0}
                  onClick={() => selectedRouteIndex !== null && onReorderRoute(selectedRouteIndex, selectedRouteIndex - 1)}
                  type="button"
                >Up</button>
                <button
                  disabled={selectedRouteIndex === null || selectedRouteIndex >= project.routes.length - 1}
                  onClick={() => selectedRouteIndex !== null && onReorderRoute(selectedRouteIndex, selectedRouteIndex + 1)}
                  type="button"
                >Down</button>
                <button
                  disabled={selectedRouteIndex === null}
                  onClick={() => selectedRouteIndex !== null && onRemoveRoute(selectedRouteIndex)}
                  type="button"
                >Disconnect</button>
              </div>
            </>
          ) : selectedNode?.kind === 'unit' ? (
            <>
              <div className="inspector-block__meta">Selected Unit</div>
              <h2>{selectedNode.instance.id}</h2>
              <p>{selectedNode.unit.name}</p>

              <label className="project-edit-field">
                <span>Instance id</span>
                <input aria-label="Instance id" onChange={event => setRenameDraft(event.target.value)} value={renameDraft} />
              </label>
              <div className="project-edit-actions">
                <button
                  disabled={!renameDraft || renameDraft === selectedNode.instance.id}
                  onClick={() => onRenameInstance(selectedNode.instance.id, renameDraft)}
                  type="button"
                >Rename</button>
                <button onClick={() => onDuplicateInstance(selectedNode.instance.id)} type="button">Duplicate</button>
                <button
                  disabled={selectedNode.index === 0}
                  onClick={() => onReorderInstance(selectedNode.instance.id, selectedNode.index - 1)}
                  type="button"
                >Up</button>
                <button
                  disabled={selectedNode.index >= project.nodes.length - 1}
                  onClick={() => onReorderInstance(selectedNode.instance.id, selectedNode.index + 1)}
                  type="button"
                >Down</button>
                <button onClick={() => onRemoveInstance(selectedNode.instance.id)} type="button">Remove unit</button>
              </div>

              <div className="param-list__toolbar">
                <span>{selectedDirtyCount} local edits</span>
                <button
                  disabled={selectedDirtyCount === 0}
                  onClick={() => onResetUnitParams(selectedNode.instance.id)}
                  type="button"
                >
                  Reset controls
                </button>
              </div>

              <div className="unit-route-list">
                <span>Routes</span>
                {unitRoutes.length === 0 ? (
                  <strong>No project routes for this unit</strong>
                ) : (
                  unitRoutes.map(route => (
                    <code key={`${route.from}-${route.to}`}>{route.from} {'->'} {route.to}</code>
                  ))
                )}
              </div>

              <div className="compatibility">
                <span>Compatibility</span>
                <strong>{compatibilityLabel(selectedNode.unit.compatibility)}</strong>
              </div>

              <div className="compatibility">
                <span>Unit Reference</span>
                <strong>{selectedNode.unit.file}</strong>
              </div>
            </>
          ) : (
            <>
              <div className="inspector-block__meta">Selected Node</div>
              <h2>{selectedNode?.label ?? 'Nothing selected'}</h2>
              <p>{selectedNode?.detail ?? 'Select a pedalboard unit to inspect its parameters.'}</p>
            </>
          )}
        </section>
      )}

      {isContractView && (
        <>
          <section className="inspector-block">
            <div className="inspector-block__label">Atom Focus</div>
            <div className="atom-actionbar">
              <select
                aria-label="Atom to add"
                onChange={event => setAtomToAdd(event.target.value)}
                value={atomToAdd}
              >
                {atomCatalog.atoms.map(atom => (
                  <option key={atom.name} value={atom.name}>{atom.name}</option>
                ))}
              </select>
              <button onClick={() => onAddAtom(atomToAdd)} type="button">Add</button>
              <button disabled={!selectedAtom} onClick={onCopyAtom} type="button">Copy</button>
              <button disabled={!selectedAtom} onClick={onCutAtom} type="button">Cut</button>
              <button disabled={!atomClipboard} onClick={onPasteAtom} type="button">Paste</button>
              <button disabled={!selectedAtom} onClick={onRemoveAtom} type="button">Remove</button>
            </div>

            {selectedUnitGraph ? (
              <div className="atom-focus-list">
                {selectedUnitGraph.nodes.map(node => (
                  <button
                    key={node.id}
                    className={selectedAtom?.id === node.id ? 'atom-focus-list__item atom-focus-list__item--active' : 'atom-focus-list__item'}
                    onClick={() => onSelectAtom(node.id)}
                    type="button"
                  >
                    <span>{node.id}</span>
                    <strong>{node.atom}</strong>
                  </button>
                ))}
              </div>
            ) : (
              <div className="diagnostic-empty">Select a valid unit YAML draft to edit atoms.</div>
            )}
          </section>

          {selectedAtom ? (
            <section className="inspector-block">
              <div className="inspector-block__label">Selected Atom</div>
              <div className="atom-edit-grid">
                <label>
                  <span>ID</span>
                  <input
                    value={selectedAtom.id}
                    onChange={event => onSelectedAtomChange({ ...selectedAtom, id: event.target.value }, selectedAtom.id)}
                  />
                </label>
                <label>
                  <span>Type</span>
                  <select
                    value={selectedAtom.atom}
                    onChange={event => onSelectedAtomChange({ ...selectedAtom, atom: event.target.value })}
                  >
                    {atomCatalog.atoms.map(atom => (
                      <option key={atom.name} value={atom.name}>{atom.name}</option>
                    ))}
                  </select>
                </label>
              </div>

              <div className="atom-contract-hint">
                <span>{selectedAtomContract?.category ?? 'unknown'}</span>
                <strong>
                  {selectedAtomContract
                    ? `${selectedAtomContract.inputs.length} inputs / ${selectedAtomContract.outputs.length} outputs / ${selectedAtomContract.config.length} config`
                    : 'Atom metadata unavailable'}
                </strong>
              </div>

              {(['in', 'out', 'config'] as const).map(section => (
                <div key={section} className="atom-binding-editor">
                  <div className="atom-binding-editor__header">
                    <span>{section}</span>
                    <button
                      onClick={() => {
                        const key = uniqueBindingKey(selectedAtom[section], `new_${section}`);
                        onSelectedAtomChange(updateMapValue(selectedAtom, section, key, ''));
                      }}
                      type="button"
                    >
                      Add
                    </button>
                  </div>
                  {Object.entries(selectedAtom[section]).length === 0 ? (
                    <p>No bindings</p>
                  ) : (
                    Object.entries(selectedAtom[section]).map(([key, value]) => (
                      <div key={key} className="atom-binding-editor__row">
                        <code>{key}</code>
                        <input
                          aria-label={`${selectedAtom.id} ${section} ${key}`}
                          onChange={event => onSelectedAtomChange(updateMapValue(selectedAtom, section, key, event.target.value))}
                          value={value}
                        />
                        <button onClick={() => onSelectedAtomChange(removeMapValue(selectedAtom, section, key))} type="button">
                          Remove
                        </button>
                      </div>
                    ))
                  )}
                </div>
              ))}
            </section>
          ) : null}

          <section className="inspector-block">
            <div className="inspector-block__label">Workspace Draft</div>
            <div className="workspace-editor__meta">
              <strong>{selectedUnitFile.path}</strong>
              <span>{selectedUnitFile.role}</span>
            </div>
            <textarea
              aria-label={`Workspace file ${selectedUnitFile.path}`}
              className="workspace-editor"
              onChange={event => onWorkspaceFileChange(selectedUnitFile.path, event.target.value)}
              spellCheck={false}
              value={selectedUnitFile.content}
            />
          </section>

          <AtomCatalogPanel unit={unit} catalog={atomCatalog} manifest={atomCatalogManifest} />

          <section className="inspector-block">
            <div className="inspector-block__label">Backend Contract</div>
            <div className="contract-list">
              <div>
                <span>Unit sample</span>
                <strong>{unit.name}</strong>
              </div>
              <div>
                <span>Atom catalog schema</span>
                <strong>{atomCatalog.schema}</strong>
              </div>
              <div>
                <span>Atom catalog atoms</span>
                <strong>{atomCatalog.atoms.length}</strong>
              </div>
              <div>
                <span>Atom catalog bytes</span>
                <strong>{atomCatalogManifest.bytes}</strong>
              </div>
              <div>
                <span>Atom catalog fnv1a64</span>
                <strong>{atomCatalogManifest.fnv1a64}</strong>
              </div>
            </div>
          </section>
        </>
      )}
    </aside>
  );
}
