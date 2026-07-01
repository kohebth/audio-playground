import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

const githubPagesBase = process.env.VITE_GITHUB_PAGES_BASE || '/';

// https://vite.dev/config/
export default defineConfig({
  base: githubPagesBase,
  plugins: [react()],
})
