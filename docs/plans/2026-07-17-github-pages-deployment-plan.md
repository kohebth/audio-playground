# Plan: GitHub Pages Deployment

**Date:** 2026-07-17
**Goal:** Deploy the complete Audio Playground v2 editor and its existing WASM/AudioWorklet runtime reproducibly to `https://kohebth.github.io/audio-playground/` through GitHub Actions, with production-path, CI, browser, diagnostics, and rollback acceptance proven.
**Approach:** Repair and complete the existing integrated Pages pipeline while adapting the supplied deployment architecture to the repository's actual `web-tools/` and `wasm-tools/` package boundaries.
**Complexity:** Complex

---

## Context Discovered

- The deployable Vite package is `web-tools/`; the supplied plan's example `web/` path is not the repository's current structure.
- The browser runtime is already integrated. Emscripten builds `apg_control` and `apg_processor`, `wasm-tools/` owns the message-based browser facade and AudioWorklet, and `web-tools/src/components/PreviewPanel.tsx` resolves runtime files through `import.meta.env.BASE_URL`.
- `.github/workflows/deploy-unit-editor.yml` partially implements a Pages workflow, but uses `npm install`, older Node actions, no lint/test/artifact validation, no `.nojekyll`, no pull-request production gate, and no production smoke test.
- The latest deployment failed during the web build because `web-tools/vite.config.ts` reads `process.env` without Node types in a clean install.
- GitHub currently reports Pages `build_type: legacy` with `main:/` as its publishing source. The target URL currently returns HTTP 404, and recent custom deployment runs failed.
- The app is currently a single-page React application with no router dependency. Hash routing is the selected Pages-safe foundation for `#/`, `#/projects`, and `#/unit/:unitId` URLs.
- Local autosave, project/unit/atom editing, drag-and-drop, structured runtime failures, AudioWorklet execution, repeated audio start/stop resource accounting, and browser tests already exist and should be reused rather than reimplemented.
- Generated browser WASM files are intentionally ignored under `web-tools/public/wasm/`; CI must build and stage them before the Vite build. Only `web-tools/dist/` is uploaded to Pages.
- The repository has no root `plan.md`, `task.md`, or `problem.md`; deployment state will be recorded in this plan, `docs/WEB_UI_READINESS.md`, and a focused operator document.
- `build-asan/` is unrelated untracked user state and must remain untouched.

## Execution Progress

- `d486a84` completed the production-safe web boundary: clean Node types, configurable base path, hash routes, public
  example YAML, commit diagnostics, and clean-install browser verification.
- `3cd764c` completed the deployment gates: current Pages workflow actions, clean CI installs, Emscripten/WASM smoke,
  artifact policy validation, production-build Playwright acceptance, and browser-compatible atom filtering.
- Local verification covers all repository-controlled build and application criteria. External Pages source migration,
  push/manual workflow runs, live HTTP/browser acceptance, and the final completion audit remain pending.

---

## Approaches Considered

| # | Approach | Pros | Cons | Effort |
|---|----------|------|------|--------|
| 1 | Repair the integrated editor + WASM pipeline ✓ | Preserves the working live engine, reaches the complete end state in one architecture, and avoids a later migration | Emscripten and browser smoke checks make CI slower | High |
| 2 | Deploy an editor-only artifact before enabling WASM | Isolates ordinary Pages path failures and can produce the first static page sooner | Temporarily regresses the current product, duplicates deployment acceptance, and leaves the requested audio end state incomplete | Medium initially, high overall |
| 3 | Extract reusable workflows and build orchestration first | Maximizes reuse between pull-request and deployment jobs | Adds workflow abstraction before the first successful deployment and increases debugging surface | Very high |

**Why option 1:** The repository already has a functioning message-based WASM/AudioWorklet path and browser resource tests. Keeping it in the artifact is shorter, avoids a deliberate feature regression, and satisfies the full deployment objective rather than an intermediate subset.

---

## Execution Steps

### Phase 1: Production-safe web boundary

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 1 | Replace the ad-hoc Pages base variable with typed `VITE_BASE_PATH` and `VITE_COMMIT_SHA` build inputs; make `dist`, source maps, and empty output behavior explicit | Clean `npm ci` production builds work at `/audio-playground/` and local development remains rooted at `/` | — |
| 2 | Add `HashRouter` at the application entry point and define stable hash locations for the current project workspace and unit selection | Direct visits and reloads of `#/`, `#/projects`, and `#/unit/overdrive` render without server fallback requirements | Step 1 |
| 3 | Normalize all favicon, WASM, AudioWorklet, worker, YAML, and future static URLs through Vite's base path; add explicitly distributable example YAML under `public/units/` with synchronization checks against its v2 contract source | Production requests remain under `/audio-playground/`; no private or legacy `units/` drafts enter the artifact | Step 1 |
| 4 | Expose the injected commit SHA and runtime/base-path information in Developer Diagnostics | A production operator can identify the exact deployed revision without inspecting generated files | Step 1 |

### Phase 2: Reproducible validation and deployment

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 5 | Replace the partial deploy workflow with `.github/workflows/deploy-pages.yml`, current official Pages actions, fixed Node 22, pinned Emscripten, `npm ci`, lint, tests, WASM smoke validation, and the production Vite build | Pull requests build and validate the exact production artifact; only pushes/dispatches from `main` can deploy | Phase 1 |
| 6 | Add a repository-owned artifact validator for required HTML/assets/WASM/worklet/YAML files, root-relative paths, localhost/file URLs, source-map presence, symlinks, and a documented bundle-size ceiling; create `.nojekyll` before upload | Invalid or unsafe artifacts fail before Pages upload and cannot replace production | Step 5 |
| 7 | Add a Playwright Pages smoke configuration that serves the built artifact at `/audio-playground/` and verifies hash navigation, editor surfaces, drag-and-drop, autosave reload, WASM initialization, AudioWorklet registration, clear media-permission failure, repeated start/stop cleanup, and no failed/root-relative/localhost production requests | Critical production behavior is exercised against `dist`, not the Vite development server | Steps 2-6 |
| 8 | Audit the exposed atom catalog against existing WASM/browser compatibility evidence and filter unsupported public atoms if the evidence does not cover them | The deployed catalog does not claim browser support for unvalidated atoms | Step 7 |

### Phase 3: Operations and live acceptance

| # | Step | Expected Output | Depends On |
|---|------|-----------------|------------|
| 9 | Document deployment inputs, public-data constraints, Pages configuration, diagnostics, routine rollback, and emergency rollback; align `docs/WEB_UI_READINESS.md` | Maintainers can reproduce and safely operate the deployment without editing `dist` or using a `gh-pages` branch | Phase 2 |
| 10 | Run clean local validation, commit only deployment-slice files, push `main`, switch the repository Pages source from legacy to workflow, and constrain the `github-pages` environment to `main` where GitHub permits | The custom workflow owns production publishing and external configuration matches the repository workflow | Step 9 |
| 11 | Monitor the main deployment, exercise `workflow_dispatch`, inspect the uploaded artifact and deployment metadata, and run live HTTP/Playwright acceptance at the target URL | The workflow is green, production returns HTTP 200, all static/runtime requests succeed from the repository subpath, and the displayed SHA matches the deployed commit | Step 10 |
| 12 | Perform a requirement-by-requirement completion audit against the supplied Phase 1-8 acceptance criteria and record any external/manual policy item that cannot be proven through APIs | Completion is supported by direct repository, Actions, Pages, HTTP, and browser evidence | Step 11 |

---

## Risks & Assumptions

| # | Type | Description | Mitigation |
|---|------|-------------|------------|
| 1 | Risk | Pages remains on legacy branch publishing after a correct workflow lands | Read the Pages API before mutation, switch `build_type` to `workflow`, then verify it after deployment |
| 2 | Risk | A clean install differs from the developer's existing `node_modules` and hides missing types or undeclared packages | Use `npm ci` locally and in Actions; keep lockfiles authoritative |
| 3 | Risk | A Vite development-server smoke test can conceal project-subpath bugs | Build with `/audio-playground/` and run smoke tests against `vite preview` at that exact path |
| 4 | Risk | AudioWorklet and microphone behavior differs on headless CI | Use Chromium fake-media flags for success paths, an isolated denied-permission context for failure UI, and assert engine resource snapshots |
| 5 | Risk | Generated WASM assets are stale or manually copied | Rebuild them in CI, run the Emscripten smoke test, validate artifact filenames, and never commit `dist` or generated `public/wasm` files |
| 6 | Risk | Source maps or static examples unintentionally expose private material | Publish only selected v2 fixtures explicitly approved as examples; scan the final artifact and document that all `VITE_*` values are public |
| 7 | Risk | Push or Pages-setting changes affect external production state | Make repository changes and local gates pass first, inspect the exact remote/repository target, then mutate only `kohebth/audio-playground` and monitor the resulting run |
| 8 | Assumption | The authenticated GitHub identity can push `main` and administer Pages settings | Verify repository/Pages API access before the external mutation; do not broaden credentials or expose secrets |
| 9 | Assumption | Current official major actions are `checkout@v6`, `setup-node@v6`, `configure-pages@v5`, `upload-pages-artifact@v4`, and `deploy-pages@v4` | Use current official GitHub documentation and recheck action metadata before committing the workflow |

---

## Success Criteria

- [ ] `web-tools/` installs reproducibly with `npm ci`; TypeScript, ESLint, unit/contract tests, WASM smoke tests, production build, artifact validation, and Pages Playwright smoke tests pass.
- [ ] The production artifact contains `index.html`, hashed CSS/JavaScript, `.nojekyll`, selected public YAML, `apg_control`/`apg_processor` JavaScript and WASM, and the AudioWorklet script at base-safe URLs.
- [ ] The artifact contains no absolute `/assets/`, `/wasm/`, or `/units/` reference, no `localhost` or `file://` runtime URL, no symlink, no repository secret, no private YAML, no test audio, and no native/debug binary.
- [ ] Hash URLs and reloads render the project browser and relevant project/unit editor state without a Pages 404.
- [ ] Drag-and-drop, YAML import/export surfaces, and locally autosaved projects work against the production build.
- [ ] WASM initializes from `/audio-playground/`, AudioWorklet registration succeeds, audio executes off the React thread, media permission failure is visible, DSP failure is contained, and repeated start/stop leaves no active processor or stream resource.
- [ ] Unsupported/unvalidated atoms are not presented as browser-compatible.
- [ ] The workflow runs on relevant pull requests and `main`, uses fixed Node and clean installs, and deploys only after every build gate succeeds.
- [ ] GitHub Pages reports workflow publishing, the `github-pages` environment is limited to `main` where configurable, and both a push deployment and manual `workflow_dispatch` complete successfully.
- [ ] `https://kohebth.github.io/audio-playground/` returns HTTP 200; HTML, CSS, JavaScript, YAML, WASM, and worklet requests return successfully with no production request to localhost or a domain-root asset path.
- [ ] Developer Diagnostics displays the exact deployed commit SHA, and the operator documentation explains standard and emergency rollback without editing build output or maintaining a `gh-pages` branch.
- [ ] A failed build cannot invoke the deploy job, preserving the previous successful Pages deployment.

---

## First Action

Make the clean production web boundary deterministic: add declared Node/Vite environment types, `VITE_BASE_PATH`, explicit build output settings, HashRouter, base-safe public example URLs, and build metadata diagnostics before changing the deployment workflow.
