type Props = { yaml: string };

export function YamlPreview({ yaml }: Props) {
  const copy = () => navigator.clipboard.writeText(yaml);
  return (
    <div className="yaml-preview">
      <div className="yaml-preview__bar">
        <span className="yaml-preview__title">YAML Preview</span>
        <button className="yaml-preview__copy" onClick={copy}>Copy</button>
      </div>
      <pre className="yaml-preview__code">{yaml}</pre>
    </div>
  );
}
