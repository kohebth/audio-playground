export const buildInfo = Object.freeze({
  basePath: import.meta.env.BASE_URL,
  commitSha: import.meta.env.VITE_COMMIT_SHA?.trim() || 'development',
  mode: import.meta.env.MODE,
});
