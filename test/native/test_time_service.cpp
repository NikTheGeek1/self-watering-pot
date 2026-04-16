#include <gtest/gtest.h>

#include "time_service.h"

namespace {

struct FakeTimeEnv {
  int configureCalls = 0;
  time_t seconds = 0;
  uint64_t epochMs = 0;
} gFakeTimeEnv;

void fakeConfigure(const char*, const char*, const char*) { ++gFakeTimeEnv.configureCalls; }

time_t fakeReadSeconds() { return gFakeTimeEnv.seconds; }

bool fakeReadTimespec(struct timespec* out) {
  if (out == nullptr) {
    return false;
  }

  out->tv_sec = static_cast<time_t>(gFakeTimeEnv.epochMs / 1000ULL);
  out->tv_nsec = static_cast<long>((gFakeTimeEnv.epochMs % 1000ULL) * 1000000ULL);
  return true;
}

const TimeServiceOps kFakeOps = {
    fakeConfigure,
    fakeReadSeconds,
    fakeReadTimespec,
};

class TimeServiceTest : public ::testing::Test {
 protected:
  void SetUp() override { gFakeTimeEnv = FakeTimeEnv{}; }
};

}  // namespace

TEST_F(TimeServiceTest, BeginsUnsynchronizedWhenEpochIsInvalid) {
  TimeService service(&kFakeOps);
  service.begin();

  EXPECT_FALSE(service.isSynchronized());
  EXPECT_EQ(service.currentEpochMs(), 0u);
}

TEST_F(TimeServiceTest, BeginsSynchronizedWhenEpochIsAlreadyValid) {
  gFakeTimeEnv.seconds = 1705000000;
  gFakeTimeEnv.epochMs = 1705000000123ULL;

  TimeService service(&kFakeOps);
  service.begin();

  EXPECT_TRUE(service.isSynchronized());
  EXPECT_EQ(service.currentEpochMs(), 1705000000123ULL);
}

TEST_F(TimeServiceTest, StartSyncRequestsNtpConfigurationImmediately) {
  TimeService service(&kFakeOps);
  service.begin();

  service.startSync();

  EXPECT_EQ(gFakeTimeEnv.configureCalls, 1);
}

TEST_F(TimeServiceTest, StartsSyncOnceAndBecomesSynchronizedAfterValidTimeAppears) {
  TimeService service(&kFakeOps);
  service.begin();

  service.tick(1000, false);
  EXPECT_EQ(gFakeTimeEnv.configureCalls, 0);

  service.tick(1000, true);
  EXPECT_EQ(gFakeTimeEnv.configureCalls, 1);
  EXPECT_FALSE(service.isSynchronized());

  gFakeTimeEnv.seconds = 1705000000;
  gFakeTimeEnv.epochMs = 1705000000456ULL;
  service.tick(3000, true);

  EXPECT_TRUE(service.isSynchronized());
  EXPECT_EQ(service.currentEpochMs(), 1705000000456ULL);
  EXPECT_EQ(gFakeTimeEnv.configureCalls, 1);
}

TEST_F(TimeServiceTest, TickWhileDisconnectedDoesNotStartSync) {
  TimeService service(&kFakeOps);
  service.begin();

  service.tick(1000, false);
  service.tick(3000, false);

  EXPECT_EQ(gFakeTimeEnv.configureCalls, 0);
  EXPECT_FALSE(service.isSynchronized());
}
