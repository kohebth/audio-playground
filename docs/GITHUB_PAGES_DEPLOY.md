# GitHub Pages Deploy for Unit Editor

This repo’s web UI can be deployed from `web-tools` to GitHub Pages.

## Automatic deploy

The workflow `.github/workflows/deploy-unit-editor.yml` deploys on every push to
`main` and when manually triggered.

- Reads repository type to determine the path:
  - `/` for `*.github.io` repos
  - `/<repo-name>/` for project pages
- Builds the Vite app with `VITE_GITHUB_PAGES_BASE` set accordingly
- Publishes `web-tools/dist` to GitHub Pages

## Manual local preview for Pages path

From `web-tools`:

```sh
VITE_GITHUB_PAGES_BASE=/audio-playground/ npm run build
npm run preview
```

Replace `/audio-playground/` with `/` for user pages.

## Notes

- No backend is required to run the static app at build time for this workflow.
- If you later add routes, keep them compatible with client-side routing on Pages.
