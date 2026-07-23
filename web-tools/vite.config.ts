import { readFileSync } from 'node:fs'
import { resolve } from 'node:path'
import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'

function normalizeBasePath(value: string | undefined): string {
  if (!value || value === '/') return '/'
  return `/${value.replace(/^\/+|\/+$/g, '')}/`
}

function lanHttpsServer(mode: string) {
  if (mode !== 'lan-https') return undefined
  const env = loadEnv(mode, process.cwd(), '')
  const certificatePath = env.APG_HTTPS_CERT
  const keyPath = env.APG_HTTPS_KEY
  if (!certificatePath || !keyPath) {
    throw new Error(
      'npm run dev:https requires APG_HTTPS_CERT and APG_HTTPS_KEY. See web-tools/README.md for the trusted LAN setup.',
    )
  }
  return {
    host: '0.0.0.0',
    https: {
      cert: readFileSync(resolve(process.cwd(), certificatePath)),
      key: readFileSync(resolve(process.cwd(), keyPath)),
    },
  }
}

// https://vite.dev/config/
export default defineConfig(({ mode }) => ({
  base: normalizeBasePath(process.env.VITE_BASE_PATH),
  plugins: [react()],
  server: lanHttpsServer(mode),
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
}))
