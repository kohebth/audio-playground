import { memo, useEffect, type CSSProperties } from 'react';
import { Handle, Position, type NodeProps, type Node } from '@xyflow/react';
import type { ProjectNodeData } from '../lib/projectGraph';
import { useLiveBypass } from '../lib/liveBypass';
import { ParamKnob } from './ParamKnob';
import { markComponentRender } from '../lib/perfTelemetry';

type ProjectFlowNode = Node<ProjectNodeData, 'projectNode'>;

function orderParamsByUnitContract(data: Extract<ProjectNodeData, { kind: 'unit' }>) {
  const paramsByKey = new Map(data.instance.params.map(param => [param.key, param]));
  const contractKeys = new Set(data.paramControls?.map(control => control.key) ?? []);
  return [
    ...(data.paramControls ?? []).flatMap(control => {
      const param = paramsByKey.get(control.key);
      return param ? [param] : [];
    }),
    ...data.instance.params.filter(param => !contractKeys.has(param.key)),
  ];
}

export const ProjectNode = memo(({ data, selected }: NodeProps<ProjectFlowNode>) => {
  const renderId = data.kind === 'system' ? data.id : data.instance.id;
  useEffect(() => markComponentRender('ProjectNode', renderId));
  const { controller } = useLiveBypass();
  const style = { '--node-color': data.color } as CSSProperties;

  if (data.kind === 'system') {
    const isInput = data.id === 'system-input';

    return (
      <div
        data-testid={`project-node-${data.id}`}
        className={`project-node node-card project-node--system ${selected ? 'project-node--selected selected' : ''}`}
        style={style}
      >
        {!isInput && <Handle type="target" position={Position.Left} id="in" className="project-node__handle" />}
        <div className="node-system">
          <span className="node-sys-label">System</span>
          <span className="node-sys-title">{data.label}</span>
          <i
            className={`fa-solid ${isInput ? 'fa-right-to-bracket' : 'fa-right-from-bracket'} node-sys-icon`}
            aria-hidden="true"
          />
          <span className="node-sys-port">{data.detail}</span>
        </div>
        {isInput && <Handle type="source" position={Position.Right} id="out" className="project-node__handle" />}
      </div>
    );
  }

  const bypassed = controller?.bypassByInstance[data.instance.id] ?? false;
  const params = orderParamsByUnitContract(data);
  const controlsByKey = new Map(data.paramControls?.map(control => [control.key, control]) ?? []);
  const wide = params.length >= 3;

  return (
    <div
      data-testid={`project-node-${data.instance.id}`}
      className={`project-node node-card ${selected ? 'project-node--selected selected' : ''}`}
      style={style}
    >
      <Handle type="target" position={Position.Left} id="in" className="project-node__handle" />
      <div className={`node-pedal${wide ? ' wide' : ''}`}>
        <div className="node-pedal-header">
          <span className="pedal-type-name">{data.unit.name}</span>
          <div className="project-node__tools">
            <button
              data-testid={`project-node-bypass-${data.instance.id}`}
              aria-label={`Turn ${bypassed ? 'on' : 'off'} ${data.instance.id}`}
              aria-pressed={bypassed}
              className={`project-node__bypass bypass-btn ${bypassed ? 'off project-node__bypass--active' : 'on'} nodrag nopan`}
              disabled={!controller}
              onClick={() => void controller?.setBypass(data.instance.id, !bypassed)}
              onPointerDown={event => event.stopPropagation()}
              title={bypassed ? 'Turn on' : 'Turn off'}
              type="button"
            >
              {bypassed ? 'OFF' : 'ON'}
            </button>
          </div>
        </div>
        <div className="node-pedal-body">
          <span className="pedal-instance">{data.instance.id}</span>
          {params.length > 0 ? (
            <div className="project-node__knobs knobs-row" aria-label={`${data.instance.id} controls`}>
              {params.map(param => {
                const control = controlsByKey.get(param.key);
                return (
                  <ParamKnob
                    key={param.key}
                    ariaLabel={`${data.instance.id} ${param.key}`}
                    compact
                    integer={control?.type === 'int'}
                    label={control?.label ?? param.key}
                    max={control?.max}
                    min={control?.min}
                    onChange={value => data.onParamChange?.(data.instance.id, param.key, value)}
                    unit={control?.unit}
                    value={param.value}
                  />
                );
              })}
            </div>
          ) : (
            <span className="project-node__empty">No exposed controls</span>
          )}
        </div>
        <div className="node-pedal-footer" aria-hidden="true">
          <span>IN</span>
          <span>OUT</span>
        </div>
      </div>
      <Handle type="source" position={Position.Right} id="out" className="project-node__handle" />
    </div>
  );
});

ProjectNode.displayName = 'ProjectNode';
