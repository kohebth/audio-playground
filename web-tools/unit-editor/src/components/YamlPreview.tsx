import { useState, useEffect, useRef } from 'react';
import { parseYaml } from '../lib/yamlParser';

type Props = { 
  yaml: string;
  onChange: (yaml: string) => void;
};

export function YamlPreview({ yaml, onChange }: Props) {
  const [localYaml, setLocalYaml] = useState(yaml);
  const [error, setError] = useState<string | null>(null);
  const isInternalChange = useRef(false);

  // Sync local state when external yaml changes (e.g. from graph edits)
  // But only if the change didn't come from this editor
  useEffect(() => {
    if (!isInternalChange.current) {
      setLocalYaml(yaml);
      setError(null);
    }
    isInternalChange.current = false;
  }, [yaml]);

  const handleChange = (e: React.ChangeEvent<HTMLTextAreaElement>) => {
    const next = e.target.value;
    setLocalYaml(next);
    
    try {
      // Validate YAML before passing it up
      parseYaml(next);
      setError(null);
      isInternalChange.current = true;
      onChange(next);
    } catch (err) {
      setError((err as Error).message);
    }
  };

  const copy = () => navigator.clipboard.writeText(localYaml);

  return (
    <div className="yaml-preview">
      <div className="yaml-preview__bar">
        <div className="yaml-preview__title-group">
          <span className="yaml-preview__title">YAML Editor</span>
          {error ? (
            <span className="yaml-preview__error">⚠️ {error}</span>
          ) : (
            <span className="yaml-preview__status">✓ Valid</span>
          )}
        </div>
        <div className="yaml-preview__actions">
          <button className="yaml-preview__copy" onClick={copy}>Copy</button>
        </div>
      </div>
      <textarea
        className={`yaml-preview__editor ${error ? 'yaml-preview__editor--error' : ''}`}
        value={localYaml}
        onChange={handleChange}
        spellCheck={false}
      />
    </div>
  );
}
