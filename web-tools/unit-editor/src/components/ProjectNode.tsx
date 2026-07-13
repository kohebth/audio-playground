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
    return (
      <div className={`project-node project-node--system ${selected ? 'project-node--selected' : ''}`} style={style}>
        <Handle type="target" position={Position.Left} id="in" className="project-node__handle" />
        <div className="project-node__eyebrow">System</div>
        <div className="project-node__title">{data.label}</div>
        <div className="project-node__meta">{data.detail}</div>
        <Handle type="source" position={Position.Right} id="out" className="project-node__handle" />
      </div>
    );
  }

  const bypassed = controller?.bypassByInstance[data.instance.id] ?? false;

  return (
    <div className={`project-node ${selected ? 'project-node--selected' : ''}`} style={style}>
      <Handle type="target" position={Position.Left} id="in" className="project-node__handle" />
      <div className="project-node__eyebrow">{data.unit.name}</div>
      <div className="project-node__title">{data.instance.id}</div>
      <div className="project-node__meta">{data.instance.params.length} params</div>
      <button
        aria-label={`${bypassed ? 'Disable' : 'Enable'} bypass for ${data.instance.id}`}
        aria-pressed={bypassed}
        className={`project-node__bypass nodrag nopan${bypassed ? ' project-node__bypass--active' : ''}`}
        disabled={!controller?.running}
        onClick={() => void controller?.setBypass(data.instance.id, !bypassed)}
        onPointerDown={event => event.stopPropagation()}
        title={bypassed ? 'Disable bypass' : 'Enable bypass'}
        type="button"
      >
        Bypass
      </button>
      {data.instance.params.length > 0 && (
        <div className="project-node__knobs" aria-label={`${data.instance.id} controls`}>
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
      )}
      <div className="project-node__chips">
        {Object.entries(data.unit.compatibility).map(([key, enabled]) => (
          <span key={key} className={`project-node__chip ${enabled ? 'project-node__chip--ok' : ''}`}>
            {key}
          </span>
        ))}
      </div>
      <Handle type="source" position={Position.Right} id="out" className="project-node__handle" />
    </div>
  );
});

ProjectNode.displayName = 'ProjectNode';
