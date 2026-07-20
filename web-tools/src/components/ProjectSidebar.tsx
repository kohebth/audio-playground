import { useState, type FormEvent } from 'react';

import type { ProjectInspect, WorkspaceFile } from '../lib/backendSamples';
import { markPerfSpan } from '../lib/perfTelemetry';

type Props = {
  project: ProjectInspect;
  workspaceFiles: WorkspaceFile[];
  selectedWorkspacePath: string;
  selectedNodeId: string | null;
  selectedRouteIndex: number | null;
  onSelectWorkspaceFile: (path: string) => void;
  onCreateUnit: (name: string) => void;
  onAddInstance: (unitId: string, instanceId: string) => void;
  onAddUnitFromLibrary: (unitId: string) => void;
  onAddRoute: (route: { from: string; to: string }) => void;
  routeSources: string[];
  routeTargets: string[];
  onSelectNode: (id: string) => void;
  onOpenContractGraph: (id: string) => void;
  onSelectRoute: (index: number) => void;
  selectedInstanceIds: string[];
  onToggleBatchInstance: (instanceId: string) => void;
};

export const UNIT_DRAG_TYPE = 'application/x-apg-unit';

const unitDotColors = ['var(--accent-blue)', 'var(--accent-orange)', 'var(--accent-cyan)', 'var(--accent-purple)', 'var(--accent-green)', '#f472b6'];
type SidebarSection = 'workspace' | 'pedalboard' | 'routes' | 'drafts';

function friendlyFileName(file: WorkspaceFile): string {
  if (file.role === 'project') return 'Project settings';
  const name = file.path.split('/').at(-1)?.replace(/\.unit\.v2\.yaml$/i, '') ?? file.path;
  return name.replace(/_/g, ' ');
}

export function ProjectSidebar({
  project,
  workspaceFiles,
  selectedWorkspacePath,
  selectedNodeId,
  selectedRouteIndex,
  onSelectWorkspaceFile,
  onCreateUnit,
  onAddInstance,
  onAddUnitFromLibrary,
  onAddRoute,
  onSelectNode,
  onOpenContractGraph,
  onSelectRoute,
  routeSources,
  routeTargets,
  selectedInstanceIds,
  onToggleBatchInstance,
}: Props) {
  const [unitName, setUnitName] = useState('');
  const [createError, setCreateError] = useState<string | null>(null);
  const [instanceUnit, setInstanceUnit] = useState(project.units[0]?.id ?? '');
  const [instanceId, setInstanceId] = useState('');
  const [instanceError, setInstanceError] = useState<string | null>(null);
  const [routeSource, setRouteSource] = useState('system.input');
  const [routeTarget, setRouteTarget] = useState('system.output');
  const [routeError, setRouteError] = useState<string | null>(null);
  const [collapsedSections, setCollapsedSections] = useState<Record<SidebarSection, boolean>>({
    workspace: false,
    pedalboard: false,
    routes: false,
    drafts: false,
  });

  const toggleSection = (section: SidebarSection) => {
    setCollapsedSections(current => ({ ...current, [section]: !current[section] }));
  };

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
        <button
          aria-expanded={!collapsedSections.workspace}
          className={`sidebar-title sidebar-title--button ${collapsedSections.workspace ? 'closed' : 'open'}`}
          onClick={() => toggleSection('workspace')}
          type="button"
        >
          <span>Project files</span>
          <i className="fa-solid fa-chevron-down chevron" aria-hidden="true" />
        </button>
        <button className="sidebar-icon-btn" onClick={() => setUnitName('new_unit')} title="Create a unit" type="button">+</button>
      </div>

      <div className={`file-list ${collapsedSections.workspace ? 'hidden' : ''}`} aria-label="Workspace files">
        {workspaceFiles.map(file => (
          <button
            key={file.path}
            className={`file-item ${selectedWorkspacePath === file.path ? 'file-item--active' : ''}`}
            onClick={() => onSelectWorkspaceFile(file.path)}
            type="button"
          >
            <i className={`fa-solid ${file.role === 'project' ? 'fa-folder-open' : 'fa-file-code'}`} aria-hidden="true" />
            <span className="file-item__copy"><strong>{friendlyFileName(file)}</strong><small>{file.role === 'project' ? 'Project' : 'Unit'}</small></span>
          </button>
        ))}
      </div>

      <div className="sidebar-left__scroll">
      <section className="accordion">
      <button
        aria-expanded={!collapsedSections.pedalboard}
        className={`accordion-trigger ${collapsedSections.pedalboard ? 'closed' : 'open'}`}
        onClick={() => toggleSection('pedalboard')}
        type="button"
      >
        <span><i className="fa-solid fa-guitar" aria-hidden="true" />Pedalboard Units</span>
        <i className="fa-solid fa-chevron-down chevron" aria-hidden="true" />
      </button>
      <div className={`accordion-body ${collapsedSections.pedalboard ? 'hidden' : ''}`}>
      <div className="unit-library" aria-label="Unit library">
        {project.units.map(unit => (
          <button
            key={unit.id}
            className="unit-library__item"
            draggable
            data-testid={`project-unit-item-${unit.id}`}
            onClick={() => onAddUnitFromLibrary(unit.id)}
            onDragStart={event => {
              markPerfSpan('ui.dragStart.projectUnit', () => {
                event.dataTransfer.setData(UNIT_DRAG_TYPE, unit.id);
                event.dataTransfer.effectAllowed = 'copy';
              }, { unit: unit.id });
            }}
            type="button"
          >
            <span>{unit.name}</span>
            <strong>{unit.id}</strong>
          </button>
        ))}
      </div>
        <form className="project-instance-create add-unit-widget" onSubmit={submitInstance}>
        <select
          data-testid="project-instance-unit"
          aria-label="Unit type"
          onChange={event => setInstanceUnit(event.target.value)}
          value={instanceUnit}
        >
          {project.units.map(unit => <option key={unit.id} value={unit.id}>{unit.id}</option>)}
        </select>
        <input
          aria-label="New instance id"
          data-testid="project-instance-id"
          onChange={event => setInstanceId(event.target.value)}
          placeholder="id"
          spellCheck={false}
          value={instanceId}
        />
        <button
          data-testid="project-instance-add"
          disabled={!instanceUnit || !instanceId.trim()}
          title="Add unit"
          type="submit"
        >
          <i className="fa-solid fa-plus" aria-hidden="true" />
        </button>
      </form>
      {instanceError ? <p className="workspace-ledger__error project-instance-create__error">{instanceError}</p> : null}

      <div className="project-list">
        {project.nodes.map((instance, index) => {
          const nodeId = `unit-${instance.id}`;

          return (
            <div className="project-list__row" key={instance.id}>
              <button
                className={`project-list__item unit-item ${selectedNodeId === nodeId ? 'project-list__item--active' : ''}`}
                data-testid={`project-instance-item-${instance.id}`}
                onClick={() => onSelectNode(nodeId)}
                onDoubleClick={() => onOpenContractGraph(nodeId)}
                type="button"
              >
              <span className="project-list__main">
                <span className="project-list__index unit-dot" style={{ background: unitDotColors[index % unitDotColors.length] }} />
                <span className="project-list__name unit-name">{instance.id}</span>
                <span className="project-list__unit unit-type">({instance.unit})</span>
              </span>
              <span className="project-list__params unit-params">
                {instance.params.length} {instance.params.length === 1 ? 'param' : 'params'}
              </span>
              </button>
              <button
                aria-label={`Select ${instance.id} for batch editing`}
                aria-pressed={selectedInstanceIds.includes(instance.id)}
                className="project-list__batch"
                onClick={() => onToggleBatchInstance(instance.id)}
                title="Add to batch selection"
                type="button"
              >
                <i className={`fa-solid ${selectedInstanceIds.includes(instance.id) ? 'fa-check-square' : 'fa-square'}`} aria-hidden="true" />
              </button>
            </div>
          );
        })}
      </div>

      </div>
      </section>

      <section className="route-list accordion">
        <button
          aria-expanded={!collapsedSections.routes}
          className={`route-list__header accordion-trigger ${collapsedSections.routes ? 'closed' : 'open'}`}
          onClick={() => toggleSection('routes')}
          type="button"
        >
          <span><i className="fa-solid fa-circle-nodes" aria-hidden="true" />Connection Matrix</span>
          <span className="route-list__header-meta"><strong>{project.routes.length}</strong><i className="fa-solid fa-chevron-down chevron" aria-hidden="true" /></span>
        </button>
        <div className={`accordion-body route-list__body ${collapsedSections.routes ? 'hidden' : ''}`}>
        <form className="route-create add-unit-widget" onSubmit={submitRoute}>
          <select
            aria-label="New route source"
            data-testid="project-route-source"
            onChange={event => setRouteSource(event.target.value)}
            value={routeSource}
          >
            {routeSources.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
          </select>
          <select
            aria-label="New route target"
            data-testid="project-route-target"
            onChange={event => setRouteTarget(event.target.value)}
            value={routeTarget}
          >
            {routeTargets.map(endpoint => <option key={endpoint} value={endpoint}>{endpoint}</option>)}
          </select>
          <button data-testid="project-route-add" disabled={!routeSource || !routeTarget} type="submit">Connect</button>
        </form>
        {routeError ? <p className="workspace-ledger__error">{routeError}</p> : null}
        <div className="route-list__columns" aria-hidden="true">
          <span>Source</span>
          <span>Destination</span>
        </div>
        {project.routes.map((route, index) => (
          <button
            key={`${index}-${route.from}-${route.to}`}
            className={`route-list__item route-item ${selectedRouteIndex === index ? 'route-list__item--active' : ''}`}
            data-testid={`route-item-${index}`}
            onClick={() => onSelectRoute(index)}
            type="button"
          >
            <span>{route.from}</span>
            <i className="fa-solid fa-arrow-right route-arrow" aria-hidden="true" />
            <strong>{route.to}</strong>
          </button>
        ))}
        </div>
      </section>

      <section className="workspace-ledger accordion">
        <button
          aria-expanded={!collapsedSections.drafts}
          className={`sample-ledger__title accordion-trigger ${collapsedSections.drafts ? 'closed' : 'open'}`}
          onClick={() => toggleSection('drafts')}
          type="button"
        >
          <span>Unit Files</span>
          <i className="fa-solid fa-chevron-down chevron" aria-hidden="true" />
        </button>
        <div className={`accordion-body workspace-ledger__body ${collapsedSections.drafts ? 'hidden' : ''}`}>
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
      </section>
      </div>
    </aside>
  );
}
