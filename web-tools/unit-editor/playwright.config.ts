import { defineConfig, devices } from '@playwright/test';

const browserMatrix = process.env.APG_BROWSER_MATRIX === '1';

const chromium = {
  name: 'chromium',
  use: {
    ...devices['Desktop Chrome'],
    permissions: ['microphone'],
    launchOptions: {
      args: ['--use-fake-device-for-media-stream', '--use-fake-ui-for-media-stream'],
    },
  },
};

export default defineConfig({
  testDir: './tests',
  timeout: 120_000,
  outputDir: './test-results/perf-ui',
  use: {
    baseURL: 'http://127.0.0.1:4173',
    ignoreHTTPSErrors: true,
  },
  projects: browserMatrix ? [
    chromium,
    { name: 'firefox', use: { ...devices['Desktop Firefox'] } },
    { name: 'webkit', use: { ...devices['Desktop Safari'] } },
  ] : [chromium],
  webServer: {
    command: 'npm run dev -- --host 127.0.0.1 --port 4173',
    url: 'http://127.0.0.1:4173',
    reuseExistingServer: !process.env.CI,
    timeout: 90_000,
  },
});
