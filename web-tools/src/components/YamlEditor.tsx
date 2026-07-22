import { useEffect, useRef, useState } from 'react';
import { EditorState } from '@codemirror/state';
import { EditorView, lineNumbers, highlightActiveLine, highlightActiveLineGutter, keymap } from '@codemirror/view';
import { defaultKeymap, history, historyKeymap } from '@codemirror/commands';
import { searchKeymap, highlightSelectionMatches } from '@codemirror/search';
import { parseYaml } from '../lib/yamlParser';

type Props = {
  yaml: string;
  onChange: (yaml: string) => void;
};

const theme = EditorView.theme({
  '&': { height: '100%', fontSize: '13px' },
  '.cm-content': { fontFamily: 'var(--font-mono)', padding: '8px 0', caretColor: 'var(--accent)' },
  '.cm-line': { padding: '0 8px' },
  '.cm-gutters': { backgroundColor: 'var(--bg-elevated)', borderRight: '1px solid var(--border)', color: 'var(--text-muted)' },
  '.cm-activeLineGutter': { backgroundColor: 'var(--bg-hover)' },
  '.cm-activeLine': { backgroundColor: 'color-mix(in srgb, var(--bg-elevated) 65%, transparent)' },
  '.cm-selectionMatch': { backgroundColor: 'color-mix(in srgb, var(--accent) 24%, transparent)' },
  '.cm-cursor': { borderLeftColor: 'var(--accent)', borderLeftWidth: '2px' },
  '&.cm-focused .cm-cursor': { borderLeftColor: 'var(--accent)', borderLeftWidth: '2px' },
  '&.cm-focused .cm-selectionBackground, .cm-selectionBackground': { backgroundColor: '#3b82f640' },
  '.cm-searchMatch': { backgroundColor: 'color-mix(in srgb, var(--accent) 32%, transparent)', outline: '1px solid var(--accent)' },
  '.cm-searchMatch.cm-searchMatch-selected': { backgroundColor: 'color-mix(in srgb, var(--accent) 56%, transparent)' },
  '.cm-scroller': { overflow: 'auto' },
});

export function YamlEditor({ yaml, onChange }: Props) {
  const containerRef = useRef<HTMLDivElement>(null);
  const viewRef = useRef<EditorView | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const isInternalChange = useRef(false);
  const initialYaml = useRef(yaml);
  const onChangeRef = useRef(onChange);

  useEffect(() => {
    onChangeRef.current = onChange;
  }, [onChange]);

  useEffect(() => {
    if (!containerRef.current) return;

    const startState = EditorState.create({
      doc: initialYaml.current,
      extensions: [
        lineNumbers(),
        highlightActiveLine(),
        highlightActiveLineGutter(),
        highlightSelectionMatches(),
        history(),
        keymap.of([...defaultKeymap, ...historyKeymap, ...searchKeymap]),
        theme,
        EditorView.updateListener.of((update) => {
          if (update.docChanged) {
            const doc = update.state.doc.toString();
            try {
              parseYaml(doc);
              setError(null);
            } catch (err) {
              setError((err as Error).message);
            }
            if (!isInternalChange.current) {
              onChangeRef.current(doc);
            }
            isInternalChange.current = false;
          }
        }),
      ],
    });

    const view = new EditorView({
      state: startState,
      parent: containerRef.current,
    });

    viewRef.current = view;

    return () => {
      view.destroy();
    };
  }, []);

  useEffect(() => {
    if (!viewRef.current) return;
    const currentDoc = viewRef.current.state.doc.toString();
    if (currentDoc !== yaml) {
      isInternalChange.current = true;
      viewRef.current.dispatch({
        changes: { from: 0, to: currentDoc.length, insert: yaml },
      });
      setError(null);
    }
  }, [yaml]);

  useEffect(() => {
    if (!viewRef.current || !searchQuery) return;
    const doc = viewRef.current.state.doc;
    const query = new RegExp(searchQuery, 'gi');
    let match: { from: number; to: number } | null = null;
    for (let i = 0; i < doc.length; i++) {
      const text = doc.sliceString(i, Math.min(i + 100, doc.length));
      const m = text.match(query);
      if (m) {
        match = { from: i, to: i + m[0].length };
        break;
      }
    }
    if (match) {
      viewRef.current.dispatch({
        selection: { anchor: match.from, head: match.to },
        scrollIntoView: true,
      });
    }
  }, [searchQuery]);

  const handleCopy = () => {
    if (viewRef.current) {
      navigator.clipboard.writeText(viewRef.current.state.doc.toString());
    }
  };

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
          <input
            type="text"
            className="yaml-preview__search"
            placeholder="Search (Ctrl+F)"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
          />
          <button className="yaml-preview__copy" onClick={handleCopy}>
            Copy
          </button>
        </div>
      </div>
      <div ref={containerRef} className="yaml-preview__editor" />
    </div>
  );
}
