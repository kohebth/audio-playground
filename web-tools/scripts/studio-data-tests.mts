import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import {
  createApgProjectPackage,
  createMonoAudioAsset,
  decodeBytesBase64,
  parseApgProjectPackage,
  serializeApgProjectPackage,
  validateApgProjectPackage,
} from '../src/lib/projectPackage.ts';
import {
  createEmptyProjectPackage,
  createWorkspaceTemplateProjectPackage,
} from '../src/lib/projectTemplates.ts';
import { evaluateWorkspaceReadiness } from '../src/lib/projectReadiness.ts';
import {
  BUILT_IN_UNIT_PRESETS,
  PERSONAL_UNIT_SCHEMA,
  PERSONAL_UNIT_VERSION,
  createPersonalPreset,
  listPresetsForUnit,
} from '../src/lib/presetLibrary.ts';
import { MemoryStudioRepository } from '../src/lib/studioRepository.ts';
import {
  findMigratableBrowserWorkspace,
  migrateApgProjectRouting,
  migrateLegacyWetDryWorkspace,
  migrateWorkspaceValue,
} from '../src/lib/workspaceMigrations.ts';
import { createWorkspacePayload } from '../src/lib/workspacePersistence.ts';
import { parseProjectGraphDraft } from '../src/lib/projectV2Graph.ts';

const createdAt = '2026-07-20T08:00:00.000Z';
const empty = createEmptyProjectPackage({ id: 'empty-1', name: 'First Board', now: createdAt });
assert.equal(empty.manifest.lastMode, 'simple');
assert.equal(empty.workspace.files.length, 1);
assert.match(empty.workspace.files[0].content, /units: \[\]/);
assert.match(empty.workspace.files[0].content, /from: system\.input\n      to: system\.output/);
const emptyReadiness = evaluateWorkspaceReadiness(empty.workspace);
assert.equal(emptyReadiness.validation, 'ready');
assert.equal(emptyReadiness.targets.wasm_realtime, 'ready');

const templateCopy = createWorkspaceTemplateProjectPackage({
  id: 'template-1',
  name: 'Template Copy',
  now: createdAt,
  description: 'Copied from a template.',
  entryProject: empty.workspace.entryProject,
  files: empty.workspace.files.map(file => ({ ...file, originalContent: file.content })),
});
assert.equal(templateCopy.manifest.name, 'Template Copy');
assert.equal(templateCopy.manifest.description, 'Copied from a template.');
assert.match(templateCopy.workspace.files[0].content, /name: first_board/);

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

const repo = resolve(import.meta.dirname, '../..');
const routingHelpers = {
  panner: { content: readFileSync(resolve(repo, 'test/fixtures/units-v2/path_panner_2.unit.v2.yaml'), 'utf8') },
  mixer: { content: readFileSync(resolve(repo, 'test/fixtures/units-v2/path_mixer_2.unit.v2.yaml'), 'utf8') },
};
const legacyParallelProject = `kind: apg.project
schema: apg.project.v2
name: legacy-parallel
version: 2.0.0
units:
  - id: overdrive_unit
    file: ../units/overdrive.unit.v2.yaml
  - id: wet_dry_mix_unit
    file: ../units/wet_dry_mix.unit.v2.yaml
chain:
  nodes:
    - id: drive1
      unit: overdrive_unit
      params: { drive: 2.2 }
    - id: blend
      unit: wet_dry_mix_unit
      params: { mix: 0.25 }
  routes:
    - from: system.input
      to: blend.dry
    - from: system.input
      to: drive1.input
    - from: drive1.output
      to: blend.wet
    - from: blend.output
      to: system.output
scenes:
  - name: Wider
    params: { blend.mix: 0.75 }
    bypass: { blend: true, drive1: false }
targets:
  default: desktop_full
`;
const legacyWorkspace = createWorkspacePayload('projects/legacy.project.v2.yaml', [
  {
    path: 'projects/legacy.project.v2.yaml',
    role: 'project',
    content: legacyParallelProject,
    originalContent: legacyParallelProject,
  },
  {
    path: 'units/overdrive.unit.v2.yaml',
    role: 'unit',
    content: readFileSync(resolve(repo, 'test/fixtures/units-v2/overdrive.unit.v2.yaml'), 'utf8'),
    originalContent: '',
  },
  {
    path: 'units/wet_dry_mix.unit.v2.yaml',
    role: 'unit',
    content: readFileSync(resolve(repo, 'test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml'), 'utf8'),
    originalContent: '',
  },
]);
const routingMigration = migrateLegacyWetDryWorkspace(legacyWorkspace, routingHelpers);
assert.equal(routingMigration.migratedSections, 1);
assert.equal(routingMigration.ambiguousSections, 0);
assert(routingMigration.workspace.files.some(file => file.path === 'units/path_panner_2.unit.v2.yaml'));
assert(routingMigration.workspace.files.some(file => file.path === 'units/path_mixer_2.unit.v2.yaml'));
const migratedProjectFile = routingMigration.workspace.files.find(file => file.path === legacyWorkspace.entryProject);
assert(migratedProjectFile);
const migratedDraft = parseProjectGraphDraft(migratedProjectFile.content);
const panner = migratedDraft.nodes.find(node => node.id === 'blend_pan');
const mixer = migratedDraft.nodes.find(node => node.id === 'blend');
assert.equal(panner?.routing?.section, 'parallel_1');
assert.equal(mixer?.routing?.section, 'parallel_1');
assert.equal(mixer?.params.path_1_db, '-2.4988');
assert.equal(mixer?.params.path_2_db, '-12.0412');
assert.deepEqual(migratedDraft.routes, [
  { from: 'system.input', to: 'blend_pan.input' },
  { from: 'blend_pan.path_1', to: 'blend.path_1' },
  { from: 'blend_pan.path_2', to: 'drive1.input' },
  { from: 'drive1.output', to: 'blend.path_2' },
  { from: 'blend.output', to: 'system.output' },
]);
assert.equal(migratedDraft.scenes[0].params['blend.path_1_db'], '-12.0412');
assert.equal(migratedDraft.scenes[0].params['blend.path_2_db'], '-2.4988');
assert(!('blend' in migratedDraft.scenes[0].bypass));
const repeatedMigration = migrateLegacyWetDryWorkspace(routingMigration.workspace, routingHelpers);
assert.equal(repeatedMigration.migratedSections, 0);
assert.deepEqual(repeatedMigration.workspace, routingMigration.workspace);

const ambiguousProject = legacyParallelProject.replace(
  '    - from: drive1.output\n      to: blend.wet\n',
  '    - from: drive1.output\n      to: system.output\n',
);
const ambiguousWorkspace = createWorkspacePayload(legacyWorkspace.entryProject, legacyWorkspace.files.map(file => ({
  ...file,
  content: file.role === 'project' ? ambiguousProject : file.content,
  originalContent: '',
})));
const ambiguousMigration = migrateLegacyWetDryWorkspace(ambiguousWorkspace, routingHelpers);
assert.equal(ambiguousMigration.migratedSections, 0);
assert.equal(ambiguousMigration.ambiguousSections, 1);
assert.deepEqual(ambiguousMigration.workspace, ambiguousWorkspace);

const legacyPackage = createApgProjectPackage(legacyWorkspace, {
  id: 'legacy-package',
  name: 'Legacy Package',
  createdAt,
  updatedAt: createdAt,
});
const migratedPackage = migrateApgProjectRouting(legacyPackage, routingHelpers);
assert.equal(migratedPackage.migratedSections, 1);
assert.equal(migratedPackage.project.manifest.id, legacyPackage.manifest.id);
assert.equal(migratedPackage.project.workspace.files.length, legacyWorkspace.files.length + 2);

console.log('studio data tests passed');
