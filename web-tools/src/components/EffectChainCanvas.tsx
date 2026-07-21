import { useCallback, useMemo, useState, type CSSProperties, type DragEvent, type KeyboardEvent as ReactKeyboardEvent } from 'react';
import type { Node } from '@xyflow/react';

import type {
  EffectChainItemDraft,
  EffectChainParallelDraft,
  EffectChainRailDraft,
} from '../lib/projectPackage';
import type { ProjectInspect } from '../lib/backendSamples';
import type { EffectChainLocation } from '../lib/effectChainDraft';
import type { ProjectNodeData } from '../lib/projectGraph';
import type { ProjectPortCatalog } from '../lib/projectV2Graph';
import { EFFECT_LIBRARY_DRAG_TYPE, type EffectLibraryDragPayload } from './SimpleLibraryPanel';
import { GraphContextMenu, GraphMenuButton, type ContextMenuPoint } from './GraphContextMenu';

const EFFECT_INSTANCE_DRAG_TYPE = 'application/x-apg-effect-chain-instance';

type Props = {
  draft: { version: 1; root: EffectChainRailDraft };
  project: ProjectInspect;
  ports: ProjectPortCatalog;
  nodes: Node<ProjectNodeData>[];
  selectedNodeId: string | null;
  canPaste: boolean;
  onSelectInstance: (nodeId: string) => void;
  onDropLibrary: (payload: EffectLibraryDragPayload, location: EffectChainLocation) => void;
  onAddParallel: (payload: EffectLibraryDragPayload, location: EffectChainLocation) => void;
  onMoveEffect: (instanceId: string, location: EffectChainLocation) => void;
  onCopy: (instanceId: string) => void;
  onCut: (instanceId: string) => void;
  onPaste: (location: EffectChainLocation) => void;
  onRemoveEffect: (instanceId: string) => void;
  onReplace: (instanceId: string, unitId: string) => void;
  onSetEndpoint: (sectionId: string, role: 'panner' | 'mixer', connected: boolean) => void;
  onParamChange: (instanceId: string, param: string, value: string) => void;
  replacementOptions: Array<{ id: string; label: string; routing?: { role: 'panner' | 'mixer'; paths: Array<{ port: string; levelParam: string }> } }>;
  parallelOptions: EffectLibraryDragPayload[];
};

type MenuState = ContextMenuPoint & {
  instanceId: string;
  location?: EffectChainLocation;
  sectionId?: string;
  role?: 'panner' | 'mixer';
};

function DropSlot({ location, onAddParallel, onDropLibrary, onMoveEffect, parallelOptions }: Pick<Props, 'onAddParallel' | 'onDropLibrary' | 'onMoveEffect' | 'parallelOptions'> & {
  location: EffectChainLocation;
}) {
  const [active, setActive] = useState(false);
  const [parallelOpen, setParallelOpen] = useState(false);
  const optionKey = (option: EffectLibraryDragPayload) => `${option.scope}:${option.recordId ?? option.id}`;
  const [parallelId, setParallelId] = useState(parallelOptions[0] ? optionKey(parallelOptions[0]) : '');
  const parallel = parallelOptions.find(option => optionKey(option) === parallelId) ?? parallelOptions[0];
  const accepts = (event: DragEvent) => (
    event.dataTransfer.types.includes(EFFECT_LIBRARY_DRAG_TYPE)
    || event.dataTransfer.types.includes(EFFECT_INSTANCE_DRAG_TYPE)
  );
  return (
    <div
      aria-label={`Drop effect at ${location.railId} position ${location.index + 1}`}
      className={`effect-chain-drop${active ? ' effect-chain-drop--active' : ''}`}
      data-rail-id={location.railId}
      data-rail-index={location.index}
      onDragEnter={event => {
        if (!accepts(event)) return;
        event.preventDefault();
        setActive(true);
      }}
      onDragLeave={() => setActive(false)}
      onDragOver={event => {
        if (!accepts(event)) return;
        event.preventDefault();
        event.dataTransfer.dropEffect = 'move';
      }}
      onDrop={event => {
        event.preventDefault();
        setActive(false);
        const instanceId = event.dataTransfer.getData(EFFECT_INSTANCE_DRAG_TYPE);
        if (instanceId) {
          onMoveEffect(instanceId, location);
          return;
        }
        const raw = event.dataTransfer.getData(EFFECT_LIBRARY_DRAG_TYPE);
        if (!raw) return;
        try {
          onDropLibrary(JSON.parse(raw) as EffectLibraryDragPayload, location);
        } catch {
          // Ignore malformed external drag data.
        }
      }}
      role="button"
      tabIndex={0}
    >
      <span>+</span>
      <button
        aria-label={`Add parallel section at ${location.railId} position ${location.index + 1}`}
        onClick={() => setParallelOpen(open => !open)}
        title="Add a nested parallel section"
        type="button"
      >∥</button>
      {parallelOpen && parallel ? (
        <div className="effect-chain-drop__parallel" onClick={event => event.stopPropagation()}>
          <label><span>Parallel effect</span><select onChange={event => setParallelId(event.target.value)} value={parallel.id}>
            {parallelOptions.map(option => <option key={optionKey(option)} value={optionKey(option)}>{option.title}</option>)}
          </select></label>
          <button onClick={() => { onAddParallel(parallel, location); setParallelOpen(false); }} type="button">Add section</button>
        </div>
      ) : null}
    </div>
  );
}

function LevelControls({ instanceId, project, ports, onParamChange }: Pick<Props, 'project' | 'ports' | 'onParamChange'> & {
  instanceId: string;
}) {
  const instance = project.nodes.find(node => node.id === instanceId);
  const contract = instance ? ports[instance.unit] : null;
  if (!instance || !contract?.routing) return null;
  const params = new Map(instance.params.map(param => [param.key, param.value]));
  return (
    <div className="effect-chain-helper__levels">
      {contract.routing.paths.map((path, index) => {
        const raw = params.get(path.levelParam) ?? '0';
        const value = Number(raw);
        return (
          <label key={path.port}>
            <span>{index + 1}</span>
            <input
              aria-label={`${instanceId} ${path.port} volume`}
              max="6"
              min="-60"
              onChange={event => onParamChange(instanceId, path.levelParam, event.target.value)}
              step="0.5"
              type="range"
              value={Number.isFinite(value) ? value : 0}
            />
            <output>{Number.isFinite(value) ? value.toFixed(1) : '0.0'} dB</output>
          </label>
        );
      })}
    </div>
  );
}

function EffectCard({
  item,
  index,
  railId,
  project,
  nodes,
  selectedNodeId,
  onSelectInstance,
  onMenu,
}: {
  item: Extract<EffectChainItemDraft, { kind: 'effect' }>;
  index: number;
  railId: string;
  project: ProjectInspect;
  nodes: Node<ProjectNodeData>[];
  selectedNodeId: string | null;
  onSelectInstance: Props['onSelectInstance'];
  onMenu: (state: MenuState) => void;
}) {
  const instance = project.nodes.find(node => node.id === item.instanceId);
  const unit = instance ? project.units.find(reference => reference.id === instance.unit) : null;
  const flowNode = nodes.find(node => node.id === `unit-${item.instanceId}`)?.data;
  const selected = selectedNodeId === `unit-${item.instanceId}`;
  return (
    <article
      aria-label={`${item.instanceId} effect`}
      className={`effect-chain-card${selected ? ' effect-chain-card--selected' : ''}`}
      draggable
      onClick={() => onSelectInstance(`unit-${item.instanceId}`)}
      onContextMenu={event => {
        event.preventDefault();
        onSelectInstance(`unit-${item.instanceId}`);
        onMenu({ x: event.clientX, y: event.clientY, instanceId: item.instanceId, location: { railId, index: index + 1 } });
      }}
      onDragStart={event => {
        event.dataTransfer.setData(EFFECT_INSTANCE_DRAG_TYPE, item.instanceId);
        event.dataTransfer.effectAllowed = 'move';
      }}
      onKeyDown={(event: ReactKeyboardEvent) => {
        if (event.key !== 'ContextMenu' && !(event.shiftKey && event.key === 'F10')) return;
        event.preventDefault();
        const rect = event.currentTarget.getBoundingClientRect();
        onMenu({ x: rect.left + 24, y: rect.top + 24, instanceId: item.instanceId, location: { railId, index: index + 1 } });
      }}
      tabIndex={0}
    >
      <i style={{ '--effect-color': flowNode?.kind === 'unit' ? flowNode.color : '#82c995' } as CSSProperties} />
      <strong>{unit?.name.replace(/_unit$/, '').replace(/_/g, ' ') ?? item.instanceId}</strong>
      <small>{item.instanceId}</small>
      {flowNode?.kind === 'unit' && flowNode.bypassed ? <b>Off</b> : null}
    </article>
  );
}

function ParallelSection({
  item,
  project,
  ports,
  nodes,
  selectedNodeId,
  canPaste,
  onSelectInstance,
  onAddParallel,
  onDropLibrary,
  onMoveEffect,
  onMenu,
  onSetEndpoint,
  onParamChange,
  parallelOptions,
}: Pick<Props,
  'project' | 'ports' | 'nodes' | 'selectedNodeId' | 'canPaste' | 'onSelectInstance' | 'onDropLibrary'
  | 'onAddParallel' | 'onMoveEffect' | 'onSetEndpoint' | 'onParamChange' | 'parallelOptions'> & {
  item: EffectChainParallelDraft;
  onMenu: (state: MenuState) => void;
}) {
  const helper = (role: 'panner' | 'mixer') => {
    const instanceId = role === 'panner' ? item.pannerInstanceId : item.mixerInstanceId;
    const storedId = role === 'panner' ? item.storedPannerInstanceId : item.storedMixerInstanceId;
    if (!instanceId) {
      return (
        <button
          className="effect-chain-helper effect-chain-helper--missing"
          onClick={() => onSetEndpoint(item.id, role, true)}
          type="button"
        >
          <i className="fa-solid fa-plus" aria-hidden="true" />
          <strong>Restore {role === 'panner' ? 'panner' : 'mixer'}</strong>
          <small>Drop a compatible helper here</small>
        </button>
      );
    }
    return (
      <article
        className={`effect-chain-helper${selectedNodeId === `unit-${instanceId}` ? ' effect-chain-helper--selected' : ''}`}
        onClick={() => onSelectInstance(`unit-${instanceId}`)}
        onContextMenu={event => {
          event.preventDefault();
          onSelectInstance(`unit-${instanceId}`);
          onMenu({ x: event.clientX, y: event.clientY, instanceId, sectionId: item.id, role });
        }}
        onKeyDown={(event: ReactKeyboardEvent) => {
          if (event.key !== 'ContextMenu' && !(event.shiftKey && event.key === 'F10')) return;
          event.preventDefault();
          const rect = event.currentTarget.getBoundingClientRect();
          onMenu({ x: rect.left + 24, y: rect.top + 24, instanceId, sectionId: item.id, role });
        }}
        tabIndex={0}
      >
        <button
          aria-label={`Open ${role} actions`}
          className="effect-chain-helper__menu"
          onClick={event => {
            event.stopPropagation();
            const rect = event.currentTarget.getBoundingClientRect();
            onMenu({ x: rect.left, y: rect.bottom + 4, instanceId, sectionId: item.id, role });
          }}
          type="button"
        >•••</button>
        <span>{role === 'panner' ? 'Split' : 'Join'}</span>
        <strong>{storedId}</strong>
        <LevelControls instanceId={instanceId} onParamChange={onParamChange} ports={ports} project={project} />
      </article>
    );
  };

  return (
    <section className="effect-chain-parallel" data-section-id={item.id}>
      <header><span>Parallel section</span><strong>{item.section}</strong></header>
      <div className="effect-chain-parallel__body">
        {helper('panner')}
        <div className="effect-chain-branches">
          {item.paths.map((path, index) => (
            <div className="effect-chain-branch" key={path.id}>
              <span className="effect-chain-branch__label">Path {index + 1}</span>
              <Rail
                canPaste={canPaste}
                nodes={nodes}
                onAddParallel={onAddParallel}
                onDropLibrary={onDropLibrary}
                onMenu={onMenu}
                onMoveEffect={onMoveEffect}
                onParamChange={onParamChange}
                onSelectInstance={onSelectInstance}
                onSetEndpoint={onSetEndpoint}
                ports={ports}
                parallelOptions={parallelOptions}
                project={project}
                rail={path.rail}
                selectedNodeId={selectedNodeId}
              />
            </div>
          ))}
        </div>
        {helper('mixer')}
      </div>
    </section>
  );
}

function Rail({
  rail,
  ...props
}: Pick<Props, 'project' | 'ports' | 'nodes' | 'selectedNodeId' | 'canPaste' | 'onSelectInstance'
  | 'onAddParallel' | 'onDropLibrary' | 'onMoveEffect' | 'onSetEndpoint' | 'onParamChange' | 'parallelOptions'> & {
  rail: EffectChainRailDraft;
  onMenu: (state: MenuState) => void;
}) {
  return (
    <div className="effect-chain-rail" data-rail-id={rail.id}>
      <DropSlot location={{ railId: rail.id, index: 0 }} onAddParallel={props.onAddParallel} onDropLibrary={props.onDropLibrary} onMoveEffect={props.onMoveEffect} parallelOptions={props.parallelOptions} />
      {rail.items.map((item, index) => (
        <div className="effect-chain-rail__item" key={item.kind === 'effect' ? item.instanceId : item.id}>
          {item.kind === 'effect' ? (
            <EffectCard
              index={index}
              item={item}
              nodes={props.nodes}
              onMenu={props.onMenu}
              onSelectInstance={props.onSelectInstance}
              project={props.project}
              railId={rail.id}
              selectedNodeId={props.selectedNodeId}
            />
          ) : (
            <ParallelSection item={item} {...props} />
          )}
          <DropSlot location={{ railId: rail.id, index: index + 1 }} onAddParallel={props.onAddParallel} onDropLibrary={props.onDropLibrary} onMoveEffect={props.onMoveEffect} parallelOptions={props.parallelOptions} />
        </div>
      ))}
    </div>
  );
}

export function EffectChainCanvas(props: Props) {
  const [menu, setMenu] = useState<MenuState | null>(null);
  const [replaceOpen, setReplaceOpen] = useState(false);
  const [replacement, setReplacement] = useState('');
  const flowNode = menu ? props.nodes.find(item => item.id === `unit-${menu.instanceId}`)?.data : null;
  const instance = menu ? props.project.nodes.find(item => item.id === menu.instanceId) : null;
  const unit = instance ? props.project.units.find(item => item.id === instance.unit) : null;
  const currentRouting = instance ? props.ports[instance.unit]?.routing : undefined;
  const options = useMemo(() => props.replacementOptions.filter(option => {
    if (!instance || option.id === instance.unit) return false;
    if (!currentRouting) return !option.routing;
    return option.routing?.role === currentRouting.role
      && option.routing.paths.length === currentRouting.paths.length;
  }), [currentRouting, instance, props.replacementOptions]);
  const chosen = options.find(option => option.id === replacement) ?? options[0];
  const closeMenu = useCallback(() => {
    setMenu(null);
    setReplaceOpen(false);
  }, []);

  return (
    <main className="effect-chain-canvas canvas-area" data-testid="effect-chain-canvas">
      <div className="effect-chain-canvas__header">
        <div><span>Effect Chain</span><strong>Drag effects anywhere along a rail</strong></div>
        <small>No manual wiring</small>
      </div>
      <div className="effect-chain-scroll">
        <div className="effect-chain-system effect-chain-system--input"><strong>Input</strong><small>system.input</small></div>
        <Rail {...props} onMenu={setMenu} rail={props.draft.root} />
        <div className="effect-chain-system effect-chain-system--output"><strong>Output</strong><small>system.output</small></div>
      </div>
      {menu && instance ? (
        <GraphContextMenu label={`${menu.instanceId} actions`} onClose={closeMenu} point={menu}>
          <div className="graph-context-menu__title"><strong>{menu.instanceId}</strong><span>{unit?.name ?? instance.unit}</span></div>
          <GraphMenuButton
            disabled={flowNode?.kind !== 'unit' || !flowNode.bypassAvailable || Boolean(menu.role)}
            icon="fa-power-off"
            onClick={() => {
              if (flowNode?.kind === 'unit') void flowNode.onBypassChange?.(menu.instanceId, !flowNode.bypassed);
              closeMenu();
            }}
          >{flowNode?.kind === 'unit' && flowNode.bypassed ? 'Turn on' : 'Turn off'}</GraphMenuButton>
          <GraphMenuButton
            disabled={options.length === 0}
            icon="fa-repeat"
            onClick={() => { setReplaceOpen(open => !open); setReplacement(chosen?.id ?? ''); }}
          >Replace…</GraphMenuButton>
          {replaceOpen && chosen ? (
            <div className="graph-context-menu__replace">
              <label><span>Replace with</span><select onChange={event => setReplacement(event.target.value)} value={chosen.id}>
                {options.map(option => <option key={option.id} value={option.id}>{option.label}</option>)}
              </select></label>
              <button className="graph-context-menu__confirm" onClick={() => { props.onReplace(menu.instanceId, chosen.id); closeMenu(); }} type="button">Confirm replace</button>
            </div>
          ) : null}
          <div className="graph-context-menu__separator" role="separator" />
          {!menu.role ? <GraphMenuButton icon="fa-scissors" onClick={() => { props.onCut(menu.instanceId); closeMenu(); }}>Cut</GraphMenuButton> : null}
          {!menu.role ? <GraphMenuButton icon="fa-copy" onClick={() => { props.onCopy(menu.instanceId); closeMenu(); }}>Copy</GraphMenuButton> : null}
          {!menu.role ? <GraphMenuButton disabled={!props.canPaste || !menu.location} icon="fa-paste" onClick={() => { if (menu.location) props.onPaste(menu.location); closeMenu(); }}>Paste</GraphMenuButton> : null}
          {menu.role && menu.sectionId ? (
            <GraphMenuButton danger icon="fa-trash" onClick={() => { props.onSetEndpoint(menu.sectionId!, menu.role!, false); closeMenu(); }}>Remove {menu.role}</GraphMenuButton>
          ) : (
            <GraphMenuButton danger icon="fa-trash" onClick={() => { props.onRemoveEffect(menu.instanceId); closeMenu(); }}>Remove</GraphMenuButton>
          )}
        </GraphContextMenu>
      ) : null}
    </main>
  );
}
