import { memo, useEffect, useRef, type CSSProperties, type DragEvent } from 'react';
import { Handle, Position, type NodeProps, type Node } from '@xyflow/react';
import { PROJECT_INSTANCE_DRAG_TYPE } from '../lib/graphDragTypes';
import type { ProjectNodeData } from '../lib/projectGraph';
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

function portTop(offset: number | undefined, index: number, count: number): string {
  return offset === undefined ? `${((index + 1) * 100) / (count + 1)}%` : `${offset}px`;
}

function routingLabel(instanceId: string, role: 'panner' | 'mixer' | undefined): string | null {
  if (role !== 'panner') return null;
  return `auto pan${instanceId.match(/(\d+)(?:_\d+)?$/)?.[1] ?? ''}`;
}

export const ProjectNode = memo(({ data, selected }: NodeProps<ProjectFlowNode>) => {
  const moveDragBlocked = useRef(false);
  const renderId = data.kind === 'system' ? data.id : data.instance.id;
  useEffect(() => markComponentRender('ProjectNode', renderId));
  const style = {
    '--node-color': data.color,
    width: `${data.visualLayout.width}px`,
    height: `${data.visualLayout.height}px`,
  } as CSSProperties;

  if (data.kind === 'system') {
    const isInput = data.id === 'system-input';

    return (
      <div
        data-testid={`project-node-${data.id}`}
        className={`project-node node-card nopan project-node--system ${selected ? 'project-node--selected selected' : ''}`}
        style={style}
      >
        {!isInput && (
          <Handle
            type="target"
            position={Position.Left}
            id="output"
            className="project-node__handle"
            style={{ top: `${data.visualLayout.railTop}px` }}
          />
        )}
        <div className="node-system" style={{ width: '100%', height: '100%' }}>
          <span className="node-sys-label">System</span>
          <span className="node-sys-title">{data.label}</span>
          <i
            className={`fa-solid ${isInput ? 'fa-right-to-bracket' : 'fa-right-from-bracket'} node-sys-icon`}
            aria-hidden="true"
          />
          <span className="node-sys-port">{data.detail}</span>
        </div>
        {isInput && (
          <Handle
            type="source"
            position={Position.Right}
            id="input"
            className="project-node__handle"
            style={{ top: `${data.visualLayout.railTop}px` }}
          />
        )}
      </div>
    );
  }

  const bypassed = data.bypassed ?? false;
  const contractedParams = orderParamsByUnitContract(data);
  const controlsByKey = new Map(data.paramControls?.map(control => [control.key, control]) ?? []);
  const routingContract = data.ports?.routing;
  const routing = Boolean(data.instance.routing || routingContract);
  const movable = !routing;
  const flexibleRouting = Boolean(routingContract);
  const helperLabel = routingLabel(data.instance.id, routingContract?.role);
  const routingParamKeys = new Set(routingContract?.paths.map(path => path.levelParam) ?? []);
  const params = routingContract ? [
    ...routingContract.paths.flatMap(path => {
      const param = contractedParams.find(candidate => candidate.key === path.levelParam);
      return param ? [param] : [];
    }),
    ...contractedParams.filter(param => !routingParamKeys.has(param.key)),
  ] : contractedParams;
  const wide = !flexibleRouting && params.length >= 3;
  const inputPorts = data.ports?.inputs.length ? data.ports.inputs : ['input'];
  const outputPorts = data.ports?.outputs.length ? data.ports.outputs : ['output'];
  const renderKnob = (param: (typeof params)[number]) => {
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
  };
  const startMoveDrag = (event: DragEvent<HTMLDivElement>) => {
    const blocked = event.target instanceof Element
      && Boolean(event.target.closest('.nodrag, button, input, select, textarea'));
    if (!movable || blocked || moveDragBlocked.current) {
      event.preventDefault();
      return;
    }
    event.dataTransfer.effectAllowed = 'move';
    event.dataTransfer.setData(PROJECT_INSTANCE_DRAG_TYPE, data.instance.id);
  };

  return (
    <div
      data-testid={`project-node-${data.instance.id}`}
      className={`project-node node-card nopan ${routing ? 'project-node--routing' : 'project-node--movable'}${routingContract?.role === 'panner' ? ' project-node--routing-panner' : ''} ${bypassed ? 'project-node--bypassed' : ''} ${selected ? 'project-node--selected selected' : ''}`}
      draggable={movable}
      onDragEnd={() => { moveDragBlocked.current = false; }}
      onDragStart={startMoveDrag}
      onPointerDownCapture={event => {
        moveDragBlocked.current = event.target instanceof Element
          && Boolean(event.target.closest('.nodrag, button, input, select, textarea'));
      }}
      onPointerCancelCapture={() => { moveDragBlocked.current = false; }}
      onPointerUpCapture={() => { moveDragBlocked.current = false; }}
      style={style}
    >
      {inputPorts.map((port, index) => (
        <Handle
          aria-label={`${data.instance.id} ${port} input`}
          className="project-node__handle project-node__handle--input"
          id={port}
          key={port}
          position={Position.Left}
          style={{
            top: portTop(
              data.routingLayout?.inputTops[port] ?? data.visualLayout.railTop,
              index,
              inputPorts.length,
            ),
          }}
          title={port}
          type="target"
        />
      ))}
      <div
        className={`node-pedal${wide ? ' wide' : ''}${flexibleRouting ? ' node-pedal--routing' : ''}${routingContract?.role === 'panner' ? ' node-pedal--routing-panner' : ''}`}
        style={{ width: '100%', height: '100%' }}
      >
        <div className="node-pedal-header">
          <span className="pedal-type-name">{helperLabel ?? data.unit.name}</span>
          {routing && !flexibleRouting ? <span className="project-node__routing-badge">Always active</span> : null}
          {movable ? (
            <span aria-hidden="true" className="project-node__move-grip" title="Drag to another rail">
              <i className="fa-solid fa-grip-lines" />
            </span>
          ) : null}
        </div>
        <div className={`node-pedal-body${flexibleRouting ? ' node-pedal-body--routing' : ''}`}>
          {!flexibleRouting ? <span className="pedal-instance">{data.instance.id}</span> : null}
          {!flexibleRouting && params.length > 0 ? (
            <div className="project-node__knobs knobs-row" aria-label={`${data.instance.id} controls`}>
              {params.map(renderKnob)}
            </div>
          ) : !flexibleRouting ? (
            <span className="project-node__empty">No exposed controls</span>
          ) : null}
        </div>
        {flexibleRouting && params.length > 0 ? (
          <div className="project-node__routing-controls" aria-label={`${data.instance.id} path controls`}>
            {params.map((param, index) => (
              <div
                className="project-node__routing-control"
                key={param.key}
                style={{
                  top: portTop(
                    data.routingLayout?.controlTops[param.key],
                    index,
                    params.length,
                  ),
                }}
              >
                {renderKnob(param)}
              </div>
            ))}
          </div>
        ) : null}
        {routing ? (
          <div className="node-pedal-footer node-pedal-footer--always-on" title="Routing helpers are always active">
            <span className="node-pedal-footer__indicator" aria-hidden="true" />
            <span>ROUTING ON</span>
          </div>
        ) : (
          <button
            data-testid={`project-node-bypass-${data.instance.id}`}
            aria-label={`Turn ${bypassed ? 'on' : 'off'} ${data.instance.id}`}
            aria-pressed={!bypassed}
            className={`node-pedal-footer ${bypassed ? 'node-pedal-footer--off' : 'node-pedal-footer--on'} nodrag nopan`}
            disabled={!data.bypassAvailable}
            onClick={event => {
              event.stopPropagation();
              void data.onBypassChange?.(data.instance.id, !bypassed);
            }}
            onDoubleClick={event => event.stopPropagation()}
            onPointerDown={event => event.stopPropagation()}
            title={bypassed ? 'Turn on' : 'Turn off'}
            type="button"
          >
            <span className="node-pedal-footer__indicator" aria-hidden="true" />
            <span>{bypassed ? 'OFF' : 'ON'}</span>
          </button>
        )}
      </div>
      {outputPorts.map((port, index) => (
        <Handle
          aria-label={`${data.instance.id} ${port} output`}
          className="project-node__handle project-node__handle--output"
          id={port}
          key={port}
          position={Position.Right}
          style={{
            top: portTop(
              data.routingLayout?.outputTops[port] ?? data.visualLayout.railTop,
              index,
              outputPorts.length,
            ),
          }}
          title={port}
          type="source"
        />
      ))}
    </div>
  );
});

ProjectNode.displayName = 'ProjectNode';
