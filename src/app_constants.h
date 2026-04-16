#pragma once

#include <Arduino.h>

constexpr char kAppVersion[] = "1.0.5-water-history-start";
constexpr char kDeviceLabel[] = "Smart Pot";
constexpr char kHostname[] = "smart-pot";
constexpr char kMdnsName[] = "smart-pot.local";
constexpr char kWebAuthUser[] = "smartpot";
constexpr char kSharedSecret[] = "smart-pot-2026";
constexpr char kPreferencesNamespace[] = "smart-plant";
constexpr char kSoftApPrefix[] = "Smart-Pot-Setup-";
constexpr char kNtpServerPrimary[] = "pool.ntp.org";
constexpr char kNtpServerSecondary[] = "time.nist.gov";
constexpr char kNtpServerTertiary[] = "time.google.com";

constexpr uint8_t kRedLedPin = 14;
constexpr uint8_t kGreenLedPin = 13;
constexpr uint8_t kPumpPin = 26;
constexpr uint8_t kMoisturePin = 34;

constexpr unsigned long kHeartbeatPeriodMs = 2000;
constexpr unsigned long kHeartbeatOnMs = 150;
constexpr unsigned long kDiagnosticStepMs = 2000;

constexpr uint32_t kDefaultPumpPulseMs = 1200;
constexpr uint32_t kDefaultCooldownMs = 45000;
constexpr uint32_t kDefaultSampleIntervalMs = 5000;
constexpr uint8_t kDefaultDryThresholdPercent = 35;

constexpr uint32_t kMinPumpPulseMs = 250;
constexpr uint32_t kMaxPumpPulseMs = 3000;
constexpr uint32_t kMinCooldownMs = 5000;
constexpr uint32_t kMaxCooldownMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kMinSampleIntervalMs = 1000;
constexpr uint32_t kMaxSampleIntervalMs = 10UL * 60UL * 1000UL;
constexpr uint8_t kMinThresholdPercent = 5;
constexpr uint8_t kMaxThresholdPercent = 95;

constexpr size_t kCommandBufferSize = 96;
constexpr size_t kMoistureSamplesPerRead = 8;
constexpr size_t kWateringHistoryLimit = 5;

constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiConnectTimeoutMs = 10000;
constexpr unsigned long kWifiRetryGapMs = 5000;
constexpr unsigned long kNtpSyncCheckIntervalMs = 2000;
