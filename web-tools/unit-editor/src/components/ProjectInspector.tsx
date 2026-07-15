import { AtomCatalogPanel } from './AtomCatalogPanel';
import { CompatibilityExportPanel } from './CompatibilityExportPanel';
import { DraftExportPanel } from './DraftExportPanel';
import type { ProjectNodeData } from '../lib/projectGraph';
import { useEffect, useMemo, useState } from 'react';
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
import {
  previewAtomReplacement,
  type AtomReplacementPreview,
  type UnitGraphDraft,
  type UnitGraphNode,
} from '../lib/unitV2Graph';
import type { PerfRenderSpan, PerfSpan } from '../lib/perfTelemetry';

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
  hasDirtyParamDrafts: boolean;
  selectedUnitFile: WorkspaceFile;
  selectedUnitGraph: UnitGraphDraft | null;
  selectedAtom: UnitGraphNode | null;
  atomClipboard: UnitGraphNode | null;
  graphEditError: string | null;
  paramOverrides: ParamOverride[];
  perfSpans: PerfSpan[];
  renderPerfSpans: PerfRenderSpan[];
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
  onReplaceAtom: (nodeId: string, nextAtomName: string, preserveId: boolean) => void;
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
  hasDirtyParamDrafts,
  selectedUnitFile,
  selectedUnitGraph,
  selectedAtom,
  atomClipboard,
  graphEditError,
  paramOverrides,
  perfSpans,
  renderPerfSpans,
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
  onReplaceAtom,
  onResetUnitParams,
  onSelectAtom,
  onSelectedAtomChange,
  onWorkspaceFileChange,
}: Props) {
  const [atomToAdd, setAtomToAdd] = useState(atomCatalog.atoms[0]?.name ?? '');
  const [replacementAtom, setReplacementAtom] = useState(atomCatalog.atoms[0]?.name ?? '');
  const [preserveReplacementId, setPreserveReplacementId] = useState(false);
  const [replaceOpen, setReplaceOpen] = useState(false);
  const [renameDraft, setRenameDraft] = useState('');
  const [routeFrom, setRouteFrom] = useState('');
  const [routeTo, setRouteTo] = useState('');
  const selectedDirtyCount = selectedNode?.kind === 'unit'
    ? paramOverrides.filter(override => override.instanceId === selectedNode.instance.id).length
    : 0;
  const readinessMessage = hasDirtyParamDrafts ? 'Unsaved local edits' : 'Up to date';
  const isProjectView = inspectorView === 'project';
  const isAtomView = inspectorView === 'atom';
  const isContractView = inspectorView === 'contract';
  const atomByName = useMemo(() => new Map(atomCatalog.atoms.map(atom => [atom.name, atom])), [atomCatalog.atoms]);
  const selectedAtomContract = selectedAtom ? atomByName.get(selectedAtom.atom) ?? null : null;
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

  useEffect(() => {
    if (!selectedAtom) return;
    const candidate = atomCatalog.atoms.find(atom => atom.name !== selectedAtom.atom)?.name ?? selectedAtom.atom;
    setReplacementAtom(candidate);
    setPreserveReplacementId(false);
    setReplaceOpen(false);
  }, [atomCatalog.atoms, selectedAtom]);

  let replacementPreview: AtomReplacementPreview | null = null;
  if (selectedUnitFile.role === 'unit' && selectedAtom && replacementAtom && replacementAtom !== selectedAtom.atom) {
    try {
      replacementPreview = previewAtomReplacement(selectedUnitFile.content, atomCatalog, selectedAtom.id, replacementAtom);
    } catch {
      replacementPreview = null;
    }
  }

  return (
    <aside className="project-inspector sidebar-right">
      <div className="inspector-switcher">
        <button
          data-testid="inspector-tab-project"
          className={`inspector-tab ${isProjectView ? 'inspector-tab--active' : ''}`}
          onClick={() => onInspectorViewChange('project')}
          type="button"
        >
          Project
        </button>
        <button
          data-testid="inspector-tab-atom"
          className={`inspector-tab ${isAtomView ? 'inspector-tab--active' : ''}`}
          onClick={() => onInspectorViewChange('atom')}
          type="button"
        >
          Atom
        </button>
        <button
          data-testid="inspector-tab-contract"
          className={`inspector-tab ${isContractView ? 'inspector-tab--active' : ''}`}
          onClick={() => onInspectorViewChange('contract')}
          type="button"
        >
          Contract
        </button>
      </div>
      <div className="inspector-content">
      {graphEditError ? (
        <div className="diagnostic-list__item project-edit-error">
          <strong>Edit blocked</strong>
          <p>{graphEditError}</p>
        </div>
      ) : null}
      {isProjectView && (
        <>
          <details className="inspector-block" open>
            <summary className="inspector-block__label">Validation</summary>
            <div className="inspector-block__meta">Readiness: {readinessMessage}</div>
            <div className="validation-line">
              <span className={`validation-dot ${validation.ok ? 'validation-dot--ok' : 'validation-dot--bad'}`} />
              <strong>{validation.ok ? 'Project is valid' : 'Project has errors'}</strong>
            </div>
            <div className="inspector-block__meta">
              {validation.errors.length} errors / {validation.warnings.length} warnings
            </div>
            {validation.errors.length === 0 && validation.warnings.length === 0 ? (
              <div className="diagnostic-empty">No diagnostics.</div>
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
                    </details>

          <details className="inspector-block" open>
            <summary className="inspector-block__label">Render Preview</summary>
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
                    </details>

          <CompatibilityExportPanel project={project} />

          <DraftExportPanel projectFile={projectFile} overrides={paramOverrides} />
        </>
      )}

      {isAtomView && (
        <details className="inspector-block inspector-block--selected" open>
          <summary className="inspector-block__label">Atom Inspector</summary>
          {selectedRoute ? (
            <>
              <div className="inspector-block__meta">Selected Route</div>
              <label className="project-edit-field">
                <span>From</span>
                <select
                  aria-label="Route source"
                  data-testid="project-route-source-select"
                  onChange={event => setRouteFrom(event.target.value)}
                  value={routeFrom}
                >
                  {routeSources.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
                </select>
              </label>
              <label className="project-edit-field">
                <span>To</span>
                <select
                  aria-label="Route target"
                  data-testid="project-route-target-select"
                  onChange={event => setRouteTo(event.target.value)}
                  value={routeTo}
                >
                  {routeTargets.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
                </select>
              </label>
              <div className="project-edit-actions">
                <button
                  data-testid="inspector-route-replace"
                  disabled={selectedRouteIndex === null || !routeFrom || !routeTo}
                  onClick={() => selectedRouteIndex !== null && onUpdateRoute(selectedRouteIndex, { from: routeFrom, to: routeTo })}
                  type="button"
                >Replace</button>
                <button
                  data-testid="inspector-route-up"
                  disabled={selectedRouteIndex === null || selectedRouteIndex === 0}
                  onClick={() => selectedRouteIndex !== null && onReorderRoute(selectedRouteIndex, selectedRouteIndex - 1)}
                  type="button"
                >Up</button>
                <button
                  data-testid="inspector-route-down"
                  disabled={selectedRouteIndex === null || selectedRouteIndex >= project.routes.length - 1}
                  onClick={() => selectedRouteIndex !== null && onReorderRoute(selectedRouteIndex, selectedRouteIndex + 1)}
                  type="button"
                >Down</button>
                <button
                  data-testid="inspector-route-disconnect"
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
                  data-testid="inspector-instance-rename"
                  disabled={!renameDraft || renameDraft === selectedNode.instance.id}
                  onClick={() => onRenameInstance(selectedNode.instance.id, renameDraft)}
                  type="button"
                >Rename</button>
                <button
                  data-testid="inspector-instance-duplicate"
                  onClick={() => onDuplicateInstance(selectedNode.instance.id)}
                  type="button"
                >Duplicate</button>
                <button
                  data-testid="inspector-instance-up"
                  disabled={selectedNode.index === 0}
                  onClick={() => onReorderInstance(selectedNode.instance.id, selectedNode.index - 1)}
                  type="button"
                >Up</button>
                <button
                  data-testid="inspector-instance-down"
                  disabled={selectedNode.index >= project.nodes.length - 1}
                  onClick={() => onReorderInstance(selectedNode.instance.id, selectedNode.index + 1)}
                  type="button"
                >Down</button>
                <button
                  data-testid="inspector-instance-remove"
                  onClick={() => onRemoveInstance(selectedNode.instance.id)}
                  type="button"
                >Remove unit</button>
              </div>

              <div className="param-list__toolbar">
                <span>{selectedDirtyCount} local edits</span>
                <button
                  data-testid="inspector-reset-params"
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
        </details>
      )}

      {isContractView && (
        <>
          <details className="inspector-block" open>
            <summary className="inspector-block__label">Atom Focus</summary>
            <div className="atom-actionbar">
              <select
                data-testid="contract-atom-to-add"
                aria-label="Atom to add"
                onChange={event => setAtomToAdd(event.target.value)}
                value={atomToAdd}
              >
                {atomCatalog.atoms.map(atom => (
                  <option key={atom.name} value={atom.name}>{atom.name}</option>
                ))}
              </select>
              <button data-testid="contract-atom-add" onClick={() => onAddAtom(atomToAdd)} type="button">Add</button>
              <button data-testid="contract-atom-copy" disabled={!selectedAtom} onClick={onCopyAtom} type="button">Copy</button>
              <button data-testid="contract-atom-cut" disabled={!selectedAtom} onClick={onCutAtom} type="button">Cut</button>
              <button data-testid="contract-atom-paste" disabled={!atomClipboard} onClick={onPasteAtom} type="button">Paste</button>
              <button data-testid="contract-atom-remove" disabled={!selectedAtom} onClick={onRemoveAtom} type="button">Remove</button>
            </div>

            {selectedUnitGraph ? (
              <div className="atom-focus-list">
                {selectedUnitGraph.nodes.map(node => (
                  <button
                    key={node.id}
                    className={selectedAtom?.id === node.id ? 'atom-focus-list__item atom-focus-list__item--active' : 'atom-focus-list__item'}
                    data-testid={`contract-atom-item-${node.id}`}
                    onClick={() => onSelectAtom(node.id)}
                    type="button"
                  >
                    <span>{node.id}</span>
                    <strong>{node.atom}</strong>
                  </button>
                ))}
              </div>
            ) : (
              <div className="diagnostic-empty">Select a valid unit YAML file to edit atoms.</div>
            )}
          </details>

          {selectedAtom ? (
            <details className="inspector-block" data-testid="contract-selected-atom-panel" open>
              <summary className="inspector-block__label">Selected Atom</summary>
              <div className="atom-edit-grid">
                <label>
                  <span>ID</span>
                  <input
                    data-testid="contract-atom-id"
                    value={selectedAtom.id}
                    onChange={event => onSelectedAtomChange({ ...selectedAtom, id: event.target.value }, selectedAtom.id)}
                  />
                </label>
                <label>
                  <span>Type</span>
                  <strong className="atom-type-lock" data-testid="contract-atom-type">
                    {selectedAtom.atom}
                    <i className="fa-solid fa-lock" aria-hidden="true" />
                  </strong>
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

                <div className="atom-replace-panel">
                <button data-testid="contract-atom-replace-open" onClick={() => setReplaceOpen(open => !open)} type="button">
                  Replace atom...
                </button>
                <span>Type changes require a compatibility preview.</span>
              </div>

              {replaceOpen ? (
                <div className="atom-replace-preview">
                  <label>
                    <span>New Type</span>
                    <select
                      data-testid="contract-atom-replace-type"
                      onChange={event => setReplacementAtom(event.target.value)}
                      value={replacementAtom}
                    >
                      {atomCatalog.atoms
                        .filter(atom => atom.name !== selectedAtom.atom)
                        .map(atom => <option key={atom.name} value={atom.name}>{atom.name}</option>)}
                    </select>
                  </label>
                    <label className="atom-replace-preview__check">
                      <input
                        checked={preserveReplacementId}
                        data-testid="contract-atom-replace-preserve"
                        onChange={event => setPreserveReplacementId(event.target.checked)}
                        type="checkbox"
                      />
                    <span>Preserve instance ID</span>
                  </label>
                  {replacementPreview ? (
                    <div className="atom-replace-preview__grid">
                      <div>
                        <span>Preserved</span>
                        <strong>
                          {[...replacementPreview.preservedInputs, ...replacementPreview.preservedOutputs, ...replacementPreview.preservedConfig].join(', ') || 'none'}
                        </strong>
                      </div>
                      <div>
                        <span>Removed</span>
                        <strong>
                          {[
                            ...replacementPreview.removedInputs.map(item => `in.${item.field}`),
                            ...replacementPreview.removedOutputs.map(item => `out.${item.field}`),
                            ...replacementPreview.removedConfig.map(item => `config.${item.field}`),
                          ].join(', ') || 'none'}
                        </strong>
                      </div>
                      <div>
                        <span>Added</span>
                        <strong>
                          {[
                            ...replacementPreview.addedInputs.map(field => `in.${field}`),
                            ...replacementPreview.addedOutputs.map(field => `out.${field}`),
                            ...replacementPreview.addedConfig.map(field => `config.${field}`),
                          ].join(', ') || 'none'}
                        </strong>
                      </div>
                    </div>
                  ) : (
                    <p className="diagnostic-empty diagnostic-empty--error">Replacement preview unavailable.</p>
                  )}
                  <div className="atom-replace-preview__actions">
                    <button onClick={() => setReplaceOpen(false)} type="button">Cancel</button>
                    <button
                      data-testid="contract-atom-replace-confirm"
                      disabled={!replacementPreview}
                      onClick={() => {
                        onReplaceAtom(selectedAtom.id, replacementAtom, preserveReplacementId);
                        setReplaceOpen(false);
                      }}
                      type="button"
                    >
                      Confirm replacement
                    </button>
                  </div>
                </div>
              ) : null}

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
            </details>
          ) : null}

          <details className="inspector-block developer-diagnostics">
            <summary className="inspector-block__label">
              <i className="fa-solid fa-code" aria-hidden="true" />
              Developer Diagnostics
            </summary>
            <div className="workspace-editor__meta">
              <strong>{selectedUnitFile.path}</strong>
              <span>{selectedUnitFile.role}</span>
            </div>
            <div className="command-panel">
              <span>Validation Command</span>
              <code>{commands.validateProject}</code>
            </div>
            <div className="command-panel">
              <span>Render Command</span>
              <code>{commands.renderProject}</code>
            </div>
            {perfSpans.length > 0 ? (
              <details className="inspector-block">
                <summary className="inspector-block__label">Operation Spans ({perfSpans.length})</summary>
                <pre className="workspace-editor" style={{ marginTop: '0.5rem', whiteSpace: 'pre-wrap' }}>
                  {JSON.stringify(
                    perfSpans.map(span => ({
                      name: span.name,
                      durationMs: Number(span.durationMs.toFixed(3)),
                      at: new Date(span.at).toLocaleTimeString(),
                      meta: span.meta,
                    })),
                    null,
                    2,
                  )}
                </pre>
              </details>
            ) : null}
            {renderPerfSpans.length > 0 ? (
              <details className="inspector-block">
                <summary className="inspector-block__label">Render Spans ({renderPerfSpans.length})</summary>
                <pre className="workspace-editor" style={{ marginTop: '0.5rem', whiteSpace: 'pre-wrap' }}>
                  {JSON.stringify(
                    renderPerfSpans.map(span => ({
                      id: span.id,
                      phase: span.phase,
                      actualDurationMs: Number(span.actualDurationMs.toFixed(3)),
                      baseDurationMs: Number(span.baseDurationMs.toFixed(3)),
                      at: new Date(span.at).toLocaleTimeString(),
                    })),
                    null,
                    2,
                  )}
                </pre>
              </details>
            ) : null}
            <textarea
              aria-label={`Workspace file ${selectedUnitFile.path}`}
              className="workspace-editor"
              onChange={event => onWorkspaceFileChange(selectedUnitFile.path, event.target.value)}
              spellCheck={false}
              value={selectedUnitFile.content}
            />
          </details>

          <AtomCatalogPanel unit={unit} catalog={atomCatalog} manifest={atomCatalogManifest} onAddAtom={onAddAtom} />

          <details className="inspector-block" open>
            <summary className="inspector-block__label">Unit Contract</summary>
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
          </details>
        </>
      )}
      </div>
    </aside>
  );
}
