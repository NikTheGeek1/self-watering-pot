#include "web_content.h"

namespace {

String htmlEscape(const String& text) {
  String escaped = text;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  return escaped;
}

}  // namespace

String buildProvisioningPage(const String& apSsid, const String& message) {
  const String safeSsid = htmlEscape(apSsid);
  const String safeMessage = htmlEscape(message);

  return String(R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Pot Setup</title>
  <style>
    body { font-family: Verdana, sans-serif; margin: 0; background: linear-gradient(180deg, #f2f8ef, #dfead6); color: #18331f; }
    main { max-width: 540px; margin: 0 auto; padding: 24px 18px 48px; }
    .card { background: rgba(255,255,255,0.92); border-radius: 18px; padding: 24px; box-shadow: 0 20px 40px rgba(19, 58, 26, 0.12); }
    h1 { margin-top: 0; font-size: 2rem; }
    p, label { line-height: 1.45; }
    .meta { font-size: 0.95rem; color: #40614a; }
    .notice { padding: 12px 14px; border-radius: 12px; background: #eef6ea; margin: 16px 0; }
    form { display: grid; gap: 14px; margin-top: 18px; }
    input { width: 100%; padding: 12px 14px; border: 1px solid #b8cab2; border-radius: 12px; font-size: 1rem; box-sizing: border-box; }
    button { padding: 12px 16px; border: 0; border-radius: 999px; background: #295e36; color: white; font-size: 1rem; cursor: pointer; }
    button:hover { background: #214d2c; }
    .small { font-size: 0.9rem; color: #55705d; }
  </style>
</head>
<body>
  <main>
    <div class="card">
      <h1>Smart Pot Setup</h1>
      <p class="meta">Join <strong>)HTML") +
         safeSsid +
         String(R"HTML(</strong>, then open <strong>http://192.168.4.1</strong>.</p>)HTML") +
         (safeMessage.isEmpty() ? String()
                                : String(R"HTML(<div class="notice">)HTML") + safeMessage +
                                      String(R"HTML(</div>)HTML")) +
         String(R"HTML(
      <form method="post" action="/api/provision">
        <label>
          Wi-Fi network name (SSID)
          <input type="text" name="ssid" autocomplete="off" required>
        </label>
        <label>
          Wi-Fi password
          <input type="password" name="password" autocomplete="off">
        </label>
        <button type="submit">Save and Connect</button>
      </form>
      <p class="small">Leave the password blank only for open Wi-Fi networks.</p>
    </div>
  </main>
</body>
</html>
)HTML");
}

String buildDashboardPage() {
  return String(R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Pot Dashboard</title>
  <style>
    :root { color-scheme: light; }
    body { margin: 0; font-family: "Trebuchet MS", sans-serif; color: #173123; background:
      radial-gradient(circle at top left, #fef6dc, transparent 28%),
      linear-gradient(180deg, #f7fbf2, #e5efe1 44%, #dce8d8); }
    main { max-width: 980px; margin: 0 auto; padding: 28px 18px 56px; }
    .hero { display: flex; flex-wrap: wrap; justify-content: space-between; gap: 18px; margin-bottom: 18px; }
    .hero h1 { margin: 0 0 8px; font-size: 2.3rem; }
    .hero p { margin: 0; color: #3b5c48; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 18px; }
    .card { background: rgba(255,255,255,0.9); border-radius: 22px; padding: 20px; box-shadow: 0 18px 42px rgba(28, 54, 29, 0.12); }
    h2 { margin-top: 0; font-size: 1.15rem; }
    dl { margin: 0; display: grid; grid-template-columns: 1fr auto; gap: 8px 12px; }
    dt { color: #58715d; }
    dd { margin: 0; font-weight: 700; }
    form { display: grid; gap: 12px; }
    label { display: grid; gap: 6px; font-size: 0.95rem; }
    input { padding: 10px 12px; border-radius: 12px; border: 1px solid #bacbb9; font-size: 1rem; }
    .actions { display: flex; flex-wrap: wrap; gap: 10px; }
    button { padding: 11px 14px; border: 0; border-radius: 999px; cursor: pointer; font-size: 0.95rem; background: #254d35; color: #fff; }
    button.secondary { background: #dce8d7; color: #23412d; }
    .banner { margin-bottom: 16px; min-height: 1.2em; color: #254d35; font-weight: 700; }
    .history-card { grid-column: 1 / -1; }
    .history-list { display: grid; gap: 12px; }
    .history-item { padding: 14px 16px; border-radius: 16px; background: #f3f8ef; border: 1px solid #d3e0d1; }
    .history-head { display: flex; justify-content: space-between; gap: 10px; align-items: center; margin-bottom: 10px; }
    .history-reason { font-size: 0.8rem; letter-spacing: 0.08em; text-transform: uppercase; color: #4b6d57; font-weight: 700; }
    .history-duration { font-size: 1.05rem; font-weight: 700; }
    .history-metrics { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 8px 14px; color: #23412d; }
    .history-metrics span { color: #5b7560; display: block; font-size: 0.82rem; margin-bottom: 3px; }
    .empty-state { margin: 0; color: #5b7560; }
  </style>
</head>
<body>
  <main>
    <section class="hero">
      <div>
        <h1>Smart Pot</h1>
        <p>Local dashboard for status, watering settings, and calibration.</p>
      </div>
      <div class="card" style="min-width:220px">
        <div style="font-size:0.85rem;color:#5a735e">Firmware</div>
        <div id="appVersion" style="font-size:1.35rem;font-weight:700">Loading...</div>
      </div>
    </section>

    <div id="message" class="banner"></div>

    <section class="grid">
      <div class="card">
        <h2>Status</h2>
        <dl>
          <dt>Wi-Fi state</dt><dd id="wifiState">-</dd>
          <dt>Wi-Fi SSID</dt><dd id="wifiSsid">-</dd>
          <dt>IP address</dt><dd id="ipAddress">-</dd>
          <dt>mDNS</dt><dd id="mdnsName">-</dd>
          <dt>OTA available</dt><dd id="otaAvailable">-</dd>
          <dt>Auto mode</dt><dd id="autoEnabled">-</dd>
          <dt>Pump running</dt><dd id="pumpRunning">-</dd>
          <dt>Status note</dt><dd id="statusMessage">-</dd>
        </dl>
      </div>

      <div class="card">
        <h2>Moisture</h2>
        <dl>
          <dt>Raw reading</dt><dd id="lastRawReading">-</dd>
          <dt>Moisture %</dt><dd id="lastMoisturePercent">-</dd>
          <dt>Dry calibration</dt><dd id="dryRaw">-</dd>
          <dt>Wet calibration</dt><dd id="wetRaw">-</dd>
          <dt>Threshold %</dt><dd id="dryThresholdPercent">-</dd>
          <dt>Sample interval</dt><dd id="sampleIntervalMs">-</dd>
          <dt>Cooldown</dt><dd id="cooldownMs">-</dd>
          <dt>Pump pulse</dt><dd id="pumpPulseMs">-</dd>
        </dl>
      </div>

      <div class="card">
        <h2>Settings</h2>
        <form id="settingsForm">
          <label>Dry threshold (%)
            <input id="thresholdInput" name="threshold" type="number" min="5" max="95" required>
          </label>
          <label>Pump pulse (ms)
            <input id="pulseInput" name="pulseMs" type="number" min="250" max="3000" required>
          </label>
          <label>Cooldown (ms)
            <input id="cooldownInput" name="cooldownMs" type="number" min="5000" max="600000" required>
          </label>
          <label>Sample interval (ms)
            <input id="sampleInput" name="sampleMs" type="number" min="1000" max="600000" required>
          </label>
          <label style="grid-template-columns:auto 1fr;align-items:center;gap:10px">
            <input id="autoInput" type="checkbox">
            Enable automatic watering
          </label>
          <div class="actions">
            <button type="submit">Save Settings</button>
          </div>
        </form>
      </div>

      <div class="card">
        <h2>Calibration</h2>
        <p>Use the current sensor reading as the dry or wet reference point.</p>
        <div class="actions">
          <button id="dryBtn" type="button">Capture Dry</button>
          <button id="wetBtn" type="button">Capture Wet</button>
          <button id="clearBtn" type="button" class="secondary">Clear Calibration</button>
        </div>
      </div>

      <div class="card history-card">
        <h2>Recent Watering</h2>
        <div id="historyList" class="history-list">
          <p class="empty-state">No watering history recorded yet.</p>
        </div>
      </div>
    </section>
  </main>

  <script>
    const messageEl = document.getElementById('message');

    function setMessage(text) {
      messageEl.textContent = text || '';
    }

    function setText(id, value) {
      document.getElementById(id).textContent = value;
    }

    function setInputValue(id, value) {
      const input = document.getElementById(id);
      if (document.activeElement !== input) {
        input.value = value;
      }
    }

    function escapeHtml(value) {
      return String(value)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;');
    }

    function formatPercent(value) {
      return value >= 0 ? value + '%' : 'uncalibrated';
    }

    function formatRaw(value) {
      return value >= 0 ? value : 'unset';
    }

    function formatDuration(value) {
      if (value < 1000) {
        return value + ' ms';
      }
      const seconds = value / 1000;
      return (Number.isInteger(seconds) ? seconds.toFixed(0) : seconds.toFixed(1)) + ' s';
    }

    function formatTimestamp(value) {
      if (!value || value <= 0) {
        return 'time unavailable';
      }

      return new Date(value).toLocaleString([], {
        year: 'numeric',
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
      });
    }

    function renderHistory(events) {
      const container = document.getElementById('historyList');
      if (!Array.isArray(events) || events.length === 0) {
        container.innerHTML = '<p class="empty-state">No watering history recorded yet.</p>';
        return;
      }

      container.innerHTML = events.map((event) => `
        <article class="history-item">
          <div class="history-head">
            <div class="history-reason">${escapeHtml(event.reason || 'unknown')}</div>
            <div class="history-duration">${escapeHtml(formatDuration(event.durationMs || 0))}</div>
          </div>
          <div class="history-metrics">
            <div><span>Started</span>${escapeHtml(formatTimestamp(event.startedAtEpochMs))}</div>
            <div><span>Start moisture</span>${escapeHtml(formatPercent(event.startPercent))}</div>
            <div><span>End moisture</span>${escapeHtml(formatPercent(event.endPercent))}</div>
            <div><span>Start raw</span>${escapeHtml(formatRaw(event.startRaw))}</div>
            <div><span>End raw</span>${escapeHtml(formatRaw(event.endRaw))}</div>
          </div>
        </article>
      `).join('');
    }

    async function fetchStatus() {
      const response = await fetch('/api/status', { cache: 'no-store' });
      if (!response.ok) {
        throw new Error('Unable to fetch status');
      }
      return response.json();
    }

    function syncView(data) {
      setText('appVersion', data.appVersion);
      setText('wifiState', data.wifiState);
      setText('wifiSsid', data.wifiSsid || '-');
      setText('ipAddress', data.ipAddress || '-');
      setText('mdnsName', data.mdnsName || '-');
      setText('otaAvailable', data.otaAvailable ? 'Yes' : 'No');
      setText('autoEnabled', data.autoEnabled ? 'ON' : 'OFF');
      setText('pumpRunning', data.pumpRunning ? 'YES' : 'NO');
      setText('statusMessage', data.statusMessage || '-');
      setText('lastRawReading', data.lastRawReading);
      setText('lastMoisturePercent', data.lastMoisturePercent >= 0 ? data.lastMoisturePercent + '%' : 'uncalibrated');
      setText('dryRaw', data.dryRaw >= 0 ? data.dryRaw : 'unset');
      setText('wetRaw', data.wetRaw >= 0 ? data.wetRaw : 'unset');
      setText('dryThresholdPercent', data.dryThresholdPercent + '%');
      setText('sampleIntervalMs', data.sampleIntervalMs + ' ms');
      setText('cooldownMs', data.cooldownMs + ' ms');
      setText('pumpPulseMs', data.pumpPulseMs + ' ms');
      renderHistory(data.wateringHistory);

      setInputValue('thresholdInput', data.dryThresholdPercent);
      setInputValue('pulseInput', data.pumpPulseMs);
      setInputValue('cooldownInput', data.cooldownMs);
      setInputValue('sampleInput', data.sampleIntervalMs);

      const autoInput = document.getElementById('autoInput');
      if (document.activeElement !== autoInput) {
        autoInput.checked = !!data.autoEnabled;
      }
    }

    async function postForm(url, body) {
      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
        body
      });
      const data = await response.json().catch(() => ({ message: 'Unexpected response' }));
      if (!response.ok || !data.ok) {
        throw new Error(data.message || 'Request failed');
      }
      return data;
    }

    async function refresh() {
      try {
        const status = await fetchStatus();
        syncView(status);
      } catch (error) {
        setMessage(error.message);
      }
    }

    document.getElementById('settingsForm').addEventListener('submit', async (event) => {
      event.preventDefault();
      const params = new URLSearchParams();
      params.set('threshold', document.getElementById('thresholdInput').value);
      params.set('pulseMs', document.getElementById('pulseInput').value);
      params.set('cooldownMs', document.getElementById('cooldownInput').value);
      params.set('sampleMs', document.getElementById('sampleInput').value);
      params.set('autoEnabled', document.getElementById('autoInput').checked ? '1' : '0');

      try {
        const result = await postForm('/api/settings', params.toString());
        setMessage(result.message);
        await refresh();
      } catch (error) {
        setMessage(error.message);
      }
    });

    document.getElementById('dryBtn').addEventListener('click', async () => {
      try {
        const result = await postForm('/api/calibration/dry', '');
        setMessage(result.message);
        await refresh();
      } catch (error) {
        setMessage(error.message);
      }
    });

    document.getElementById('wetBtn').addEventListener('click', async () => {
      try {
        const result = await postForm('/api/calibration/wet', '');
        setMessage(result.message);
        await refresh();
      } catch (error) {
        setMessage(error.message);
      }
    });

    document.getElementById('clearBtn').addEventListener('click', async () => {
      try {
        const result = await postForm('/api/calibration/clear', '');
        setMessage(result.message);
        await refresh();
      } catch (error) {
        setMessage(error.message);
      }
    });

    refresh();
    setInterval(refresh, 5000);
  </script>
</body>
</html>
)HTML");
}
