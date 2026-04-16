#!/usr/bin/env python3

import argparse
import json
import os
import sys
import time
import urllib.parse
import urllib.error
import urllib.request

REQUIRED_STATUS_KEYS = [
    "deviceLabel",
    "appVersion",
    "wifiState",
    "wifiSsid",
    "apSsid",
    "ipAddress",
    "mdnsName",
    "statusMessage",
    "otaAvailable",
    "otaInProgress",
    "lastRawReading",
    "lastMoisturePercent",
    "dryRaw",
    "wetRaw",
    "autoEnabled",
    "pumpRunning",
    "dryThresholdPercent",
    "pumpPulseMs",
    "cooldownMs",
    "sampleIntervalMs",
    "wateringHistory",
]


def required_env(name: str) -> str:
    value = os.getenv(name, "")
    if not value:
        raise SystemExit(f"Missing required environment variable: {name}")
    return value


def auth_header(user: str, password: str) -> str:
    token = f"{user}:{password}".encode("utf-8")
    return "Basic " + __import__("base64").b64encode(token).decode("ascii")


def request_json(path: str, method: str = "GET", data: dict | None = None) -> dict:
    host = required_env("SMART_POT_HOST")
    user = os.getenv("SMART_POT_USER", "smartpot")
    password = required_env("SMART_POT_PASSWORD")
    body = None
    headers = {"Authorization": auth_header(user, password)}
    if data is not None:
        body = urllib.parse.urlencode(data).encode("utf-8")
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    request = urllib.request.Request(f"http://{host}{path}", data=body, method=method, headers=headers)
    with urllib.request.urlopen(request, timeout=10) as response:
        return json.loads(response.read().decode("utf-8"))


def cmd_status() -> int:
    payload = request_json("/api/status")
    print(json.dumps(payload, indent=2))
    return 0


def cmd_status_schema() -> int:
    payload = request_json("/api/status")
    missing = [key for key in REQUIRED_STATUS_KEYS if key not in payload]
    if missing:
        print("Missing required status keys:")
        print("\n".join(missing))
        return 1

    if not isinstance(payload.get("wateringHistory"), list):
        print("wateringHistory must be a list.")
        return 1

    print("Status payload contains the expected top-level keys.")
    return 0


def cmd_auth_check() -> int:
    host = required_env("SMART_POT_HOST")
    request = urllib.request.Request(f"http://{host}/api/status", method="GET")
    try:
        urllib.request.urlopen(request, timeout=10)
    except urllib.error.HTTPError as error:
        if error.code == 401:
            print("Unauthorized request correctly rejected with 401.")
            return 0
        print(f"Unexpected HTTP status: {error.code}")
        return 1
    print("Expected 401 for unauthorized request, but request succeeded.")
    return 1


def cmd_manual_water() -> int:
    before = request_json("/api/status")
    response = request_json("/api/manual-water", method="POST")
    print(json.dumps(response, indent=2))
    time.sleep(2)
    after = request_json("/api/status")
    if len(after.get("wateringHistory", [])) < len(before.get("wateringHistory", [])):
        print("Watering history unexpectedly shrank.")
        return 1
    print("Manual watering endpoint responded and watering history is available.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Smart Pot hardware smoke checks")
    parser.add_argument("command", choices=["status", "status-schema", "auth-check", "manual-water"])
    args = parser.parse_args()

    if args.command == "status":
        return cmd_status()
    if args.command == "status-schema":
        return cmd_status_schema()
    if args.command == "auth-check":
        return cmd_auth_check()
    return cmd_manual_water()


if __name__ == "__main__":
    sys.exit(main())
