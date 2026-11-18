#include <Arduino.h>
#include <IntervalTimer.h>
#include <FlexCAN_T4.h>

// Offline timing validation sketch for Teensy 4.1.
// MODE_TIMER_ONLY: no CAN initialization or access.
// MODE_PASSIVE_RX: initialize CAN1/CAN2 as listen-only and count received frames.
#define MODE_TIMER_ONLY 1

#ifndef TIMER_BASE_PERIOD_US
#define TIMER_BASE_PERIOD_US 50
#endif
constexpr uint32_t VIRTUAL_PERIOD_US = 500;
constexpr uint32_t RX_POLL_PERIOD_US = 100;
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t REPORT_INTERVAL_MS = 1000;

static_assert(VIRTUAL_PERIOD_US % TIMER_BASE_PERIOD_US == 0, "500 us period must align to ISR base");
static_assert(RX_POLL_PERIOD_US % TIMER_BASE_PERIOD_US == 0, "RX poll must align to ISR base");
static_assert(TIMER_BASE_PERIOD_US == 50 || TIMER_BASE_PERIOD_US == 100,
              "Select a 50 us or 100 us ISR base");

using Can1Type = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2Type = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
static Can1Type can1;
static Can2Type can2;
static IntervalTimer timer;

struct Counters {
  volatile uint32_t tickCount = 0;
  volatile uint32_t virtualEvents = 0;
  volatile uint32_t lastTickUs = 0;
  volatile uint32_t tickIntervalUs = 0;
  volatile uint32_t maxAbsJitterUs = 0;
  volatile uint32_t virtualIntervalUs = 0;
  volatile uint32_t maxAbsVirtualJitterUs = 0;
  volatile uint32_t virtualJitterViolations = 0;
  volatile uint32_t missedVirtualEst = 0;
  volatile uint32_t maxIsrDurationUs = 0;
  volatile uint32_t isrOverruns = 0;
  volatile uint32_t missedDeadlines = 0;
  volatile uint32_t rxPollCount = 0;
  volatile uint32_t maxRxPollDurationUs = 0;
  volatile uint32_t rxRawCan1 = 0;
  volatile uint32_t rxRawCan2 = 0;
  volatile uint32_t canTxCount = 0; // Must remain zero; this sketch has no CAN write calls.
};
static Counters counters;
static uint32_t previousReportMs = 0;
static uint32_t previousTickUs = 0;
static uint32_t previousVirtualEventUs = 0;
#if !MODE_TIMER_ONLY
static uint32_t rxPollDivider = 0;
#endif
static uint32_t virtualDivider = 0;

static void timerISR() {
  const uint32_t isrStart = micros();
  const uint32_t now = isrStart;
  uint32_t interval = 0;
  if (previousTickUs != 0) {
    interval = now - previousTickUs;
    const int32_t jitter = (int32_t)interval - (int32_t)TIMER_BASE_PERIOD_US;
    const uint32_t absJitter = (uint32_t)(jitter < 0 ? -jitter : jitter);
    if (absJitter > counters.maxAbsJitterUs) counters.maxAbsJitterUs = absJitter;
    if (interval > TIMER_BASE_PERIOD_US + TIMER_BASE_PERIOD_US / 2) {
      counters.missedDeadlines += ((interval + TIMER_BASE_PERIOD_US / 2) / TIMER_BASE_PERIOD_US) - 1;
    }
  }
  previousTickUs = now;
  counters.lastTickUs = now;
  counters.tickIntervalUs = interval;
  counters.tickCount++;

  if (++virtualDivider >= VIRTUAL_PERIOD_US / TIMER_BASE_PERIOD_US) {
    virtualDivider = 0;
    uint32_t virtualInterval = 0;
    if (previousVirtualEventUs != 0) {
      virtualInterval = now - previousVirtualEventUs;
      const int32_t virtualJitter = (int32_t)virtualInterval - (int32_t)VIRTUAL_PERIOD_US;
      const uint32_t absVirtualJitter = (uint32_t)(virtualJitter < 0 ? -virtualJitter : virtualJitter);
      if (absVirtualJitter > counters.maxAbsVirtualJitterUs) {
        counters.maxAbsVirtualJitterUs = absVirtualJitter;
      }
      if (absVirtualJitter > 50) counters.virtualJitterViolations++;
      if (virtualInterval > VIRTUAL_PERIOD_US + VIRTUAL_PERIOD_US / 2) {
        counters.missedVirtualEst += ((virtualInterval + VIRTUAL_PERIOD_US / 2) / VIRTUAL_PERIOD_US) - 1;
      }
    }
    previousVirtualEventUs = now;
    counters.virtualIntervalUs = virtualInterval;
    counters.virtualEvents++;
  }

#if !MODE_TIMER_ONLY
  if (++rxPollDivider >= RX_POLL_PERIOD_US / TIMER_BASE_PERIOD_US) {
    rxPollDivider = 0;
    const uint32_t pollStart = micros();
    CAN_message_t frame;
    while (can1.read(frame)) counters.rxRawCan1++;
    while (can2.read(frame)) counters.rxRawCan2++;
    const uint32_t duration = micros() - pollStart;
    counters.rxPollCount++;
    if (duration > counters.maxRxPollDurationUs) counters.maxRxPollDurationUs = duration;
  }
#endif

  const uint32_t duration = micros() - isrStart;
  if (duration > counters.maxIsrDurationUs) counters.maxIsrDurationUs = duration;
  if (duration >= TIMER_BASE_PERIOD_US) counters.isrOverruns++;
}

static void printStatistics() {
  noInterrupts();
  const uint32_t ticks = counters.tickCount;
  const uint32_t events = counters.virtualEvents;
  const uint32_t interval = counters.tickIntervalUs;
  const uint32_t jitter = counters.maxAbsJitterUs;
  const uint32_t virtualInterval = counters.virtualIntervalUs;
  const uint32_t virtualJitter = counters.maxAbsVirtualJitterUs;
  const uint32_t virtualJitterViolations = counters.virtualJitterViolations;
  const uint32_t missedVirtual = counters.missedVirtualEst;
  const uint32_t isrUs = counters.maxIsrDurationUs;
  const uint32_t overruns = counters.isrOverruns;
  const uint32_t missed = counters.missedDeadlines;
  const uint32_t rxPolls = counters.rxPollCount;
  const uint32_t rxUs = counters.maxRxPollDurationUs;
  const uint32_t rx1 = counters.rxRawCan1;
  const uint32_t rx2 = counters.rxRawCan2;
  const uint32_t tx = counters.canTxCount;
  interrupts();

  Serial.print("TIMER_BASE_US="); Serial.print(TIMER_BASE_PERIOD_US);
  Serial.print(",TICK_COUNT="); Serial.print(ticks);
  Serial.print(",TICK_INTERVAL_US="); Serial.print(interval);
  Serial.print(",VIRTUAL_EVENTS="); Serial.print(events);
  Serial.print(",VIRTUAL_INTERVAL_US="); Serial.print(virtualInterval);
  Serial.print(",VIRTUAL_JITTER_US="); Serial.print(virtualJitter);
  Serial.print(",VIRTUAL_JITTER_VIOLATIONS="); Serial.print(virtualJitterViolations);
  Serial.print(",MISSED_VIRTUAL_EST="); Serial.print(missedVirtual);
  Serial.print(",JITTER_US="); Serial.print(jitter);
  Serial.print(",ISR_DURATION_US="); Serial.print(isrUs);
  Serial.print(",ISR_OVERRUNS="); Serial.print(overruns);
  Serial.print(",MISSED_DEADLINES="); Serial.print(missed);
  Serial.print(",RX_POLL_COUNT="); Serial.print(rxPolls);
  Serial.print(",RX_POLL_DURATION_US="); Serial.print(rxUs);
  Serial.print(",RX_RAW_CAN1="); Serial.print(rx1);
  Serial.print(",RX_RAW_CAN2="); Serial.print(rx2);
  Serial.print(",CAN_TX_COUNT="); Serial.println(tx);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
#if !MODE_TIMER_ONLY
  can1.begin();
  can1.setBaudRate(1000000, LISTEN_ONLY);
  can2.begin();
  can2.setBaudRate(1000000, LISTEN_ONLY);
#endif
  previousReportMs = millis();
  timer.begin(timerISR, TIMER_BASE_PERIOD_US);
}

void loop() {
  const uint32_t now = millis();
  if ((uint32_t)(now - previousReportMs) >= REPORT_INTERVAL_MS) {
    previousReportMs += REPORT_INTERVAL_MS;
    printStatistics();
  }
}
