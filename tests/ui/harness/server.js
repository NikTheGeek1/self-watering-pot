const http = require("http");
const path = require("path");
const { execFileSync } = require("child_process");

const port = Number(process.env.PORT || 4173);
const renderBinary = path.resolve(__dirname, "../.tmp/render_web_content");
const authHeader =
  "Basic " + Buffer.from("smartpot:smart-pot-2026").toString("base64");

function defaultState() {
  return {
    deviceLabel: "Smart Pot",
    appVersion: "test-harness",
    wifiState: "StaConnected",
    wifiSsid: "TestNet",
    apSsid: "",
    ipAddress: "192.168.1.10",
    mdnsName: "smart-pot.local",
    statusMessage: "Connected to TestNet at 192.168.1.10",
    otaAvailable: true,
    otaInProgress: false,
    lastRawReading: 2850,
    lastMoisturePercent: 48,
    dryRaw: 3200,
    wetRaw: 1600,
    autoEnabled: false,
    pumpRunning: false,
    dryThresholdPercent: 35,
    pumpPulseMs: 1200,
    cooldownMs: 45000,
    sampleIntervalMs: 5000,
    wateringHistory: [],
  };
}

let state = defaultState();

function render(mode, apSsid = "Smart-Pot-Setup-ABCD", message = "") {
  return execFileSync(renderBinary, [mode, apSsid, message], {
    encoding: "utf8",
  });
}

function parseAuth(req) {
  return req.headers.authorization === authHeader;
}

function send(res, status, type, body, extraHeaders = {}) {
  res.writeHead(status, { "content-type": type, ...extraHeaders });
  res.end(body);
}

function collectBody(req) {
  return new Promise((resolve) => {
    let body = "";
    req.on("data", (chunk) => {
      body += chunk;
    });
    req.on("end", () => resolve(body));
  });
}

function parseForm(body) {
  const params = new URLSearchParams(body);
  return Object.fromEntries(params.entries());
}

function mergeState(patch) {
  state = { ...state, ...patch };
}

const server = http.createServer(async (req, res) => {
  if (req.url === "/health") {
    send(res, 200, "text/plain", "ok");
    return;
  }

  if (req.url === "/__test/reset" && req.method === "POST") {
    state = defaultState();
    send(res, 200, "application/json", JSON.stringify({ ok: true }));
    return;
  }

  if (req.url === "/__test/state" && req.method === "POST") {
    const body = await collectBody(req);
    mergeState(JSON.parse(body || "{}"));
    send(res, 200, "application/json", JSON.stringify({ ok: true, state }));
    return;
  }

  if (req.url === "/setup" && req.method === "GET") {
    send(res, 200, "text/html", render("provision", "Smart-Pot-Setup-ABCD", ""));
    return;
  }

  if (req.url === "/api/provision" && req.method === "POST") {
    const form = parseForm(await collectBody(req));
    if (!form.ssid) {
      send(
        res,
        400,
        "text/html",
        render("provision", "Smart-Pot-Setup-ABCD", "Wi-Fi SSID is required.")
      );
      return;
    }

    send(
      res,
      200,
      "text/html",
      render(
        "provision",
        "Smart-Pot-Setup-ABCD",
        `Saved Wi-Fi credentials for ${form.ssid}. The Smart Pot is attempting to join the network now.`
      )
    );
    return;
  }

  if (req.url === "/" && req.method === "GET") {
    if (!parseAuth(req)) {
      send(
        res,
        401,
        "text/plain",
        "Smart Pot credentials required.",
        { "WWW-Authenticate": 'Basic realm="Smart Pot"' }
      );
      return;
    }

    send(res, 200, "text/html", render("dashboard"));
    return;
  }

  if (req.url === "/api/status" && req.method === "GET") {
    if (!parseAuth(req)) {
      send(
        res,
        401,
        "application/json",
        JSON.stringify({ ok: false, message: "Unauthorized" }),
        { "WWW-Authenticate": 'Basic realm="Smart Pot"' }
      );
      return;
    }

    send(res, 200, "application/json", JSON.stringify(state));
    return;
  }

  if (!parseAuth(req) && req.url.startsWith("/api/")) {
    send(
      res,
      401,
      "application/json",
      JSON.stringify({ ok: false, message: "Unauthorized" }),
      { "WWW-Authenticate": 'Basic realm="Smart Pot"' }
    );
    return;
  }

  if (req.url === "/api/settings" && req.method === "POST") {
    const form = parseForm(await collectBody(req));
    if (form.autoEnabled === "1" && (state.dryRaw < 0 || state.wetRaw < 0 || state.dryRaw === state.wetRaw)) {
      send(
        res,
        409,
        "application/json",
        JSON.stringify({
          ok: false,
          message: "Auto mode requires both dry and wet calibration values.",
        })
      );
      return;
    }

    mergeState({
      dryThresholdPercent: Number(form.threshold),
      pumpPulseMs: Number(form.pulseMs),
      cooldownMs: Number(form.cooldownMs),
      sampleIntervalMs: Number(form.sampleMs),
      autoEnabled: form.autoEnabled === "1",
    });
    send(res, 200, "application/json", JSON.stringify({ ok: true, message: "Settings updated." }));
    return;
  }

  if (req.url === "/api/calibration/dry" && req.method === "POST") {
    mergeState({ dryRaw: state.lastRawReading });
    send(res, 200, "application/json", JSON.stringify({ ok: true, message: "Dry calibration saved from the current reading." }));
    return;
  }

  if (req.url === "/api/calibration/wet" && req.method === "POST") {
    mergeState({ wetRaw: state.lastRawReading });
    send(res, 200, "application/json", JSON.stringify({ ok: true, message: "Wet calibration saved from the current reading." }));
    return;
  }

  if (req.url === "/api/calibration/clear" && req.method === "POST") {
    mergeState({ dryRaw: -1, wetRaw: -1, autoEnabled: false });
    send(res, 200, "application/json", JSON.stringify({ ok: true, message: "Calibration cleared and auto mode disabled." }));
    return;
  }

  if (req.url === "/api/manual-water" && req.method === "POST") {
    if (state.otaInProgress) {
      send(res, 503, "application/json", JSON.stringify({ ok: false, message: "Manual watering is temporarily locked while OTA is in progress." }));
      return;
    }
    if (state.pumpRunning) {
      send(res, 409, "application/json", JSON.stringify({ ok: false, message: "Pump is already running." }));
      return;
    }

    mergeState({ pumpRunning: true });
    setTimeout(() => {
      state = {
        ...state,
        pumpRunning: false,
        wateringHistory: [
          {
            sequence: (state.wateringHistory[0]?.sequence || 0) + 1,
            reason: "manual",
            startedAtEpochMs: 1705000000000,
            durationMs: state.pumpPulseMs,
            startRaw: state.lastRawReading,
            startPercent: state.lastMoisturePercent,
            endRaw: state.lastRawReading - 5,
            endPercent: state.lastMoisturePercent - 1,
          },
          ...state.wateringHistory,
        ].slice(0, 5),
      };
    }, 10);

    send(
      res,
      200,
      "application/json",
      JSON.stringify({
        ok: true,
        message: `Manual watering started for ${state.pumpPulseMs} ms.`,
      })
    );
    return;
  }

  send(res, 404, "text/plain", "not found");
});

server.listen(port, "127.0.0.1");
