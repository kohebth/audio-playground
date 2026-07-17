# GitHub Pages Deployment

Audio Playground is deployed from `kohebth/audio-playground` to:

`https://kohebth.github.io/audio-playground/`

GitHub Actions is the only production publisher. Do not commit `dist`, maintain a `gh-pages` branch, or edit generated
production files. GitHub Pages hosts the React application, v2 example YAML, Web Workers, AudioWorklet JavaScript, and
Emscripten JavaScript/WASM modules; it does not run the native C11 runtime or a backend service.

## Production boundary

The deployable package is `web-tools/`, and only `web-tools/dist/` is uploaded. A valid artifact contains:

```text
dist/
├── .nojekyll
├── index.html
├── icon.svg
├── assets/
│   ├── index-<hash>.js
│   ├── index-<hash>.css
│   └── *.js.map
├── units/
│   └── overdrive.unit.v2.yaml
└── wasm/
    ├── apg_control.mjs
    ├── apg_control.wasm
    ├── apg_processor.mjs
    ├── apg_processor.wasm
    └── processor.worklet.js
```

Generated files under `web-tools/public/wasm/` and `web-tools/dist/` are ignored. The workflow rebuilds them from source
for every candidate artifact. The public YAML allowlist contains only the v2 overdrive fixture; legacy `units/` drafts,
test audio, native/debug binaries, environment files, private YAML, repository secrets, and local configuration are not
published.

## Workflow

`.github/workflows/deploy-pages.yml` runs for relevant pull requests, relevant pushes to `main`, and manual dispatches.
Its build job uses Ubuntu 24.04, Node 22, Emscripten 5.0.1, and clean `npm ci` installs.

The build gate performs, in order:

1. Build and typecheck the `wasm-tools/` facade.
2. Compile `apg_control` and `apg_processor` with Emscripten and run the Node WASM runtime smoke test.
3. Stage the generated WASM modules and processor Worklet under `web-tools/public/wasm/`.
4. Install `web-tools/` dependencies with `npm ci`.
5. Run TypeScript, ESLint, contract/unit tests, and a production Vite build.
6. Add `.nojekyll` and validate the complete static artifact.
7. Serve the built artifact below `/audio-playground/` and run the Chromium Pages smoke suite.
8. Upload the Pages artifact only for a push or manual run; pull requests never deploy.
9. Run the deployment job only for `refs/heads/main` after the build job succeeds.

The artifact validator requires all runtime assets and source maps, enforces the public YAML and root-entry allowlists,
rejects symlinks, native/debug files, test audio, environment/private files, root-relative asset URLs, `file://` URLs,
and localhost request URLs, and caps the main JavaScript bundle at 800 KiB and the complete artifact at 8 MiB. The
browser smoke test is the authoritative check that every same-origin production request remains below
`/audio-playground/` and returns below HTTP 400.

## Build inputs

The production build accepts only these Vite inputs:

| Variable | Production value | Visibility |
|---|---|---|
| `VITE_BASE_PATH` | `/audio-playground/` | Public; compiled into asset URLs |
| `VITE_COMMIT_SHA` | `${{ github.sha }}` | Public; shown in Developer Diagnostics |

Never put API keys, tokens, credentials, or private endpoints in `VITE_*`; Vite values are browser-visible. The smoke
runner also accepts `APG_PAGES_BASE_URL` and `APG_EXPECTED_COMMIT_SHA`, but those are test-process inputs and are not
compiled into the application.

## Local reproduction

From the repository root, build the same Emscripten modules used by CI:

```sh
cd wasm-tools
npm ci
npm run build
cd ..
./wasm-tools/build-emscripten-docker.sh
```

Then build and validate the Pages artifact:

```sh
cd web-tools
npm ci
npm run typecheck
npm run lint
npm test
VITE_BASE_PATH=/audio-playground/ VITE_COMMIT_SHA=local-pages npm run build
touch dist/.nojekyll
VITE_BASE_PATH=/audio-playground/ npm run pages:validate
APG_EXPECTED_COMMIT_SHA=local-pages npm run pages:smoke
```

The smoke configuration starts `vite preview` with the production base path. It checks hash-route reloads, the project,
unit, and atom surfaces, the public YAML response, release diagnostics, drag-and-drop, autosave after reload, workspace
export, processor WASM loading, AudioWorklet start/stop cleanup, microphone cleanup and permission failure, invalid DSP
containment/recovery, and failed or out-of-base same-origin requests.

## Repository configuration

In **Settings → Pages → Build and deployment**, Source must be **GitHub Actions**. The equivalent authenticated API
change is:

```sh
gh api --method PUT repos/kohebth/audio-playground/pages -f build_type=workflow
```

The `github-pages` environment must use a custom deployment branch policy allowing only the `main` branch. Verify both
settings without changing them:

```sh
gh api repos/kohebth/audio-playground/pages
gh api repos/kohebth/audio-playground/environments/github-pages
gh api repos/kohebth/audio-playground/environments/github-pages/deployment-branch-policies
```

No production secret is required. Optional required reviewers can be added to `github-pages` through repository policy;
do not add a reviewer merely to store or reveal a frontend credential.

## Deploy and monitor

A relevant merge or push to `main` triggers the workflow. A manual production rebuild uses:

```sh
gh workflow run deploy-pages.yml --ref main
gh run list --workflow deploy-pages.yml --limit 5
gh run watch <run-id> --exit-status
```

After deployment, verify HTTP and the browser flow:

```sh
curl -I https://kohebth.github.io/audio-playground/
cd web-tools
APG_PAGES_BASE_URL=https://kohebth.github.io/audio-playground/ \
APG_EXPECTED_COMMIT_SHA=<deployed-sha> npm run pages:smoke
```

In the application, open **Contract → Developer Diagnostics** and compare **Build commit** with the deployed workflow's
SHA. **Deployment base** must be `/audio-playground/`, and **Build mode** must be `production`.

## Failure and rollback

The deploy job depends on the complete build job. A failed dependency install, typecheck, lint, test, WASM build, artifact
validation, or browser smoke test skips deployment, so the previous successful Pages version remains active.

Standard rollback:

1. Identify the last stable commit and the first defective commit.
2. Create a rollback branch from current `main`.
3. Revert the defective commit or commit range; do not rewrite `main` history.
4. Open and merge the rollback pull request.
5. Let the same Pages workflow validate and deploy the revert.
6. Confirm the displayed build SHA and rerun live smoke acceptance.

Emergency rollback uses the same history-preserving route: prepare the smallest revert/hotfix against `main`, merge it,
and manually dispatch `deploy-pages.yml` from `main` if the path filter did not trigger. The `github-pages` environment
intentionally rejects tag or non-main deployments. If the replacement build fails, fix the build gate rather than
bypassing it; production remains on the last successful artifact.

Never roll back by editing `web-tools/dist`, changing Pages back to legacy branch publishing, force-pushing `main`, or
maintaining production in a `gh-pages` branch.

## Common failures

| Symptom | Check |
|---|---|
| Production URL returns 404 before a workflow deploy | Pages `build_type` is still `legacy`, or no Actions artifact has deployed |
| CSS, JavaScript, YAML, or WASM returns 404 | `VITE_BASE_PATH` was not `/audio-playground/`, or a root-relative URL bypassed `BASE_URL` |
| Clean CI cannot typecheck `vite.config.ts` | `@types/node` is missing from `web-tools` dev dependencies |
| Audio engine remains in `error` during initialization | Inspect the control module/WASM responses and structured Developer Diagnostics |
| Audio starts locally but not on Pages | Confirm HTTPS secure context and the Worklet, processor module, and processor WASM artifact files |
| Artifact validation rejects size | Review the generated bundle report; raise a limit only with an intentional, documented payload decision |
| Deploy job is skipped after a green build | Confirm the event is a push/manual dispatch on `refs/heads/main` and the environment policy allows `main` |
