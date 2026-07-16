import assert from 'node:assert/strict';

import {
  createWorkspacePayload,
  hydrateWorkspaceFiles,
  parseWorkspacePayload,
  persistWorkspacePayload,
  validateWorkspacePayload,
} from '../src/lib/workspacePersistence.ts';

const initial = [
  { path: 'projects/main.project.v2.yaml', role: 'project' as const, content: 'project', originalContent: 'project' },
  { path: 'units/gain.unit.v2.yaml', role: 'unit' as const, content: 'gain', originalContent: 'gain' },
];
const payload = createWorkspacePayload(initial[0].path, initial);
assert.equal(payload.version, 2);
assert.equal(payload.entryProject, initial[0].path);
assert.equal(parseWorkspacePayload(JSON.stringify(payload)).files.length, 2);

const writes: Array<{ key: string; value: string }> = [];
const persisted = persistWorkspacePayload('workspace', payload, {
  setItem: (key, value) => writes.push({ key, value }),
});
assert.equal(writes.length, 1);
assert.equal(writes[0].key, 'workspace');
assert.equal(writes[0].value, persisted);
assert.throws(
  () => persistWorkspacePayload('workspace', payload, { setItem: () => undefined }, () => {
    throw new Error('serialization failed');
  }),
  /serialization failed/,
);
assert.equal(writes.length, 1);
assert.throws(
  () => persistWorkspacePayload('workspace', payload, {
    setItem: () => {
      throw new Error('storage failed');
    },
  }),
  /storage failed/,
);

const edited = { ...payload, files: [
  { ...payload.files[0], content: 'edited project' },
  ...payload.files.slice(1),
  { path: 'workspace/new.unit.v2.yaml', role: 'unit' as const, content: 'new' },
] };
const hydrated = hydrateWorkspaceFiles(validateWorkspacePayload(edited), initial);
assert.equal(hydrated[0].originalContent, 'project');
assert.equal(hydrated[2].originalContent, '');

assert.throws(() => validateWorkspacePayload({ ...payload, version: 1 }), /format version 2/);
assert.throws(() => validateWorkspacePayload({ ...payload, entryProject: 'missing.project.v2.yaml' }), /entry project is missing/);
assert.throws(() => validateWorkspacePayload({ ...payload, files: [...payload.files, payload.files[0]] }), /duplicated/);
assert.throws(() => validateWorkspacePayload({ ...payload, files: [{ ...payload.files[0], path: '../escape.yaml' }] }), /invalid path/);
assert.throws(() => validateWorkspacePayload({ ...payload, files: [{ ...payload.files[0], role: 'other' }] }), /invalid role/);

console.log('workspace persistence tests passed');
