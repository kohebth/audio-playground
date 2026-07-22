import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

function normalizeBasePath(value: string | undefined): string {
  if (!value || value === '/') return '/'
  return `/${value.replace(/^\/+|\/+$/g, '')}/`
}

// https://vite.dev/config/
export default defineConfig({
  base: normalizeBasePath(process.env.VITE_BASE_PATH),
  plugins: [react()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    sourcemap: true,
    rollupOptions: {
      output: {
        sourcemapExcludeSources: true,
        manualChunks: {
          'graph-vendor': ['@xyflow/react'],
          'yaml-vendor': ['js-yaml'],
        },
      },
    },
  },
  worker: {
    rollupOptions: {
      output: {
        sourcemapExcludeSources: true,
      },
    },
  },
})
