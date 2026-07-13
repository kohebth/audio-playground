import { memo, type CSSProperties } from 'react';
import { Handle, Position, type NodeProps, type Node } from '@xyflow/react';
import type { ProjectNodeData } from '../lib/projectGraph';
import { ParamKnob } from './ParamKnob';

type ProjectFlowNode = Node<ProjectNodeData, 'projectNode'>;

export const ProjectNode = memo(({ data, selected }: NodeProps<ProjectFlowNode>) => {
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

  return (
    <div className={`project-node ${selected ? 'project-node--selected' : ''}`} style={style}>
      <Handle type="target" position={Position.Left} id="in" className="project-node__handle" />
      <div className="project-node__eyebrow">{data.unit.name}</div>
      <div className="project-node__title">{data.instance.id}</div>
      <div className="project-node__meta">{data.instance.params.length} params</div>
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
