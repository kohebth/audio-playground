import type { ProjectInspect, WorkspaceFile } from '../lib/backendSamples';

type Props = {
  project: ProjectInspect;
  sampleSources: Record<string, string>;
  workspaceFiles: WorkspaceFile[];
  selectedWorkspacePath: string;
  selectedNodeId: string | null;
  selectedRouteIndex: number | null;
  onSelectWorkspaceFile: (path: string) => void;
  onSelectNode: (id: string) => void;
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
  onSelectNode,
  onSelectRoute,
}: Props) {
  return (
    <aside className="project-sidebar">
      <section className="project-card">
        <span className="project-card__label">Loaded Project</span>
        <strong>{project.file}</strong>
        <span>{project.schema}</span>
      </section>

      <div className="project-sidebar__header">
        <span className="project-sidebar__title">Pedalboard</span>
        <span className="project-sidebar__count">{project.nodes.length} units</span>
      </div>

      <div className="project-list">
        {project.nodes.map((instance, index) => {
          const unit = project.units.find(item => item.id === instance.unit);
          const nodeId = `unit-${instance.id}`;

          return (
            <button
              key={instance.id}
              className={`project-list__item ${selectedNodeId === nodeId ? 'project-list__item--active' : ''}`}
              onClick={() => onSelectNode(nodeId)}
              type="button"
            >
              <span className="project-list__index">{index + 1}</span>
              <span className="project-list__main">
                <span className="project-list__name">{instance.id}</span>
                <span className="project-list__unit">{unit?.name ?? instance.unit}</span>
              </span>
              <span className="project-list__params">{instance.params.length}</span>
            </button>
          );
        })}
      </div>

      <div className="route-list">
        <div className="route-list__header">
          <span>Routes</span>
          <strong>{project.routes.length}</strong>
        </div>
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
      </div>

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
    </aside>
  );
}
