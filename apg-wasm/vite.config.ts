import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    lib: {
      entry: 'web/index.ts',
      formats: ['es'],
      fileName: 'wasm-tools',
    },
    target: 'es2020',
  },
});
