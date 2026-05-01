import { memo } from 'react';
import { Handle, Position, type NodeProps } from '@xyflow/react';
import type { StageNodeData } from '../lib/graphBuilder';
import { ATOM_MAP, CATEGORY_COLORS } from '../atoms/atomCatalog';

type AtomNodeData = StageNodeData & { color: string };

export const AtomNode = memo(({ data, selected }: NodeProps) => {
  const { stage } = data as AtomNodeData;
  const atomDef = ATOM_MAP.get(stage.fn);
  const category = atomDef?.category ?? 'filter';
  const accentColor = CATEGORY_COLORS[category] ?? '#6b7280';

  const ins  = stage.in.length  > 0 ? stage.in  : (atomDef?.ins.map(name => ({ key: name, value: '' })) ?? []);
  const outs = stage.out.length > 0 ? stage.out : (atomDef?.outs.map(name => ({ key: name, value: '' })) ?? []);

  return (
    <div className={`atom-node ${selected ? 'atom-node--selected' : ''}`} style={{ '--accent': accentColor } as React.CSSProperties}>
      {/* Node header */}
      <div className="atom-node__header" style={{ background: accentColor }}>
        <span className="atom-node__fn">{stage.fn}</span>
        <span className="atom-node__category">{category}</span>
      </div>

      {/* Node body */}
      <div className="atom-node__body">
        <div className="atom-node__id">{stage.id}</div>
        {stage.config.length > 0 && (
          <div className="atom-node__config">
            {stage.config.map(kv => (
              <div key={kv.key} className="atom-node__kv">
                <span className="kv-key">{kv.key}</span>
                <span className="kv-val">{kv.value}</span>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Input handles (at top) */}
      {ins.map((kv, i) => (
        <Handle
          key={`in-${kv.key}`}
          type="target"
          position={Position.Top}
          id={`in-${kv.key}`}
          style={{ left: `${(i + 1) * 100 / (ins.length + 1)}%` }}
          className="atom-handle atom-handle--in"
        >
          <span className="handle-label handle-label--in">{kv.key}</span>
        </Handle>
      ))}

      {/* Output handles (at bottom) */}
      {outs.map((kv, i) => (
        <Handle
          key={`out-${kv.key}`}
          type="source"
          position={Position.Bottom}
          id={`out-${kv.key}`}
          style={{ left: `${(i + 1) * 100 / (outs.length + 1)}%` }}
          className="atom-handle atom-handle--out"
        >
          <span className="handle-label handle-label--out">{kv.value || kv.key}</span>
        </Handle>
      ))}
    </div>
  );
});

AtomNode.displayName = 'AtomNode';
