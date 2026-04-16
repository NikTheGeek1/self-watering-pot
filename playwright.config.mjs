import { defineConfig } from "@playwright/test";

export default defineConfig({
  testDir: "./tests/ui",
  timeout: 30000,
  use: {
    baseURL: "http://127.0.0.1:4173",
    httpCredentials: {
      username: "smartpot",
      password: "smart-pot-2026",
    },
    headless: true,
  },
  webServer: {
    command: "node tests/ui/harness/server.js",
    url: "http://127.0.0.1:4173/health",
    reuseExistingServer: !process.env.CI,
    timeout: 30000,
  },
});
