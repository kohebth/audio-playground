import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
  statSync,
} from 'node:fs';
import { relative, resolve, sep } from 'node:path';

const distDir = resolve(process.cwd(), process.argv[2] ?? 'dist');
const expectedBase = normalizeBase(process.env.VITE_BASE_PATH ?? '/audio-playground/');
const maxMainBundleBytes = 800 * 1024;
const maxArtifactBytes = 8 * 1024 * 1024;

function normalizeBase(value) {
  if (!value || value === '/') return '/';
  return `/${value.replace(/^\/+|\/+$/g, '')}/`;
}

function invariant(condition, message) {
  if (!condition) throw new Error(message);
}

function artifactPath(path) {
  return relative(distDir, path).split(sep).join('/');
}

function collectFiles(directory) {
  const files = [];
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const path = resolve(directory, entry.name);
    const metadata = lstatSync(path);
    invariant(!metadata.isSymbolicLink(), `Pages artifact must not contain symlink: ${artifactPath(path)}`);
    if (entry.isDirectory()) files.push(...collectFiles(path));
    else files.push(path);
  }
  return files;
}

invariant(existsSync(distDir) && statSync(distDir).isDirectory(), `Pages artifact directory is missing: ${distDir}`);

const files = collectFiles(distDir);
const names = files.map(artifactPath);
const nameSet = new Set(names);
const requiredFiles = [
  '.nojekyll',
  'index.html',
  'icon.svg',
  'units/overdrive.unit.v2.yaml',
  'wasm/apg_control.mjs',
  'wasm/apg_control.wasm',
  'wasm/apg_processor.mjs',
  'wasm/apg_processor.wasm',
  'wasm/processor.worklet.js',
];

for (const path of requiredFiles) {
  invariant(nameSet.has(path), `Pages artifact is missing required file: ${path}`);
}

const assetScripts = names.filter(path => /^assets\/[^/]+\.js$/.test(path));
const assetStyles = names.filter(path => /^assets\/[^/]+\.css$/.test(path));
const mainBundles = names.filter(path => /^assets\/index-[^/]+\.js$/.test(path));
invariant(assetScripts.length > 0, 'Pages artifact has no JavaScript assets');
invariant(assetStyles.length > 0, 'Pages artifact has no CSS assets');
invariant(mainBundles.length === 1, `Expected one main JavaScript bundle, found ${mainBundles.length}`);

for (const script of assetScripts) {
  invariant(nameSet.has(`${script}.map`), `JavaScript source map is missing: ${script}.map`);
}

const allowedYaml = new Set(['units/overdrive.unit.v2.yaml']);
const forbiddenBinary = /\.(?:a|dll|dylib|exe|lib|o|obj|pdb|so)$/i;
const forbiddenAudio = /\.(?:aac|flac|m4a|mp3|ogg|opus|wav)$/i;
const forbiddenName = /(?:^|\/)(?:\.env(?:\..*)?|.*\.secret|private[-_.].*)$/i;
const allowedRoots = new Set(['.nojekyll', 'assets', 'icon.svg', 'index.html', 'units', 'wasm']);

for (const name of names) {
  invariant(allowedRoots.has(name.split('/')[0]), `Unexpected Pages artifact entry: ${name}`);
  invariant(!forbiddenBinary.test(name), `Native or debug binary must not be published: ${name}`);
  invariant(!forbiddenAudio.test(name), `Unapproved audio must not be published: ${name}`);
  invariant(!forbiddenName.test(name), `Private or environment file must not be published: ${name}`);
  if (/\.ya?ml$/i.test(name)) invariant(allowedYaml.has(name), `Unapproved YAML must not be published: ${name}`);
}

const indexHtml = readFileSync(resolve(distDir, 'index.html'), 'utf8');
const localReferences = [...indexHtml.matchAll(/\b(?:href|src)=["']([^"']+)["']/g)]
  .map(match => match[1])
  .filter(value => value.startsWith('/') && !value.startsWith('//'));
invariant(localReferences.length > 0, 'index.html contains no local asset references');
for (const reference of localReferences) {
  invariant(reference.startsWith(expectedBase), `Root-relative index reference escapes ${expectedBase}: ${reference}`);
}

const textFiles = files.filter(path => /\.(?:css|html|js|mjs|svg|ya?ml)$/i.test(path) && !path.endsWith('.map'));
const rootRelativeAsset = /["'`(=:\s]\/((?:assets|wasm|units)\/|icon\.svg(?:[?"'`)\s]|$))/;
for (const path of textFiles) {
  const name = artifactPath(path);
  const content = readFileSync(path, 'utf8');
  const rootMatch = rootRelativeAsset.exec(content);
  invariant(!rootMatch, `Root-relative production asset path found in ${name}: /${rootMatch?.[1] ?? ''}`);
  invariant(!/file:\/\//i.test(content), `file:// URL found in Pages artifact: ${name}`);
  if (!/^assets\/index-[^/]+\.js$/.test(name)) {
    invariant(!/https?:\/\/(?:localhost|127\.0\.0\.1|\[::1\])/i.test(content), `Localhost URL found in Pages artifact: ${name}`);
  } else {
    invariant(!/https?:\/\/(?:localhost|127\.0\.0\.1|\[::1\]):\d+/i.test(content), `Localhost request URL found in main bundle: ${name}`);
  }
}

const mainBundleBytes = statSync(resolve(distDir, mainBundles[0])).size;
const artifactBytes = files.reduce((total, path) => total + statSync(path).size, 0);
invariant(
  mainBundleBytes <= maxMainBundleBytes,
  `Main JavaScript bundle exceeds ${maxMainBundleBytes} bytes: ${mainBundleBytes}`,
);
invariant(
  artifactBytes <= maxArtifactBytes,
  `Pages artifact exceeds ${maxArtifactBytes} bytes: ${artifactBytes}`,
);

console.log(`Pages artifact validated: ${names.length} files, ${artifactBytes} bytes, base ${expectedBase}`);
console.log(`Main JavaScript bundle: ${mainBundles[0]} (${mainBundleBytes} bytes)`);
