import { validateApgProjectPackage, type ApgProjectPackage } from './projectPackage.ts';
import {
  validatePersonalUnit,
  validateUnitPreset,
  type PersonalUnitRecord,
  type UnitPreset,
} from './presetLibrary.ts';

const DATABASE_NAME = 'apg-studio';
const DATABASE_VERSION = 1;
const PROJECT_STORE = 'projects';
const PRESET_STORE = 'presets';
const UNIT_STORE = 'units';

export type StudioRepository = {
  listProjects: () => Promise<ApgProjectPackage[]>;
  getProject: (id: string) => Promise<ApgProjectPackage | null>;
  saveProject: (project: ApgProjectPackage) => Promise<void>;
  deleteProject: (id: string) => Promise<void>;
  listPersonalPresets: () => Promise<UnitPreset[]>;
  savePersonalPreset: (preset: UnitPreset) => Promise<void>;
  deletePersonalPreset: (id: string) => Promise<void>;
  listPersonalUnits: () => Promise<PersonalUnitRecord[]>;
  savePersonalUnit: (unit: PersonalUnitRecord) => Promise<void>;
  deletePersonalUnit: (id: string) => Promise<void>;
};

function clone<T>(value: T): T {
  return JSON.parse(JSON.stringify(value)) as T;
}

function newestFirst<T extends { updatedAt: string }>(items: T[]): T[] {
  return items.sort((left, right) => right.updatedAt.localeCompare(left.updatedAt));
}

export class MemoryStudioRepository implements StudioRepository {
  private readonly projects = new Map<string, ApgProjectPackage>();
  private readonly presets = new Map<string, UnitPreset>();
  private readonly units = new Map<string, PersonalUnitRecord>();

  async listProjects(): Promise<ApgProjectPackage[]> {
    return [...this.projects.values()]
      .sort((left, right) => right.manifest.updatedAt.localeCompare(left.manifest.updatedAt))
      .map(clone);
  }

  async getProject(id: string): Promise<ApgProjectPackage | null> {
    const project = this.projects.get(id);
    return project ? clone(project) : null;
  }

  async saveProject(project: ApgProjectPackage): Promise<void> {
    const validated = validateApgProjectPackage(project);
    this.projects.set(validated.manifest.id, clone(validated));
  }

  async deleteProject(id: string): Promise<void> {
    this.projects.delete(id);
  }

  async listPersonalPresets(): Promise<UnitPreset[]> {
    return newestFirst([...this.presets.values()]).map(clone);
  }

  async savePersonalPreset(preset: UnitPreset): Promise<void> {
    const validated = validateUnitPreset(preset);
    if (validated.scope !== 'personal') throw new Error('Only personal presets can be saved.');
    this.presets.set(validated.id, clone(validated));
  }

  async deletePersonalPreset(id: string): Promise<void> {
    this.presets.delete(id);
  }

  async listPersonalUnits(): Promise<PersonalUnitRecord[]> {
    return newestFirst([...this.units.values()]).map(clone);
  }

  async savePersonalUnit(unit: PersonalUnitRecord): Promise<void> {
    const validated = validatePersonalUnit(unit);
    this.units.set(validated.id, clone(validated));
  }

  async deletePersonalUnit(id: string): Promise<void> {
    this.units.delete(id);
  }
}

function requestResult<T>(request: IDBRequest<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    request.addEventListener('success', () => resolve(request.result), { once: true });
    request.addEventListener('error', () => reject(request.error ?? new Error('IndexedDB request failed.')), { once: true });
  });
}

function transactionDone(transaction: IDBTransaction): Promise<void> {
  return new Promise((resolve, reject) => {
    transaction.addEventListener('complete', () => resolve(), { once: true });
    transaction.addEventListener('abort', () => reject(transaction.error ?? new Error('IndexedDB transaction aborted.')), { once: true });
    transaction.addEventListener('error', () => reject(transaction.error ?? new Error('IndexedDB transaction failed.')), { once: true });
  });
}

function openDatabase(factory: IDBFactory): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = factory.open(DATABASE_NAME, DATABASE_VERSION);
    request.addEventListener('upgradeneeded', () => {
      const database = request.result;
      if (!database.objectStoreNames.contains(PROJECT_STORE)) database.createObjectStore(PROJECT_STORE, { keyPath: 'manifest.id' });
      if (!database.objectStoreNames.contains(PRESET_STORE)) database.createObjectStore(PRESET_STORE, { keyPath: 'id' });
      if (!database.objectStoreNames.contains(UNIT_STORE)) database.createObjectStore(UNIT_STORE, { keyPath: 'id' });
    });
    request.addEventListener('success', () => resolve(request.result), { once: true });
    request.addEventListener('error', () => reject(request.error ?? new Error('Unable to open the APG project database.')), { once: true });
  });
}

export class IndexedDbStudioRepository implements StudioRepository {
  private readonly database: Promise<IDBDatabase>;

  constructor(factory: IDBFactory) {
    this.database = openDatabase(factory);
  }

  private async listStore<T>(storeName: string): Promise<T[]> {
    const database = await this.database;
    const transaction = database.transaction(storeName, 'readonly');
    const done = transactionDone(transaction);
    const items = await requestResult(transaction.objectStore(storeName).getAll()) as T[];
    await done;
    return items;
  }

  private async getStore<T>(storeName: string, id: string): Promise<T | null> {
    const database = await this.database;
    const transaction = database.transaction(storeName, 'readonly');
    const done = transactionDone(transaction);
    const item = await requestResult(transaction.objectStore(storeName).get(id)) as T | undefined;
    await done;
    return item ?? null;
  }

  private async putStore(storeName: string, value: unknown): Promise<void> {
    const database = await this.database;
    const transaction = database.transaction(storeName, 'readwrite');
    const done = transactionDone(transaction);
    await requestResult(transaction.objectStore(storeName).put(value));
    await done;
  }

  private async deleteStore(storeName: string, id: string): Promise<void> {
    const database = await this.database;
    const transaction = database.transaction(storeName, 'readwrite');
    const done = transactionDone(transaction);
    await requestResult(transaction.objectStore(storeName).delete(id));
    await done;
  }

  async listProjects(): Promise<ApgProjectPackage[]> {
    return (await this.listStore<ApgProjectPackage>(PROJECT_STORE))
      .map(validateApgProjectPackage)
      .sort((left, right) => right.manifest.updatedAt.localeCompare(left.manifest.updatedAt));
  }

  async getProject(id: string): Promise<ApgProjectPackage | null> {
    const project = await this.getStore<ApgProjectPackage>(PROJECT_STORE, id);
    return project ? validateApgProjectPackage(project) : null;
  }

  async saveProject(project: ApgProjectPackage): Promise<void> {
    await this.putStore(PROJECT_STORE, validateApgProjectPackage(project));
  }

  async deleteProject(id: string): Promise<void> {
    await this.deleteStore(PROJECT_STORE, id);
  }

  async listPersonalPresets(): Promise<UnitPreset[]> {
    return newestFirst((await this.listStore<UnitPreset>(PRESET_STORE)).map(validateUnitPreset));
  }

  async savePersonalPreset(preset: UnitPreset): Promise<void> {
    const validated = validateUnitPreset(preset);
    if (validated.scope !== 'personal') throw new Error('Only personal presets can be saved.');
    await this.putStore(PRESET_STORE, validated);
  }

  async deletePersonalPreset(id: string): Promise<void> {
    await this.deleteStore(PRESET_STORE, id);
  }

  async listPersonalUnits(): Promise<PersonalUnitRecord[]> {
    return newestFirst((await this.listStore<PersonalUnitRecord>(UNIT_STORE)).map(validatePersonalUnit));
  }

  async savePersonalUnit(unit: PersonalUnitRecord): Promise<void> {
    await this.putStore(UNIT_STORE, validatePersonalUnit(unit));
  }

  async deletePersonalUnit(id: string): Promise<void> {
    await this.deleteStore(UNIT_STORE, id);
  }
}

export function createStudioRepository(factory: IDBFactory | undefined = globalThis.indexedDB): StudioRepository {
  return factory ? new IndexedDbStudioRepository(factory) : new MemoryStudioRepository();
}
