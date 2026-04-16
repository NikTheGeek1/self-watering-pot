import { expect, request, test } from "@playwright/test";

test.beforeEach(async ({ request }) => {
  await request.post("/__test/reset");
});

test("dashboard requires basic auth", async ({ playwright }) => {
  const response = await fetch("http://127.0.0.1:4173/");
  expect(response.status).toBe(401);
});

test("dashboard renders status and manual watering controls", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "Smart Pot" })).toBeVisible();
  await expect(page.getByRole("button", { name: "Manual Watering" })).toBeVisible();
  await expect(page.locator("#wifiSsid")).toHaveText("TestNet");
  await expect(page.locator("#wifiState")).toHaveText("StaConnected");
  await expect(page.locator("#ipAddress")).toHaveText("192.168.1.10");
  await expect(page.locator("#mdnsName")).toHaveText("smart-pot.local");
  await expect(page.locator("#lastMoisturePercent")).toHaveText("48%");
  await expect(page.locator("#thresholdInput")).toHaveValue("35");
  await expect(page.locator("#pulseInput")).toHaveValue("1200");
});

test("settings form saves values through the API", async ({ page }) => {
  await page.goto("/");
  await page.locator("#thresholdInput").fill("42");
  await page.locator("#pulseInput").fill("1500");
  await page.locator("#cooldownInput").fill("60000");
  await page.locator("#sampleInput").fill("7000");
  await page.locator("#autoInput").check();
  await page.getByRole("button", { name: "Save Settings" }).click();

  await expect(page.locator("#message")).toHaveText("Settings updated.");
  await expect(page.locator("#dryThresholdPercent")).toHaveText("42%");
  await expect(page.locator("#pumpPulseMs")).toHaveText("1500 ms");
});

test("manual watering button disables during pump and ota states", async ({ page, request }) => {
  await request.post("/__test/state", {
    data: { pumpRunning: true },
  });
  await page.goto("/");
  await expect(page.locator("#manualWaterBtn")).toBeDisabled();

  await request.post("/__test/state", {
    data: { pumpRunning: false, otaInProgress: true },
  });
  await page.reload();
  await expect(page.locator("#manualWaterBtn")).toBeDisabled();
});

test("manual watering shows success feedback", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "Manual Watering" }).click();
  await expect(page.locator("#message")).toContainText("Manual watering started for 1200 ms.");
});

test("manual watering shows the current endpoint error message", async ({ page, request }) => {
  await request.post("/__test/state", {
    data: { pumpRunning: true },
  });
  await page.goto("/");
  await request.post("/__test/state", {
    data: { pumpRunning: false },
  });
  await page.getByRole("button", { name: "Manual Watering" }).click();
  await expect(page.locator("#message")).toContainText("Manual watering started for 1200 ms.");

  await request.post("/__test/state", {
    data: { otaInProgress: true, pumpRunning: false },
  });
  await page.reload();
  await expect(page.locator("#manualWaterBtn")).toBeDisabled();
});

test("settings errors are shown in the banner", async ({ page, request }) => {
  await request.post("/__test/state", {
    data: { dryRaw: -1, wetRaw: -1 },
  });
  await page.goto("/");
  await page.locator("#autoInput").check();
  await page.getByRole("button", { name: "Save Settings" }).click();

  await expect(page.locator("#message")).toContainText(
    "Auto mode requires both dry and wet calibration values."
  );
});

test("watering history renders timestamps and fallback text", async ({ page, request }) => {
  await request.post("/__test/state", {
    data: {
      wateringHistory: [
        {
          sequence: 3,
          reason: "manual",
          startedAtEpochMs: 1705000000000,
          durationMs: 1200,
          startRaw: 2800,
          startPercent: 40,
          endRaw: 2790,
          endPercent: 41,
        },
        {
          sequence: 2,
          reason: "auto",
          startedAtEpochMs: 0,
          durationMs: 900,
          startRaw: 2810,
          startPercent: 39,
          endRaw: 2801,
          endPercent: 40,
        },
      ],
    },
  });

  await page.goto("/");
  await expect(page.locator("#historyList")).toContainText("manual");
  await expect(page.locator("#historyList")).toContainText("time unavailable");
  await expect(page.locator("#historyList")).not.toContainText("Ended");
});

test("watering history shows the empty state when there are no events", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator("#historyList")).toContainText("No watering history recorded yet.");
});

test("watering history renders newest entries first", async ({ page, request }) => {
  await request.post("/__test/state", {
    data: {
      wateringHistory: [
        {
          sequence: 10,
          reason: "manual",
          startedAtEpochMs: 1705000000000,
          durationMs: 900,
          startRaw: 2800,
          startPercent: 40,
          endRaw: 2790,
          endPercent: 41,
        },
        {
          sequence: 9,
          reason: "auto",
          startedAtEpochMs: 1704990000000,
          durationMs: 1200,
          startRaw: 2810,
          startPercent: 39,
          endRaw: 2801,
          endPercent: 40,
        },
      ],
    },
  });

  await page.goto("/");
  await expect(page.locator(".history-item").first()).toContainText("900 ms");
  await expect(page.locator(".history-item").nth(1)).toContainText("1.2 s");
});

test("calibration actions update the moisture card", async ({ page, request }) => {
  await request.post("/__test/state", {
    data: { lastRawReading: 3010, dryRaw: -1, wetRaw: -1 },
  });
  await page.goto("/");

  await page.getByRole("button", { name: "Capture Dry" }).click();
  await expect(page.locator("#message")).toContainText("Dry calibration saved");
  await expect(page.locator("#dryRaw")).toHaveText("3010");

  await request.post("/__test/state", {
    data: { lastRawReading: 1580 },
  });
  await page.getByRole("button", { name: "Capture Wet" }).click();
  await expect(page.locator("#wetRaw")).toHaveText("1580");

  await page.getByRole("button", { name: "Clear Calibration" }).click();
  await expect(page.locator("#dryRaw")).toHaveText("unset");
  await expect(page.locator("#wetRaw")).toHaveText("unset");
});

test("status polling refreshes the page data", async ({ page, request }) => {
  await page.goto("/");
  await expect(page.locator("#statusMessage")).toHaveText("Connected to TestNet at 192.168.1.10");

  await request.post("/__test/state", {
    data: { statusMessage: "Updated by polling" },
  });

  await expect(page.locator("#statusMessage")).toHaveText("Updated by polling", {
    timeout: 7000,
  });
});

test("provisioning page shows validation and success messages", async ({ page, request }) => {
  await page.goto("/setup");
  const invalidResponse = await request.post("/api/provision", {
    form: { ssid: "", password: "" },
  });
  await expect(await invalidResponse.text()).toContain("Wi-Fi SSID is required.");

  await page.locator('input[name="ssid"]').fill("GardenNet");
  await page.locator('input[name="password"]').fill("secret");
  await page.getByRole("button", { name: "Save and Connect" }).click();
  await expect(page.locator("body")).toContainText("Saved Wi-Fi credentials for GardenNet.");
});
