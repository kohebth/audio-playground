import { AtomCatalogPanel } from './AtomCatalogPanel';
import { CompatibilityExportPanel } from './CompatibilityExportPanel';
import { DraftExportPanel } from './DraftExportPanel';
import { PreviewPanel } from './PreviewPanel';
import type { ProjectNodeData } from '../lib/projectGraph';
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
  hasDirtyParamDrafts: boolean;
  selectedWorkspaceFile: WorkspaceFile;
  paramDrafts: ParamDrafts;
  paramOverrides: ParamOverride[];
  onParamChange: (instanceId: string, paramKey: string, value: string) => void;
  onParamReset: (instanceId: string, paramKey: string, value: string) => void;
  onResetUnitParams: (instanceId: string) => void;
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
  hasDirtyParamDrafts,
  selectedWorkspaceFile,
  paramDrafts,
  paramOverrides,
  onParamChange,
  onParamReset,
  onResetUnitParams,
  onWorkspaceFileChange,
}: Props) {
  const selectedDirtyCount =
    selectedNode?.kind === 'unit' ? countDirtyParamsForInstance(selectedNode.instance, paramDrafts) : 0;
  const readinessMessage = hasDirtyParamDrafts ? 'Out of sync with local edits' : 'Synchronized with local draft state';
  const commandState = hasDirtyParamDrafts ? 'frozen' : 'current';
  const isProjectView = inspectorView === 'project';
  const isAtomView = inspectorView === 'atom';
  const isContractView = inspectorView === 'contract';

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
            render={render}
            paramOverrides={paramOverrides}
            selectedInstanceId={selectedNode?.kind === 'unit' ? selectedNode.instance.id : null}
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

                  return (
                    <div key={param.key} className={`param-list__row ${dirty ? 'param-list__row--dirty' : ''}`}>
                      <label className="param-list__field">
                        <span>{param.key}</span>
                        <input
                          aria-label={`${selectedNode.instance.id} ${param.key}`}
                          inputMode="decimal"
                          onChange={event => onParamChange(selectedNode.instance.id, param.key, event.target.value)}
                          value={value}
                        />
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
            <div className="inspector-block__label">Workspace Draft</div>
            <div className="workspace-editor__meta">
              <strong>{selectedWorkspaceFile.path}</strong>
              <span>{selectedWorkspaceFile.role}</span>
            </div>
            <textarea
              aria-label={`Workspace file ${selectedWorkspaceFile.path}`}
              className="workspace-editor"
              onChange={event => onWorkspaceFileChange(selectedWorkspaceFile.path, event.target.value)}
              spellCheck={false}
              value={selectedWorkspaceFile.content}
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
