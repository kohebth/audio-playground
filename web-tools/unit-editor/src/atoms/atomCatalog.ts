import type { AtomDef } from './atomCatalog.generated';

export * from './atomCatalog.generated';

export const CATEGORY_COLORS: Record<AtomDef['category'], string> = {
  generation: '#f59e0b',
  amplitude: '#10b981',
  filter: '#3b82f6',
  delay: '#8b5cf6',
  mix: '#ec4899',
  detect: '#06b6d4',
  modulation: '#f97316',
  nonlinear: '#ef4444',
  freq: '#6366f1',
  src: '#14b8a6',
  interpolation: '#84cc16',
  math: '#94a3b8',
};
