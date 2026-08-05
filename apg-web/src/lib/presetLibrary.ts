export const UNIT_PRESET_SCHEMA = 'apg.unit-preset.v1';
export const UNIT_PRESET_VERSION = 1;
export const PERSONAL_UNIT_SCHEMA = 'apg.personal-unit.v1';
export const PERSONAL_UNIT_VERSION = 1;

export type UnitPreset = {
  schema: typeof UNIT_PRESET_SCHEMA;
  version: typeof UNIT_PRESET_VERSION;
  id: string;
  name: string;
  description: string;
  unitName: string;
  params: Record<string, string>;
  scope: 'built-in' | 'personal';
  createdAt: string;
  updatedAt: string;
};

export type PersonalUnitRecord = {
  schema: typeof PERSONAL_UNIT_SCHEMA;
  version: typeof PERSONAL_UNIT_VERSION;
  id: string;
  name: string;
  title: string;
  category: string;
  description: string;
  content: string;
  createdAt: string;
  updatedAt: string;
};

const BUILT_IN_DATE = '2026-07-20T00:00:00.000Z';

function builtInPreset(
  id: string,
  name: string,
  description: string,
  unitName: string,
  params: Record<string, string>,
): UnitPreset {
  return {
    schema: UNIT_PRESET_SCHEMA,
    version: UNIT_PRESET_VERSION,
    id,
    name,
    description,
    unitName,
    params,
    scope: 'built-in',
    createdAt: BUILT_IN_DATE,
    updatedAt: BUILT_IN_DATE,
  };
}

export const BUILT_IN_UNIT_PRESETS: readonly UnitPreset[] = [
  builtInPreset('drive-warm-push', 'Warm Push', 'Soft edge with enough level to wake up an amp stage.', 'overdrive', {
    drive: '2.4', tone: '0.72', level: '0.82',
  }),
  builtInPreset('drive-tight-rhythm', 'Tight Rhythm', 'Focused drive with a controlled low end.', 'overdrive', {
    drive: '3.2', tone: '1.08', level: '0.74',
  }),
  builtInPreset('phaser-slow-orbit', 'Slow Orbit', 'A wide, slow sweep that stays behind the dry note.', 'phaser', {
    rate: '0.28', depth: '0.7', center: '820', feedback: '0.22', mix: '0.36',
  }),
  builtInPreset('chorus-studio-width', 'Studio Width', 'Subtle movement for clean guitar and keys.', 'chorus', {
    rate: '0.62', depth: '300', mix: '0.24',
  }),
  builtInPreset('delay-dotted-glow', 'Dotted Glow', 'Present repeats that leave room for the attack.', 'delay', {
    time_samples: '18000', feedback: '0.38', mix: '0.3',
  }),
  builtInPreset('reverb-small-room', 'Small Room', 'Short ambience for a close, finished sound.', 'schroeder_reverb', {
    decay: '0.48', mix: '0.17',
  }),
  builtInPreset('reverb-long-hall', 'Long Hall', 'A smooth tail for ambient passages.', 'schroeder_reverb', {
    decay: '0.84', mix: '0.32',
  }),
  builtInPreset('tremolo-slow-pulse', 'Slow Pulse', 'A rounded pulse with moderate depth.', 'tremolo', {
    rate: '2.4', depth: '0.42',
  }),
];

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function requiredString(value: unknown, field: string): string {
  if (typeof value !== 'string' || value.trim() === '') throw new Error(`${field} must be a non-empty string.`);
  return value;
}

function stringRecord(value: unknown, field: string): Record<string, string> {
  if (!isRecord(value)) throw new Error(`${field} must be an object.`);
  return Object.fromEntries(Object.entries(value).map(([key, item]) => {
    if (typeof item !== 'string') throw new Error(`${field}.${key} must be a string.`);
    return [key, item];
  }));
}

export function validateUnitPreset(value: unknown): UnitPreset {
  if (!isRecord(value)) throw new Error('Unit preset must be an object.');
  if (value.schema !== UNIT_PRESET_SCHEMA || value.version !== UNIT_PRESET_VERSION) {
    throw new Error(`Unit preset must use ${UNIT_PRESET_SCHEMA} version ${UNIT_PRESET_VERSION}.`);
  }
  if (value.scope !== 'built-in' && value.scope !== 'personal') throw new Error('Unit preset scope is invalid.');
  return {
    schema: UNIT_PRESET_SCHEMA,
    version: UNIT_PRESET_VERSION,
    id: requiredString(value.id, 'Unit preset id'),
    name: requiredString(value.name, 'Unit preset name'),
    description: typeof value.description === 'string' ? value.description : '',
    unitName: requiredString(value.unitName, 'Unit preset unitName'),
    params: stringRecord(value.params, 'Unit preset params'),
    scope: value.scope,
    createdAt: requiredString(value.createdAt, 'Unit preset createdAt'),
    updatedAt: requiredString(value.updatedAt, 'Unit preset updatedAt'),
  };
}

export function createPersonalPreset(
  input: Pick<UnitPreset, 'id' | 'name' | 'description' | 'unitName' | 'params'>,
  now = new Date().toISOString(),
): UnitPreset {
  return validateUnitPreset({
    ...input,
    schema: UNIT_PRESET_SCHEMA,
    version: UNIT_PRESET_VERSION,
    scope: 'personal',
    createdAt: now,
    updatedAt: now,
  });
}

export function validatePersonalUnit(value: unknown): PersonalUnitRecord {
  if (!isRecord(value)) throw new Error('Personal unit must be an object.');
  if (value.schema !== PERSONAL_UNIT_SCHEMA || value.version !== PERSONAL_UNIT_VERSION) {
    throw new Error(`Personal unit must use ${PERSONAL_UNIT_SCHEMA} version ${PERSONAL_UNIT_VERSION}.`);
  }
  return {
    schema: PERSONAL_UNIT_SCHEMA,
    version: PERSONAL_UNIT_VERSION,
    id: requiredString(value.id, 'Personal unit id'),
    name: requiredString(value.name, 'Personal unit name'),
    title: requiredString(value.title, 'Personal unit title'),
    category: requiredString(value.category, 'Personal unit category'),
    description: typeof value.description === 'string' ? value.description : '',
    content: requiredString(value.content, 'Personal unit content'),
    createdAt: requiredString(value.createdAt, 'Personal unit createdAt'),
    updatedAt: requiredString(value.updatedAt, 'Personal unit updatedAt'),
  };
}

export function listPresetsForUnit(unitName: string, personal: readonly UnitPreset[] = []): UnitPreset[] {
  return [...BUILT_IN_UNIT_PRESETS, ...personal]
    .filter(preset => preset.unitName === unitName)
    .sort((left, right) => left.scope === right.scope
      ? left.name.localeCompare(right.name)
      : left.scope === 'built-in' ? -1 : 1);
}
