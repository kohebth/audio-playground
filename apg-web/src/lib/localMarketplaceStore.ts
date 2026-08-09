import {
  createApgProjectPackageFromFiles,
  parseApgProjectPackage,
  type ApgProjectPackage,
} from './projectPackage.ts';
import type { WorkspaceFile } from './backendSamples';
import { initialWorkspaceFiles } from './backendSamples';

export const LOCAL_MARKETPLACE_STORAGE_KEY = 'apg.marketplace.items.v1';

export type MarketplaceItem = {
  id: string;
  name: string;
  description: string;
  type: 'preset' | 'contract' | 'firmware';
  scope: 'built-in' | 'local';
  unitCount: number;
  routeCount: number;
  updatedAt: string;
  packageData: ApgProjectPackage;
};

function defaultMarketplaceItems(): MarketplaceItem[] {
  const defaultPackage = createApgProjectPackageFromFiles('test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml', initialWorkspaceFiles, {
    id: 'guitar-pedalboard-preset',
    name: 'Guitar Pedalboard Preset',
    description: 'Pro guitar pedalboard chain with Overdrive, Tone Stack, Tremolo, Chorus, Delay, & Reverb.',
    mode: 'pro',
  });

  return [
    {
      id: 'guitar-pedalboard-preset',
      name: 'Guitar Pedalboard Preset',
      description: 'Pro guitar pedalboard chain with Overdrive, Tone Stack, Tremolo, Chorus, Delay, & Reverb.',
      type: 'preset',
      scope: 'built-in',
      unitCount: 8,
      routeCount: 9,
      updatedAt: new Date().toISOString(),
      packageData: defaultPackage,
    },
  ];
}

export function loadLocalMarketplaceItems(): MarketplaceItem[] {
  const defaults = defaultMarketplaceItems();
  try {
    const raw = localStorage.getItem(LOCAL_MARKETPLACE_STORAGE_KEY);
    if (!raw) return defaults;
    const parsed = JSON.parse(raw);
    if (!Array.isArray(parsed)) return defaults;
    const locals: MarketplaceItem[] = parsed.flatMap(item => {
      try {
        const pkg = parseApgProjectPackage(JSON.stringify(item.packageData));
        return [{
          id: String(item.id ?? pkg.manifest.id),
          name: String(item.name ?? pkg.manifest.name),
          description: String(item.description ?? pkg.manifest.description),
          type: item.type === 'contract' || item.type === 'firmware' ? item.type : 'preset',
          scope: 'local' as const,
          unitCount: Number(item.unitCount ?? pkg.workspace.files.filter(f => f.role === 'unit').length),
          routeCount: Number(item.routeCount ?? 1),
          updatedAt: String(item.updatedAt ?? pkg.manifest.updatedAt),
          packageData: pkg,
        }];
      } catch {
        return [];
      }
    });

    const map = new Map<string, MarketplaceItem>();
    for (const item of [...defaults, ...locals]) {
      map.set(item.id, item);
    }
    return [...map.values()];
  } catch {
    return defaults;
  }
}

export function saveMarketplaceItem(item: MarketplaceItem): MarketplaceItem[] {
  const current = loadLocalMarketplaceItems();
  const filtered = current.filter(existing => existing.id !== item.id);
  const updated = [item, ...filtered];
  const localsOnly = updated.filter(i => i.scope === 'local');
  try {
    localStorage.setItem(LOCAL_MARKETPLACE_STORAGE_KEY, JSON.stringify(localsOnly));
  } catch {
    // QuotaExceededError fallback
  }
  return updated;
}

export function deleteMarketplaceItem(id: string): MarketplaceItem[] {
  const current = loadLocalMarketplaceItems();
  const updated = current.filter(item => item.id !== id || item.scope === 'built-in');
  const localsOnly = updated.filter(i => i.scope === 'local');
  try {
    localStorage.setItem(LOCAL_MARKETPLACE_STORAGE_KEY, JSON.stringify(localsOnly));
  } catch {
    // Fail silently
  }
  return updated;
}

export function saveWorkspaceAsMarketplacePreset(
  name: string,
  description: string,
  entryProject: string,
  files: WorkspaceFile[],
): MarketplaceItem {
  const id = `preset-${name.toLowerCase().replace(/[^a-z0-9]+/g, '-')}-${Date.now()}`;
  const pkg = createApgProjectPackageFromFiles(entryProject, files, {
    id,
    name,
    description,
  });

  const item: MarketplaceItem = {
    id,
    name,
    description,
    type: 'preset',
    scope: 'local',
    unitCount: files.filter(f => f.role === 'unit').length,
    routeCount: 1,
    updatedAt: new Date().toISOString(),
    packageData: pkg,
  };

  saveMarketplaceItem(item);
  return item;
}
