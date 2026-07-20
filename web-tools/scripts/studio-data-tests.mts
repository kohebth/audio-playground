import assert from 'node:assert/strict';

import {
  createApgProjectPackage,
  createMonoAudioAsset,
  decodeBytesBase64,
  parseApgProjectPackage,
  serializeApgProjectPackage,
  validateApgProjectPackage,
} from '../src/lib/projectPackage.ts';
import { createEmptyProjectPackage } from '../src/lib/projectTemplates.ts';
import {
  BUILT_IN_UNIT_PRESETS,
  PERSONAL_UNIT_SCHEMA,
  PERSONAL_UNIT_VERSION,
  createPersonalPreset,
  listPresetsForUnit,
} from '../src/lib/presetLibrary.ts';
import { MemoryStudioRepository } from '../src/lib/studioRepository.ts';
import { findMigratableBrowserWorkspace, migrateWorkspaceValue } from '../src/lib/workspaceMigrations.ts';
import { createWorkspacePayload } from '../src/lib/workspacePersistence.ts';

const createdAt = '2026-07-20T08:00:00.000Z';
const empty = createEmptyProjectPackage({ id: 'empty-1', name: 'First Board', now: createdAt });
assert.equal(empty.manifest.lastMode, 'simple');
assert.equal(empty.workspace.files.length, 1);
assert.match(empty.workspace.files[0].content, /units: \[\]/);
assert.match(empty.workspace.files[0].content, /from: system\.input\n      to: system\.output/);

const audio = createMonoAudioAsset({
  id: 'audio-1',
  name: 'riff.wav',
  mimeType: 'audio/wav',
  channels: 1,
  sampleRate: 48_000,
  durationSeconds: 1.25,
  bytes: new Uint8Array([0, 1, 2, 253, 254, 255]),
});
assert.deepEqual([...decodeBytesBase64(audio.data)], [0, 1, 2, 253, 254, 255]);
assert.throws(
  () => createMonoAudioAsset({ ...audio, channels: 2, bytes: new Uint8Array() }),
  /Stereo and multi-channel files are not supported/,
);

const packaged = createApgProjectPackage(empty.workspace, {
  ...empty.manifest,
  mode: 'pro',
  audio: [audio],
});
const restored = parseApgProjectPackage(serializeApgProjectPackage(packaged));
assert.equal(restored.audio[0].name, 'riff.wav');
assert.equal(restored.manifest.lastMode, 'pro');
const stereo = structuredClone(restored) as unknown as Record<string, unknown>;
((stereo.audio as Array<Record<string, unknown>>)[0]).channels = 2;
assert.throws(() => validateApgProjectPackage(stereo), /only support mono/);

const later = createEmptyProjectPackage({
  id: 'empty-2',
  name: 'Later Board',
  now: '2026-07-20T09:00:00.000Z',
});
const repository = new MemoryStudioRepository();
await repository.saveProject(empty);
await repository.saveProject(later);
assert.deepEqual((await repository.listProjects()).map(project => project.manifest.id), ['empty-2', 'empty-1']);
assert.equal((await repository.getProject('empty-1'))?.manifest.name, 'First Board');
await repository.deleteProject('empty-1');
assert.equal(await repository.getProject('empty-1'), null);

const preset = createPersonalPreset({
  id: 'my-drive',
  name: 'My Drive',
  description: 'Saved from the board.',
  unitName: 'overdrive',
  params: { drive: '4.2', tone: '0.8', level: '0.7' },
}, createdAt);
await repository.savePersonalPreset(preset);
assert.equal((await repository.listPersonalPresets())[0].name, 'My Drive');
assert(listPresetsForUnit('overdrive', [preset]).some(item => item.scope === 'built-in'));
assert(listPresetsForUnit('overdrive', [preset]).some(item => item.id === 'my-drive'));
assert(BUILT_IN_UNIT_PRESETS.every(item => item.scope === 'built-in'));
assert.rejects(() => repository.savePersonalPreset(BUILT_IN_UNIT_PRESETS[0]), /Only personal presets/);

await repository.savePersonalUnit({
  schema: PERSONAL_UNIT_SCHEMA,
  version: PERSONAL_UNIT_VERSION,
  id: 'my-unit',
  name: 'my_unit',
  title: 'My Unit',
  category: 'Custom',
  description: 'A personal unit.',
  content: 'kind: apg.unit',
  createdAt,
  updatedAt: createdAt,
});
assert.equal((await repository.listPersonalUnits())[0].id, 'my-unit');

const workspace = createWorkspacePayload(empty.workspace.entryProject, empty.workspace.files.map(file => ({
  ...file,
  originalContent: file.content,
})));
const migratedV2 = migrateWorkspaceValue(workspace, { id: 'migrated-v2', name: 'Migrated', now: createdAt });
assert.equal(migratedV2.workspace.entryProject, workspace.entryProject);
const migratedV1 = migrateWorkspaceValue(workspace.files, {
  id: 'migrated-v1',
  name: 'Migrated Legacy',
  entryProject: workspace.entryProject,
  now: createdAt,
});
assert.equal(migratedV1.workspace.files.length, 1);
const browserMigration = findMigratableBrowserWorkspace({
  getItem: key => key.endsWith('.v2') ? JSON.stringify(workspace) : null,
}, { id: 'browser-migration', name: 'Recovered Project', now: createdAt });
assert.equal(browserMigration?.sourceKey, 'apg.unit-editor.workspace.v2');
assert.equal(browserMigration?.project.manifest.name, 'Recovered Project');

console.log('studio data tests passed');
