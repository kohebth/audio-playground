export const UNIT_FAMILY_COLORS: Record<string, string> = {
  drive: '#ff9b75',
  dynamics: '#34d399',
  modulation: '#c4a7ff',
  amp: '#f1c56e',
  filter: '#84cc16',
  pitch: '#f97316',
  delay: '#3b82f6',
  reverb: '#06b6d4',
  spatial: '#14b8a6',
  synthesis: '#f472b6',
  looper: '#f43f5e',
  routing: '#94a3b8',
  utility: '#94a3b8',
  custom: '#a855f7',
};

export function parseUnitFamilies(familyInput?: unknown): string[] {
  if (!familyInput) return [];
  if (Array.isArray(familyInput)) {
    return familyInput.flatMap(item => parseUnitFamilies(item));
  }t
  if (typeof familyInput === 'string') {
    return familyInput
      .split(/[,;\s]+/)
      .map(item => item.trim().toLowerCase())
      .filter(Boolean);
  }
  return [];
}

export function unitFamilyColor(family?: string): string {
  const parsed = parseUnitFamilies(family);
  if (parsed.length === 0) {
    throw new Error('unit_family is compulsory and must be specified.');
  }
  const primary = parsed[0];
  const color = UNIT_FAMILY_COLORS[primary];
  if (!color) {
    throw new Error(`Unknown unit_family "${family}". Expected one of: ${Object.keys(UNIT_FAMILY_COLORS).join(', ')}.`);
  }
  return color;
}

export function unitFamiliesColors(familyInput?: unknown): string[] {
  const families = parseUnitFamilies(familyInput);
  if (families.length === 0) return ['#a855f7'];
  return families.map(fam => UNIT_FAMILY_COLORS[fam] ?? '#a855f7');
}

export function unitFamiliesGradient(familyInput?: unknown): string {
  const colors = unitFamiliesColors(familyInput);
  if (colors.length === 1) return colors[0];
  return `linear-gradient(135deg, ${colors.join(', ')})`;
}
