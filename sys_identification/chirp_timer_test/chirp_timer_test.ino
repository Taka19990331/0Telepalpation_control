#include <Arduino.h>
#include <IntervalTimer.h>
#include <math.h>

// Based on AK80_timer_test: timer ISR -> RAM -> foreground Serial report.
// Teensy 4.1 only. No CAN initialization or motor commands.
#if !defined(ARDUINO_TEENSY41)
#error "Select Teensy 4.1 (RAM2 is required for the complete log)."
#endif
constexpr uint32_t PERIOD_US = 500;
constexpr uint32_t SAMPLE_HZ = 2000;
constexpr uint32_t DURATION_S = 20;
constexpr uint32_t CHIRP_STEPS = SAMPLE_HZ * DURATION_S;
constexpr uint32_t CAPACITY = CHIRP_STEPS + 1;
constexpr double F0 = 1.0, F1 = 100.0;
constexpr double AMP0 = 1.0, AMP1 = 2.5;
constexpr double CHIRP_TWO_PI = 6.283185307179586476925286766559;
const double CHIRP_K = log(F1 / F0) / DURATION_S;

struct Sample {
  uint32_t actualUs;
  float torqueNm;
  uint32_t executionUs;
};
static_assert(sizeof(Sample) == 12, "Check RAM budget");
// 480012 bytes in RAM2; do not put this array on the stack or in RAM1.
DMAMEM Sample samples[CAPACITY];
IntervalTimer controlTimer;
volatile uint32_t sampleCount = 0;
volatile uint8_t requestedCommand = 0;
volatile bool active = false, reportReady = false;
bool reporting = false;
uint32_t firstTickUs = 0;
const char *stopReason = "COMPLETE";

float calculateChirp(double t) {
  if (t < 0.0 || t >= DURATION_S) return 0.0f;
  const double growth = expm1(CHIRP_K * t);
  const double frequency = F0 * (1.0 + growth);
  const double amplitude = AMP0 + (AMP1 - AMP0) *
      (frequency - F0) / (F1 - F0);
  const double phase = CHIRP_TWO_PI * F0 * growth / CHIRP_K;
  return static_cast<float>(amplitude * sin(phase));
}

void controlISR() {
  const uint32_t entryUs = micros();
  const uint8_t command = requestedCommand;
  requestedCommand = 0;
  if (command == 1 && !active && !reportReady) {
    sampleCount = 0;
    firstTickUs = entryUs;
    stopReason = "COMPLETE";
    active = true;
  }
  if (!active) return;
  const uint32_t step = sampleCount;
  // Each run starts at t=0 on its first ISR; actualUs measures ISR jitter.
  Sample &row = samples[step];
  row.actualUs = entryUs - firstTickUs;
  const bool stop = command == 2 || step == CHIRP_STEPS;
  row.torqueNm = stop ? 0.0f : calculateChirp(step * (PERIOD_US * 1e-6));
  if (stop) {
    stopReason = command == 2 ? "USER" : "COMPLETE";
    active = false;
  }
  row.executionUs = micros() - entryUs;
  sampleCount = step + 1;
  // Publish only after the final row is complete. No Serial in this ISR.
  if (stop) reportReady = true;
}

void printReport() {
  const uint32_t count = sampleCount;
  uint32_t minPeriod = UINT32_MAX, maxPeriod = 0, maxISR = 0;
  uint32_t longIntervals = 0, slowISRs = 0;
  Serial.print("CHIRP_BEGIN,VERSION,1,REASON,"); Serial.println(stopReason);
  Serial.println("CONFIG,Hz,2000,Duration_s,20,F0_Hz,1,F1_Hz,100,A0_Nm,1,A1_Nm,2.5");
  Serial.print("RAM_BYTES,"); Serial.println(sizeof(samples));
  Serial.println("DATA,index,nominal_us,actual_us,period_us,torque_Nm,ISR_us");
  for (uint32_t i = 0; i < count; ++i) {
    const Sample &row = samples[i];
    const uint32_t period = i ? row.actualUs - samples[i-1].actualUs : 0;
    if (i) {
      if (period < minPeriod) minPeriod = period;
      if (period > maxPeriod) maxPeriod = period;
      if (period >= 2 * PERIOD_US) ++longIntervals;
    }
    if (row.executionUs > maxISR) maxISR = row.executionUs;
    if (row.executionUs >= PERIOD_US) ++slowISRs;
    Serial.print("DATA,"); Serial.print(i);
    Serial.print(','); Serial.print(i * PERIOD_US);
    Serial.print(','); Serial.print(row.actualUs);
    Serial.print(','); Serial.print(period);
    Serial.print(','); Serial.print(row.torqueNm, 7);
    Serial.print(','); Serial.println(row.executionUs);
  }
  Serial.print("SUMMARY,Count,"); Serial.print(count);
  Serial.print(",PeriodMin_us,"); Serial.print(count > 1 ? minPeriod : 0);
  Serial.print(",PeriodMax_us,"); Serial.print(maxPeriod);
  Serial.print(",ISRMax_us,"); Serial.print(maxISR);
  Serial.print(",Intervals_ge_1000us,"); Serial.print(longIntervals);
  Serial.print(",ISR_ge_500us,"); Serial.println(slowISRs);
  Serial.println("CHIRP_END");
  Serial.println("READY: s=start, x=stop");
}

void setup() {
  Serial.begin(115200);
  const uint32_t waitMs = millis();
  while (!Serial && millis() - waitMs < 5000) {}
  Serial.println("CHIRP_TIMER_TEST_V1,NO_MOTOR,TEENSY41");
  Serial.println("READY: s=start 20 s chirp, x=stop; output follows completion");
  controlTimer.priority(192);
  if (!controlTimer.begin(controlISR, PERIOD_US)) {
    Serial.println("ERROR,TIMER_BEGIN_FAILED");
    while (true) yield();
  }
}

void loop() {
  for (uint8_t i = 0; i < 32 && Serial.available(); ++i) {
    const char c = Serial.read();
    noInterrupts();
    if (!reportReady && !reporting) {
      if ((c == 'x' || c == 'X') && active) requestedCommand = 2;
      else if ((c == 's' || c == 'S') && !active && requestedCommand != 2)
        requestedCommand = 1;
    }
    interrupts();
  }
  noInterrupts();
  const bool dump = reportReady;
  if (dump) {
    reporting = true;
    requestedCommand = 0;
    controlTimer.end();
    reportReady = false;
  }
  interrupts();
  if (dump) {
    printReport();
    // Discard commands typed during the dump; the next run needs a new 's'.
    while (Serial.available()) Serial.read();
    reporting = false;
    if (!controlTimer.begin(controlISR, PERIOD_US)) {
      Serial.println("ERROR,TIMER_RESTART_FAILED");
      while (true) yield();
    }
  }
}
