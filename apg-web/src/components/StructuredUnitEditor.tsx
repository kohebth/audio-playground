import { useEffect, useState, type FormEvent, type ReactNode } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import {
  addUnitPort,
  removeUnitParam,
  updateUnitCompatibility,
  updateUnitDefinition,
  updateUnitParam,
  updateUnitPort,
  type UnitGraphDraft,
  type UnitParamDraft,
  type UnitPortsDraft,
} from '../lib/unitV2Graph';

type Props = {
  file: WorkspaceFile;
  unit: UnitGraphDraft;
  ports: UnitPortsDraft;
  onChange: (content: string) => void;
  onReorderParam: (name: string, nextIndex: number) => void;
  onSaveToLibrary: () => void;
};

type DraftFieldProps = {
  ariaLabel: string;
  value: string;
  onCommit: (value: string) => void;
  multiline?: boolean;
  placeholder?: string;
};

function DraftField({ ariaLabel, value, onCommit, multiline = false, placeholder }: DraftFieldProps) {
  const [draft, setDraft] = useState(value);
  useEffect(() => setDraft(value), [value]);
  const commit = () => {
    if (draft !== value) onCommit(draft);
  };
  if (multiline) {
    return (
      <textarea
        aria-label={ariaLabel}
        onBlur={commit}
        onChange={event => setDraft(event.target.value)}
        placeholder={placeholder}
        rows={3}
        value={draft}
      />
    );
  }
  return (
    <input
      aria-label={ariaLabel}
      onBlur={commit}
      onChange={event => setDraft(event.target.value)}
      onKeyDown={event => {
        if (event.key === 'Enter') event.currentTarget.blur();
      }}
      placeholder={placeholder}
      value={draft}
    />
  );
}

function Field({ label, children }: { label: string; children: ReactNode }) {
  return <label className="structured-field"><span>{label}</span>{children}</label>;
}

const PARAM_TYPES = ['float', 'int', 'uint', 'bool', 'enum'];
const TARGETS = ['desktop_full', 'wasm_realtime', 'm7_static', 'offline_render'];

function nextName(existing: readonly string[], base: string): string {
  let index = 1;
  let candidate = base;
  while (existing.includes(candidate)) candidate = `${base}_${++index}`;
  return candidate;
}

export function StructuredUnitEditor({ file, unit, ports, onChange, onReorderParam, onSaveToLibrary }: Props) {
  const [error, setError] = useState<string | null>(null);
  const [newParamName, setNewParamName] = useState('');
  const apply = (operation: (content: string) => string) => {
    try {
      onChange(operation(file.content));
      setError(null);
    } catch (caught) {
      setError(caught instanceof Error ? caught.message : 'That edit could not be applied.');
    }
  };
  const updateParam = (originalName: string, param: UnitParamDraft) => {
    apply(content => updateUnitParam(content, originalName, param));
  };
  const addParam = (event: FormEvent) => {
    event.preventDefault();
    const name = newParamName.trim();
    if (!name) return;
    apply(content => updateUnitParam(content, null, {
      name,
      type: 'float',
      default: '0.5',
      min: '0',
      max: '1',
      smoothingMs: '10',
      ui: { label: name.replace(/_/g, ' '), control: 'knob', unit: '', display_precision: '2' },
    }));
    setNewParamName('');
  };

  return (
    <div className="structured-unit-editor" data-testid="structured-unit-editor">
      {error ? <p className="structured-unit-editor__error" role="alert">{error}</p> : null}

      <section className="structured-card">
        <header><span>Unit details</span><strong>Identity and library copy</strong></header>
        <div className="structured-grid structured-grid--two">
          <Field label="Display name">
            <DraftField ariaLabel="Unit display name" onCommit={title => apply(content => updateUnitDefinition(content, { title }))} value={unit.meta.title} />
          </Field>
          <Field label="Internal name">
            <DraftField ariaLabel="Unit internal name" onCommit={name => apply(content => updateUnitDefinition(content, { name }))} value={unit.name} />
          </Field>
          <Field label="Category">
            <DraftField ariaLabel="Unit category" onCommit={category => apply(content => updateUnitDefinition(content, { category }))} value={unit.meta.category} />
          </Field>
          <Field label="Version">
            <DraftField ariaLabel="Unit version" onCommit={version => apply(content => updateUnitDefinition(content, { version }))} value={unit.version} />
          </Field>
        </div>
        <Field label="Description">
          <DraftField ariaLabel="Unit description" multiline onCommit={description => apply(content => updateUnitDefinition(content, { description }))} value={unit.meta.description} />
        </Field>
        <button className="structured-card__library" onClick={onSaveToLibrary} type="button">
          <i className="fa-solid fa-bookmark" aria-hidden="true" /> Save this unit to Personal Library
        </button>
      </section>

      <section className="structured-card">
        <header><span>Compatibility</span><strong>Where this unit can run</strong></header>
        <div className="compatibility-toggles">
          {[...new Set([...TARGETS, ...Object.keys(unit.compatibility)])].map(target => (
            <label key={target}>
              <input
                checked={unit.compatibility[target] ?? false}
                onChange={event => apply(content => updateUnitCompatibility(content, target, event.target.checked))}
                type="checkbox"
              />
              <span>{target.replace(/_/g, ' ')}</span>
            </label>
          ))}
        </div>
      </section>

      <section className="structured-card">
        <header><span>Controls</span><strong>{unit.params.length} exposed parameters</strong></header>
        <div className="structured-param-list">
          {unit.params.map((param, index) => (
            <article className="structured-param" data-testid={`contract-param-row-${param.name}`} key={param.name}>
              <div className="structured-param__head">
                <strong>{param.ui?.label || param.name}</strong>
                <div>
                  <button aria-label={`Move ${param.name} up`} data-testid={`contract-param-${param.name}-up`} disabled={index === 0} onClick={() => onReorderParam(param.name, index - 1)} type="button">↑</button>
                  <button aria-label={`Move ${param.name} down`} data-testid={`contract-param-${param.name}-down`} disabled={index === unit.params.length - 1} onClick={() => onReorderParam(param.name, index + 1)} type="button">↓</button>
                  <button aria-label={`Remove ${param.name}`} className="danger" onClick={() => apply(content => removeUnitParam(content, param.name))} type="button">×</button>
                </div>
              </div>
              <div className="structured-grid structured-grid--three">
                <Field label="Name"><DraftField ariaLabel={`${param.name} name`} onCommit={name => updateParam(param.name, { ...param, name })} value={param.name} /></Field>
                <Field label="Label"><DraftField ariaLabel={`${param.name} label`} onCommit={label => updateParam(param.name, { ...param, ui: { ...param.ui, label } })} value={param.ui?.label ?? ''} /></Field>
                <Field label="Type">
                  <select aria-label={`${param.name} type`} onChange={event => updateParam(param.name, { ...param, type: event.target.value })} value={param.type}>
                    {PARAM_TYPES.map(type => <option key={type} value={type}>{type}</option>)}
                  </select>
                </Field>
                <Field label="Default"><DraftField ariaLabel={`${param.name} default`} onCommit={value => updateParam(param.name, { ...param, default: value })} value={param.default} /></Field>
                <Field label="Minimum"><DraftField ariaLabel={`${param.name} minimum`} onCommit={min => updateParam(param.name, { ...param, min })} value={param.min ?? ''} /></Field>
                <Field label="Maximum"><DraftField ariaLabel={`${param.name} maximum`} onCommit={max => updateParam(param.name, { ...param, max })} value={param.max ?? ''} /></Field>
                <Field label="Unit"><DraftField ariaLabel={`${param.name} unit`} onCommit={value => updateParam(param.name, { ...param, ui: { ...param.ui, unit: value } })} value={param.ui?.unit ?? ''} /></Field>
                <Field label="Smoothing ms"><DraftField ariaLabel={`${param.name} smoothing`} onCommit={smoothingMs => updateParam(param.name, { ...param, smoothingMs })} value={param.smoothingMs ?? ''} /></Field>
              </div>
            </article>
          ))}
        </div>
        <form className="structured-add" onSubmit={addParam}>
          <input aria-label="New parameter name" onChange={event => setNewParamName(event.target.value)} placeholder="new_control" value={newParamName} />
          <button disabled={!newParamName.trim()} type="submit">Add control</button>
        </form>
      </section>

      <section className="structured-card">
        <header><span>Unit ports</span><strong>One mono audio input and output</strong></header>
        <p className="structured-card__hint">Effect audio boundaries stay fixed at one mono input and one mono output. Optional control ports do not change audio routing.</p>
        <div className="structured-port-endpoints">
          {(['inputs', 'outputs'] as const).map(direction => (
            <div className="structured-port-endpoints__row" key={direction}>
              <div className="structured-port-endpoints__title">{direction === 'inputs' ? 'Input' : 'Output'}</div>
              <div className="structured-port-endpoints__list">
                {ports[direction].map(port => (
                  <span
                    className={`structured-port-endpoint-dot structured-port-endpoint-dot--${direction.slice(0, -1)}`}
                    key={`${direction}-${port.name}`}
                  >
                    <span aria-hidden="true" />
                    {port.name}
                  </span>
                ))}
              </div>
            </div>
          ))}
        </div>
        {(['inputs', 'outputs'] as const).map(direction => (
          <div className="structured-ports" key={direction}>
            <div className="structured-ports__title">
              <strong>{direction === 'inputs' ? 'Inputs' : 'Outputs'}</strong>
              <button
                onClick={() => {
                  const names = ports[direction].map(port => port.name);
                  const name = nextName(names, direction === 'inputs' ? 'input' : 'output');
                  apply(content => addUnitPort(content, direction, { name, type: 'control', signals: [] }));
                }}
                type="button"
              >+ Add port</button>
            </div>
            {ports[direction].map((port, index) => (
              <div className="structured-port" key={`${direction}-${index}`}>
                <Field label="Name"><DraftField ariaLabel={`${direction} ${index + 1} name`} onCommit={name => apply(content => updateUnitPort(content, direction, index, { ...port, name }))} value={port.name} /></Field>
                <Field label="Type">
                  <input aria-label={`${direction} ${index + 1} type`} disabled value={port.type} />
                </Field>
                <Field label="Channels"><input aria-label={`${direction} ${index + 1} channels`} disabled value={port.type === 'audio' ? '1 (mono)' : 'control'} /></Field>
              </div>
            ))}
          </div>
        ))}
      </section>

      <section className="structured-card structured-card--atoms">
        <header><span>Atom graph</span><strong>{unit.nodes.length} processing blocks</strong></header>
        <p>Select an atom on the canvas to edit its bindings. Add, connect, replace, and reorder atoms visually—source files are generated automatically.</p>
      </section>
    </div>
  );
}
