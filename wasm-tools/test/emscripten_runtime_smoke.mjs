import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const [artifactDirectory, repositoryRoot] = process.argv.slice(2);
if (!artifactDirectory || !repositoryRoot) throw new Error('Usage: emscripten_runtime_smoke.mjs <artifact-directory> <repository-root>');

async function loadModule(name) {
  const modulePath = resolve(artifactDirectory, `${name}.mjs`);
  const wasmBinary = await readFile(resolve(artifactDirectory, `${name}.wasm`));
  const { default: createModule } = await import(pathToFileURL(modulePath).href);
  return createModule({ wasmBinary });
}

function writeText(module, value) {
  const bytes = new TextEncoder().encode(value);
  const pointer = module._malloc(bytes.byteLength || 1);
  assert.notEqual(pointer, 0, 'WASM allocation failed');
  module.HEAPU8.set(bytes, pointer);
  return { pointer, bytes };
}

function withText(module, value, callback) {
  const encoded = writeText(module, value);
  try {
    return callback(encoded.pointer, encoded.bytes.byteLength);
  } finally {
    module._free(encoded.pointer);
  }
}

function diagnosticCode(module, control) {
  const pointer = module._apg_wasm_control_last_diagnostic(control);
  assert.notEqual(pointer, 0, 'Control diagnostic is unavailable');
  const codePointer = module.HEAPU32[(pointer >>> 2) + 4] ?? 0;
  return codePointer ? module.UTF8ToString(codePointer) : '';
}

function putFile(module, control, role, path, content) {
  return withText(module, path, (pathPointer, pathLength) =>
    withText(module, content, (contentPointer, contentLength) =>
      module._apg_wasm_control_put_file(control, role, pathPointer, pathLength, contentPointer, contentLength),
    ),
  );
}

const entryProject = 'test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml';
const files = [
  [1, entryProject],
  [2, 'test/fixtures/units-v2/noise_gate.unit.v2.yaml'],
  [2, 'test/fixtures/units-v2/overdrive.unit.v2.yaml'],
  [2, 'test/fixtures/units-v2/tone_stack.unit.v2.yaml'],
  [2, 'test/fixtures/units-v2/tremolo.unit.v2.yaml'],
  [2, 'test/fixtures/units-v2/delay.unit.v2.yaml'],
  [2, 'test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml'],
];

const controlModule = await loadModule('apg_control');
const control = controlModule._apg_wasm_control_create(0);
assert.notEqual(control, 0, 'Could not create Emscripten control runtime');

withText(controlModule, entryProject, (entryPointer, entryLength) => {
  assert.equal(controlModule._apg_wasm_control_begin_workspace(control, 1n, entryPointer, entryLength), 0);
});
const projectText = await readFile(resolve(repositoryRoot, entryProject), 'utf8');
assert.equal(putFile(controlModule, control, 1, entryProject, projectText), 0);
assert.notEqual(controlModule._apg_wasm_control_validate_workspace(control), 0);
assert.equal(diagnosticCode(controlModule, control), 'APG_UNIT_MISSING');

withText(controlModule, entryProject, (entryPointer, entryLength) => {
  assert.equal(controlModule._apg_wasm_control_begin_workspace(control, 2n, entryPointer, entryLength), 0);
});
for (const [role, path] of files) {
  assert.equal(putFile(controlModule, control, role, path, await readFile(resolve(repositoryRoot, path), 'utf8')), 0);
}
assert.equal(controlModule._apg_wasm_control_validate_workspace(control), 0);

const config = controlModule._malloc(16);
assert.notEqual(config, 0, 'Could not allocate Emscripten audio configuration');
const configWord = config >>> 2;
controlModule.HEAPU32[configWord] = 2;
controlModule.HEAPU32[configWord + 1] = 0;
controlModule.HEAPU32[configWord + 2] = 48000;
controlModule.HEAPU32[configWord + 3] = 64;
assert.equal(controlModule._apg_wasm_control_prepare_workspace(control, config), 0);
controlModule._free(config);

const imageSizePointer = controlModule._malloc(4);
const imagePointer = controlModule._apg_wasm_control_prepared_image(control, imageSizePointer);
const imageSize = controlModule.HEAPU32[imageSizePointer >>> 2] ?? 0;
controlModule._free(imageSizePointer);
assert.ok(imagePointer && imageSize, 'Emscripten control did not produce a prepared image');
const image = controlModule.HEAPU8.slice(imagePointer, imagePointer + imageSize);

const processorModule = await loadModule('apg_processor');
const processor = processorModule._apg_wasm_processor_create();
assert.notEqual(processor, 0, 'Could not create Emscripten processor runtime');
const processorImage = processorModule._malloc(image.byteLength);
assert.notEqual(processorImage, 0, 'Could not allocate processor image');
processorModule.HEAPU8.set(image, processorImage);
assert.equal(processorModule._apg_wasm_processor_stage_image(processor, processorImage, image.byteLength), 0);
processorModule._free(processorImage);
assert.equal(processorModule._apg_wasm_processor_commit_staged(processor, 2n), 0);

const input = processorModule._apg_wasm_processor_input_buffer(processor) >>> 2;
for (let frame = 0; frame < 64; frame += 1) processorModule.HEAPF32[input + frame] = 0.25;
assert.equal(processorModule._apg_wasm_processor_process(processor, 64), 0);
assert.equal(processorModule._apg_wasm_processor_active_revision(processor), 2n);
assert.notEqual(processorModule._apg_wasm_processor_output_meter(processor), 0, 'Emscripten processor did not expose meters');
assert.equal(processorModule._apg_wasm_processor_set_param(processor, 0, 0.2), 0);
assert.equal(processorModule._apg_wasm_processor_set_bypass(processor, 0, 1), 0);
assert.equal(processorModule._apg_wasm_processor_set_mute(processor, 1), 0);
assert.equal(processorModule._apg_wasm_processor_process(processor, 64), 0);
const output = processorModule._apg_wasm_processor_output_buffer(processor) >>> 2;
for (let frame = 0; frame < 64; frame += 1) assert.equal(processorModule.HEAPF32[output + frame], 0);

const corrupt = image.slice();
corrupt[corrupt.byteLength - 1] ^= 0x5a;
const corruptPointer = processorModule._malloc(corrupt.byteLength);
processorModule.HEAPU8.set(corrupt, corruptPointer);
assert.notEqual(processorModule._apg_wasm_processor_stage_image(processor, corruptPointer, corrupt.byteLength), 0);
processorModule._free(corruptPointer);
assert.equal(processorModule._apg_wasm_processor_active_revision(processor), 2n);

processorModule._apg_wasm_processor_destroy(processor);
controlModule._apg_wasm_control_destroy(control);
console.log('Emscripten control/processor smoke test passed');
