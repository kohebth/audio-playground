import { defineConfig, devices } from '@playwright/test';

const pagesPath = '/audio-playground/';
const localBaseUrl = `http://127.0.0.1:${process.env.APG_PAGES_PORT ?? '4174'}${pagesPath}`;
const externalBaseUrl = process.env.APG_PAGES_BASE_URL?.replace(/\/?$/, '/');
const baseURL = externalBaseUrl ?? localBaseUrl;

export default defineConfig({
  testDir: './tests',
  testMatch: 'pages-smoke.spec.ts',
  timeout: 120_000,
  fullyParallel: false,
  forbidOnly: Boolean(process.env.CI),
  retries: process.env.CI ? 1 : 0,
  workers: 1,
  reporter: 'line',
  outputDir: './test-results/pages-smoke',
  use: {
    baseURL,
    ignoreHTTPSErrors: false,
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
    video: 'retain-on-failure',
  },
  projects: [
    {
      name: 'chromium-pages',
      use: {
        ...devices['Desktop Chrome'],
        permissions: ['microphone'],
        launchOptions: {
          args: [
            '--use-fake-device-for-media-stream',
            '--use-fake-ui-for-media-stream',
          ],
        },
      },
    },
  ],
  webServer: externalBaseUrl ? undefined : {
    command: `VITE_BASE_PATH=${pagesPath} npm run preview -- --host 127.0.0.1 --port ${process.env.APG_PAGES_PORT ?? '4174'}`,
    url: localBaseUrl,
    reuseExistingServer: !process.env.CI,
    timeout: 90_000,
  },
});
