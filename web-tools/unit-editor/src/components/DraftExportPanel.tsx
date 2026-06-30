import type { ParamOverride } from '../lib/projectParams';

type Props = {
  projectFile: string;
  overrides: ParamOverride[];
};

export function DraftExportPanel({ projectFile, overrides }: Props) {
  const payload = {
    schema: 'apg.ui.param_overrides.v1',
    project: projectFile,
    params: overrides.map(({ path, value }) => ({ path, value })),
  };

  return (
    <section className="inspector-block">
      <div className="inspector-block__label">Draft Export</div>
      <div className="draft-export__summary">
        <strong>{overrides.length} parameter overrides</strong>
        <span>{projectFile}</span>
      </div>

      {overrides.length > 0 ? (
        <div className="draft-export__rows">
          {overrides.map(override => (
            <div key={override.path} className="draft-export__row">
              <span>{override.path}</span>
              <strong>
                {override.originalValue}
                {' -> '}
                {override.value}
              </strong>
            </div>
          ))}
        </div>
      ) : (
        <div className="diagnostic-empty">No local parameter edits to export.</div>
      )}

      <pre className="draft-export__preview">{JSON.stringify(payload, null, 2)}</pre>
    </section>
  );
}
