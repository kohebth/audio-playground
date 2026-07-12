import { AtomCatalogPanel } from './AtomCatalogPanel';
import { CompatibilityExportPanel } from './CompatibilityExportPanel';
import { DraftExportPanel } from './DraftExportPanel';
import { PreviewPanel } from './PreviewPanel';
import type { ProjectNodeData } from '../lib/projectGraph';
import { type CSSProperties, type PointerEvent, useEffect, useRef, useState } from 'react';
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
import {
  countDirtyParamsForInstance,
  paramDraftKey,
  type ParamOverride,
  type ParamDrafts,
} from '../lib/projectParams';
import { findAtom, type UnitGraphDraft, type UnitGraphNode, type UnitParamDraft } from '../lib/unitV2Graph';

type Props = {
  validation: ValidationResult;
  render: RenderResult;
  commands: BackendCommands;
  project: ProjectInspect;
  inspectorView: 'project' | 'atom' | 'contract';
  onInspectorViewChange: (next: 'project' | 'atom' | 'contract') => void;
  selectedNode: ProjectNodeData | null;
  selectedRoute: ProjectRoute | null;
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
  paramDrafts: ParamDrafts;
  paramOverrides: ParamOverride[];
  onAddAtom: (atomName: string) => void;
  onCopyAtom: () => void;
  onCutAtom: () => void;
  onPasteAtom: () => void;
  onParamChange: (instanceId: string, paramKey: string, value: string) => void;
  onParamReset: (instanceId: string, paramKey: string, value: string) => void;
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

function formatDragValue(value: number): string {
  return Number.isInteger(value) ? `${Math.round(value)}` : `${Number(value.toFixed(6))}`;
}

type DragParamInputProps = {
  ariaLabel: string;
  value: string;
  min?: string;
  max?: string;
  unit?: string;
  onChange: (next: string) => void;
};

function numberOrNull(value: string | undefined): number | null {
  if (value === undefined || value.trim() === '') return null;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}

function clampValue(value: number, min: number | null, max: number | null): number {
  return Math.min(max ?? value, Math.max(min ?? value, value));
}

function percentForValue(value: string, minValue: string | undefined, maxValue: string | undefined): number {
  const current = numberOrNull(value) ?? 0;
  const min = numberOrNull(minValue) ?? 0;
  const max = numberOrNull(maxValue) ?? 1;
  if (max <= min) return 0;
  return clampValue(((current - min) / (max - min)) * 100, 0, 100);
}

function DragParamInput({ ariaLabel, value, min, max, unit, onChange }: DragParamInputProps) {
  const [draft, setDraft] = useState(value);
  const minValue = numberOrNull(min);
  const maxValue = numberOrNull(max);
  const percent = percentForValue(value, min, max);
  const outOfRange = (() => {
    const parsed = numberOrNull(draft);
    if (parsed === null) return draft.trim() !== '';
    return (minValue !== null && parsed < minValue) || (maxValue !== null && parsed > maxValue);
  })();
  const dragState = useRef<{
    pointerId: number;
    lastY: number;
    lastTime: number;
    value: number;
    integer: boolean;
  } | null>(null);

  useEffect(() => {
    setDraft(value);
  }, [value]);

  const startDrag = (event: PointerEvent<HTMLInputElement>) => {
    if (event.button !== 0 || event.detail > 1) return;

    const parsed = Number(draft);
    if (!Number.isFinite(parsed)) return;

    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);

    dragState.current = {
      pointerId: event.pointerId,
      lastY: event.clientY,
      lastTime: event.timeStamp,
      value: parsed,
      integer: Number.isInteger(parsed),
    };
  };

  const updateDrag = (event: PointerEvent<HTMLInputElement>) => {
    const state = dragState.current;
    if (!state) return;

    const dy = state.lastY - event.clientY;
    const dt = Math.max(12, event.timeStamp - state.lastTime);
    const speed = Math.abs(dy) / dt;
    const range = minValue !== null && maxValue !== null ? maxValue - minValue : 1;
    const base = state.integer ? 0.55 : Math.max(range / 220, 0.001);
    const delta = dy * base * (1 + speed * 2);
    const unclamped = state.integer ? Math.round(state.value + delta) : state.value + delta;
    const next = clampValue(unclamped, minValue, maxValue);
    const nextValue = Number.isInteger(next) ? next : Number(next.toFixed(6));

    state.lastY = event.clientY;
    state.lastTime = event.timeStamp;
    state.value = Number(nextValue);

    const formatted = formatDragValue(nextValue);
    setDraft(formatted);
    onChange(formatted);
  };

  const stopDrag = () => {
    dragState.current = null;
  };

  const commitValue = (next: string) => {
    setDraft(next);
    const parsed = numberOrNull(next);
    if (parsed === null) return;
    const bounded = clampValue(parsed, minValue, maxValue);
    onChange(formatDragValue(bounded));
  };

  return (
    <div className="param-list__control">
      <input
        aria-label={`${ariaLabel} percent`}
        className="param-list__knob-input"
        inputMode="decimal"
        onPointerCancel={stopDrag}
        onPointerDown={startDrag}
        onPointerMove={updateDrag}
        onPointerUp={stopDrag}
        readOnly
        style={{ '--knob-percent': `${percent}%` } as CSSProperties}
        value={`${Math.round(percent)}%`}
      />
      <label className="param-list__value-field">
        <input
          aria-label={ariaLabel}
          className={outOfRange ? 'param-list__value-input param-list__value-input--invalid' : 'param-list__value-input'}
          inputMode="decimal"
          onBlur={() => commitValue(draft)}
          onChange={event => {
            const next = event.target.value;
            setDraft(next);
            const parsed = numberOrNull(next);
            if (parsed === null) return;
            if ((minValue !== null && parsed < minValue) || (maxValue !== null && parsed > maxValue)) return;
            onChange(formatDragValue(parsed));
          }}
          value={draft}
        />
        {unit ? <span>{unit}</span> : null}
      </label>
    </div>
  );
}

function paramMeta(params: UnitParamDraft[] | undefined, key: string): UnitParamDraft | undefined {
  return params?.find(param => param.name === key);
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
  paramDrafts,
  paramOverrides,
  onAddAtom,
  onCopyAtom,
  onCutAtom,
  onPasteAtom,
  onParamChange,
  onParamReset,
  onRemoveAtom,
  onResetUnitParams,
  onSelectAtom,
  onSelectedAtomChange,
  onWorkspaceFileChange,
}: Props) {
  const [atomToAdd, setAtomToAdd] = useState(atomCatalog.atoms[0]?.name ?? '');
  const selectedDirtyCount =
    selectedNode?.kind === 'unit' ? countDirtyParamsForInstance(selectedNode.instance, paramDrafts) : 0;
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

          <PreviewPanel
            entryProject={projectFile}
            paramOverrides={paramOverrides}
            selectedInstanceId={selectedNode?.kind === 'unit' ? selectedNode.instance.id : null}
            workspaceFiles={workspaceFiles}
          />

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
              <h2>{selectedRoute.from}</h2>
              <p>{selectedRoute.to}</p>
            </>
          ) : selectedNode?.kind === 'unit' ? (
            <>
              <div className="inspector-block__meta">Selected Unit</div>
              <h2>{selectedNode.instance.id}</h2>
              <p>{selectedNode.unit.name}</p>

              <div className="param-list__toolbar">
                <span>{selectedDirtyCount} local edits</span>
                <button
                  disabled={selectedDirtyCount === 0}
                  onClick={() => onResetUnitParams(selectedNode.instance.id)}
                  type="button"
                >
                  Reset unit
                </button>
              </div>

              <div className="param-list">
                {selectedNode.instance.params.map(param => {
                  const draftKey = paramDraftKey(selectedNode.instance.id, param.key);
                  const value = paramDrafts[draftKey] ?? param.value;
                  const dirty = value !== param.value;
                  const meta = paramMeta(selectedUnitGraph?.params, param.key);

                  return (
                    <div key={param.key} className={`param-list__row ${dirty ? 'param-list__row--dirty' : ''}`}>
                      <label className="param-list__field">
                        <span>{meta?.ui?.label ?? param.key}</span>
                        <DragParamInput
                          ariaLabel={`${selectedNode.instance.id} ${param.key}`}
                          max={meta?.max}
                          min={meta?.min}
                          onChange={next => onParamChange(selectedNode.instance.id, param.key, next)}
                          unit={meta?.ui?.unit}
                          value={value}
                        />
                        {meta?.min !== undefined && meta.max !== undefined ? (
                          <small>{meta.min} to {meta.max}</small>
                        ) : null}
                      </label>
                      <button
                        disabled={!dirty}
                        onClick={() => onParamReset(selectedNode.instance.id, param.key, param.value)}
                        type="button"
                      >
                        Reset
                      </button>
                    </div>
                  );
                })}
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
            {graphEditError ? <div className="diagnostic-list__item"><strong>Graph edit blocked</strong><p>{graphEditError}</p></div> : null}
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
