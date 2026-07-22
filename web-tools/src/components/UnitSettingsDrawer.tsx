import { useEffect, useRef } from 'react';

import type { WorkspaceFile } from '../lib/backendSamples';
import { parseUnitPortsDraft, type UnitGraphDraft } from '../lib/unitV2Graph';
import { StructuredUnitEditor } from './StructuredUnitEditor';

type Props = {
  file: WorkspaceFile;
  open: boolean;
  unit: UnitGraphDraft;
  onChange: (content: string) => void;
  onClose: () => void;
  onReorderParam: (name: string, nextIndex: number) => void;
};

export function UnitSettingsDrawer({ file, open, unit, onChange, onClose, onReorderParam }: Props) {
  const drawerRef = useRef<HTMLElement>(null);

  useEffect(() => {
    if (!open) return;
    drawerRef.current?.focus();
    const close = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', close);
    return () => window.removeEventListener('keydown', close);
  }, [onClose, open]);

  if (!open) return null;
  const ports = parseUnitPortsDraft(file.content);
  return (
    <>
      <button aria-label="Close Contract Settings" className="unit-settings-backdrop" onClick={onClose} type="button" />
      <aside
        aria-label="Contract Settings"
        aria-modal="true"
        className="unit-settings-drawer"
        data-testid="unit-settings-drawer"
        ref={drawerRef}
        role="dialog"
        tabIndex={-1}
      >
        <header>
          <div><span>Effect Contract</span><strong>Contract settings</strong></div>
          <button aria-label="Close Contract Settings" onClick={onClose} type="button">×</button>
        </header>
        <StructuredUnitEditor
          file={file}
          onChange={onChange}
          onReorderParam={onReorderParam}
          onSaveToLibrary={() => undefined}
          ports={ports}
          unit={unit}
        />
      </aside>
    </>
  );
}
