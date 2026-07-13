import { useState, type FormEvent } from 'react';

import type { ProjectInspect, WorkspaceFile } from '../lib/backendSamples';

type Props = {
  project: ProjectInspect;
  sampleSources: Record<string, string>;
  workspaceFiles: WorkspaceFile[];
  selectedWorkspacePath: string;
  selectedNodeId: string | null;
  selectedRouteIndex: number | null;
  onSelectWorkspaceFile: (path: string) => void;
  onCreateUnit: (name: string) => void;
  onAddInstance: (unitId: string, instanceId: string) => void;
  onAddRoute: (route: { from: string; to: string }) => void;
  routeSources: string[];
  routeTargets: string[];
  onSelectNode: (id: string) => void;
  onOpenContractGraph: (id: string) => void;
  onSelectRoute: (index: number) => void;
};

export function ProjectSidebar({
  project,
  sampleSources,
  workspaceFiles,
  selectedWorkspacePath,
  selectedNodeId,
  selectedRouteIndex,
  onSelectWorkspaceFile,
  onCreateUnit,
  onAddInstance,
  onAddRoute,
  onSelectNode,
  onOpenContractGraph,
  onSelectRoute,
  routeSources,
  routeTargets,
}: Props) {
  const [unitName, setUnitName] = useState('');
  const [createError, setCreateError] = useState<string | null>(null);
  const [instanceUnit, setInstanceUnit] = useState(project.units[0]?.id ?? '');
  const [instanceId, setInstanceId] = useState('');
  const [instanceError, setInstanceError] = useState<string | null>(null);
  const [routeSource, setRouteSource] = useState('system.input');
  const [routeTarget, setRouteTarget] = useState('system.output');
  const [routeError, setRouteError] = useState<string | null>(null);

  const submitUnit = (event: FormEvent) => {
    event.preventDefault();
    try {
      onCreateUnit(unitName);
      setUnitName('');
      setCreateError(null);
    } catch (error) {
      setCreateError(error instanceof Error ? error.message : String(error));
    }
  };

  const submitRoute = (event: FormEvent) => {
    event.preventDefault();
    try {
      onAddRoute({ from: routeSource, to: routeTarget });
      setRouteError(null);
    } catch (error) {
      setRouteError(error instanceof Error ? error.message : String(error));
    }
  };

  const submitInstance = (event: FormEvent) => {
    event.preventDefault();
    try {
      onAddInstance(instanceUnit, instanceId);
      setInstanceId('');
      setInstanceError(null);
    } catch (error) {
      setInstanceError(error instanceof Error ? error.message : String(error));
    }
  };

  return (
    <aside className="project-sidebar sidebar-left">
      <div className="sidebar-header">
        <span className="sidebar-title">Workspace Ledger</span>
        <button className="sidebar-icon-btn" onClick={() => setUnitName('new_unit')} title="Create draft below" type="button">+</button>
      </div>

      <div className="file-list" aria-label="Workspace files">
        {workspaceFiles.map(file => (
          <button
            key={file.path}
            className={`file-item ${selectedWorkspacePath === file.path ? 'file-item--active' : ''}`}
            onClick={() => onSelectWorkspaceFile(file.path)}
            type="button"
          >
            <span>{file.role === 'project' ? 'P' : 'U'}</span>
            <code>{file.path}</code>
          </button>
        ))}
      </div>

      <div className="sidebar-left__scroll">
      <section className="accordion">
      <div className="accordion-trigger">
        <span>Pedalboard Units</span>
        <span>{project.nodes.length}</span>
      </div>
      <div className="accordion-body">
      <form className="project-instance-create add-unit-widget" onSubmit={submitInstance}>
        <select aria-label="Unit type" onChange={event => setInstanceUnit(event.target.value)} value={instanceUnit}>
          {project.units.map(unit => <option key={unit.id} value={unit.id}>{unit.name}</option>)}
        </select>
        <input
          aria-label="New instance id"
          onChange={event => setInstanceId(event.target.value)}
          placeholder="instance_id"
          spellCheck={false}
          value={instanceId}
        />
        <button disabled={!instanceUnit || !instanceId.trim()} type="submit">Add</button>
      </form>
      {instanceError ? <p className="workspace-ledger__error project-instance-create__error">{instanceError}</p> : null}

      <div className="project-list">
        {project.nodes.map((instance, index) => {
          const unit = project.units.find(item => item.id === instance.unit);
          const nodeId = `unit-${instance.id}`;

          return (
            <button
              key={instance.id}
              className={`project-list__item unit-item ${selectedNodeId === nodeId ? 'project-list__item--active' : ''}`}
              onClick={() => onSelectNode(nodeId)}
              onDoubleClick={() => onOpenContractGraph(nodeId)}
              type="button"
            >
              <span className="project-list__index unit-dot" style={{ background: unit?.compatibility.wasm_realtime ? '#34d399' : '#4a9eff' }}>{index + 1}</span>
              <span className="project-list__main">
                <span className="project-list__name">{instance.id}</span>
                <span className="project-list__unit">{unit?.name ?? instance.unit}</span>
              </span>
              <span className="project-list__params">{instance.params.length}</span>
            </button>
          );
        })}
      </div>

      </div>
      </section>

      <section className="route-list accordion">
        <div className="route-list__header">
          <span>Routes</span>
          <strong>{project.routes.length}</strong>
        </div>
        <form className="route-create add-unit-widget" onSubmit={submitRoute}>
          <select aria-label="New route source" onChange={event => setRouteSource(event.target.value)} value={routeSource}>
            {routeSources.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
          </select>
          <select aria-label="New route target" onChange={event => setRouteTarget(event.target.value)} value={routeTarget}>
            {routeTargets.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
          </select>
          <button disabled={!routeSource || !routeTarget} type="submit">Connect</button>
        </form>
        {routeError ? <p className="workspace-ledger__error">{routeError}</p> : null}
        {project.routes.map((route, index) => (
          <button
            key={`${index}-${route.from}-${route.to}`}
            className={`route-list__item ${selectedRouteIndex === index ? 'route-list__item--active' : ''}`}
            onClick={() => onSelectRoute(index)}
            type="button"
          >
            <span>{route.from}</span>
            <strong>{route.to}</strong>
          </button>
        ))}
      </section>

      <div className="sample-ledger">
        <div className="sample-ledger__title">Frozen Sources</div>
        {Object.entries(sampleSources).map(([key, path]) => (
          <div key={key} className="sample-ledger__row">
            <span>{key}</span>
            <code>{path}</code>
          </div>
        ))}
      </div>

      <div className="workspace-ledger">
        <div className="sample-ledger__title">Workspace Drafts</div>
        <form className="workspace-ledger__create" onSubmit={submitUnit}>
          <input
            aria-label="New unit name"
            onChange={event => setUnitName(event.target.value)}
            placeholder="unit_name"
            spellCheck={false}
            value={unitName}
          />
          <button disabled={unitName.trim() === ''} type="submit">Create</button>
        </form>
        {createError ? <p className="workspace-ledger__error">{createError}</p> : null}
        {workspaceFiles.map(file => (
          <button
            key={file.path}
            className={`workspace-ledger__row ${selectedWorkspacePath === file.path ? 'workspace-ledger__row--active' : ''}`}
            onClick={() => onSelectWorkspaceFile(file.path)}
            type="button"
          >
            <span>{file.role}</span>
            <code>{file.path}</code>
            {file.content !== file.originalContent ? <strong>edited</strong> : null}
          </button>
        ))}
      </div>
      </div>
    </aside>
  );
}
