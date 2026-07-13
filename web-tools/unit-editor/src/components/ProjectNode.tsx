import { memo, type CSSProperties } from 'react';
import { Handle, Position, type NodeProps, type Node } from '@xyflow/react';
import type { ProjectNodeData } from '../lib/projectGraph';
import { useLiveBypass } from '../lib/liveBypass';
import { ParamKnob } from './ParamKnob';

type ProjectFlowNode = Node<ProjectNodeData, 'projectNode'>;

export const ProjectNode = memo(({ data, selected }: NodeProps<ProjectFlowNode>) => {
  const { controller } = useLiveBypass();
  const style = { '--node-color': data.color } as CSSProperties;

  if (data.kind === 'system') {
    const isInput = data.id === 'system-input';

    return (
      <div
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
  const compact = data.instance.params.length <= 1;

  return (
    <div className={`project-node node-card ${selected ? 'project-node--selected selected' : ''}`} style={style}>
      <Handle type="target" position={Position.Left} id="in" className="project-node__handle" />
      <div className={`node-pedal${compact ? '' : ' wide'}`}>
        <div className="node-pedal-header">
          <span className="pedal-type-name">{data.unit.name}</span>
          <div className="project-node__tools">
            <button
              aria-label={`${bypassed ? 'Disable' : 'Enable'} bypass for ${data.instance.id}`}
              aria-pressed={bypassed}
              className={`project-node__bypass bypass-btn ${bypassed ? 'off project-node__bypass--active' : 'on'} nodrag nopan`}
              disabled={!controller?.running}
              onClick={() => void controller?.setBypass(data.instance.id, !bypassed)}
              onPointerDown={event => event.stopPropagation()}
              title={bypassed ? 'Disable bypass' : 'Enable bypass'}
              type="button"
            >
              {bypassed ? 'BYP' : 'ON'}
            </button>
          </div>
        </div>
        <div className="node-pedal-body">
          <span className="pedal-instance">{data.instance.id}</span>
          {data.instance.params.length > 0 ? (
            <div className="project-node__knobs knobs-row" aria-label={`${data.instance.id} controls`}>
              {data.instance.params.map(param => {
                const control = data.paramControls?.find(item => item.key === param.key);
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
