#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <IntervalTimer.h>

// Dual-bus zero-command diagnostic for Teensy 4.1.
// Motor ID 1 is on CAN1; Motor ID 2 is on CAN2.
namespace {
constexpr uint32_t kCanBaud = 1000000;
constexpr uint32_t kControlPeriodUs = 500;       // 2 kHz per motor
constexpr uint32_t kStartupFeedbackTimeoutUs = 500000;
constexpr uint32_t kFeedbackTimeoutUs = 5000;    // 10 missed 500 us frames
constexpr uint32_t kControlOverrunUs = 500;
constexpr uint32_t kHeartbeatPeriodUs = 100000;
constexpr uint16_t kHistoryRows = 4096;          // 2.048 s at 2 kHz

using Can1 = FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>;
using Can2 = FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>;
Can1 can1;
Can2 can2;
IntervalTimer controlTimer;

struct MotorRx {
  volatile uint32_t count = 0;
  volatile uint32_t lastUs = 0;
  volatile uint32_t lastGapUs = 0;
  volatile uint8_t error = 0;
};
MotorRx rx[2];

struct Sample {
  uint32_t timeUs;
  uint32_t tickIntervalUs;
  uint32_t rxAge1Us;
  uint32_t rxAge2Us;
  uint32_t rxGap1Us;
  uint32_t rxGap2Us;
  uint32_t rxCount1;
  uint32_t rxCount2;
  uint32_t can1Esr;
  uint32_t can1Ecr;
  uint32_t can2Esr;
  uint32_t can2Ecr;
  int16_t write1;
  int16_t write2;
  uint8_t motorError1;
  uint8_t motorError2;
  uint16_t txQueue1;
  uint16_t txQueue2;
  uint16_t isrDurationUs;
  uint16_t flags;
};
Sample history[kHistoryRows];
volatile uint16_t historyNext = 0;
volatile uint16_t historyCount = 0;

enum FaultCode : uint8_t {
  kNoFault = 0,
  kTxResult = 1,
  kMotorError = 2,
  kFeedbackStartupTimeout = 3,
  kFeedbackTimeout = 4,
  kCanErrorCounter = 5,
  kCanErrorEvent = 6,
  kRxFrameInvalid = 7,
  kControlOverrun = 8,
  kShutdownTx = 9,
};
volatile bool faultLatched = false;
volatile uint8_t faultCode = kNoFault;
volatile uint8_t faultBus = 0;
volatile int16_t faultWriteResult = 0;
volatile uint32_t faultTimeUs = 0;
volatile uint32_t faultEsr = 0;
volatile uint32_t faultEcr = 0;
volatile uint32_t faultRxAgeUs = UINT32_MAX;
uint32_t startUs = 0;
uint32_t previousTickUs = 0;
uint32_t lastHeartbeatUs = 0;
uint32_t isrMaxUs = 0;
uint32_t isrIntervalMaxUs = 0;
uint32_t txCount[2] = {0, 0};
bool faultReported = false;

void latchFault(uint8_t code, uint8_t bus, int16_t writeResult,
                uint32_t esr, uint32_t ecr, uint32_t rxAgeUs) {
  if (faultLatched) return;
  faultCode = code;
  faultBus = bus;
  faultWriteResult = writeResult;
  faultTimeUs = micros();
  faultEsr = esr;
  faultEcr = ecr;
  faultRxAgeUs = rxAgeUs;
  faultLatched = true;
}

void receiveMotorFrame(uint8_t index, const CAN_message_t &message);
void onCan1(const CAN_message_t &message) { receiveMotorFrame(0, message); }
void onCan2(const CAN_message_t &message) { receiveMotorFrame(1, message); }

void receiveMotorFrame(uint8_t index, const CAN_message_t &message) {
  const uint8_t expectedId = index == 0 ? 1 : 2;
  if (message.flags.remote || message.flags.extended || message.len != 8 ||
      message.buf[0] != expectedId || message.flags.overrun) {
    const uint32_t esr = index == 0 ? FLEXCANb_ESR1(CAN1) : FLEXCANb_ESR1(CAN2);
    const uint32_t ecr = index == 0 ? FLEXCANb_ECR(CAN1) : FLEXCANb_ECR(CAN2);
    latchFault(kRxFrameInvalid, expectedId, 0, esr, ecr, rxAgeUs(index, micros()));
    return;
  }

  const uint32_t now = micros();
  const uint32_t previous = rx[index].lastUs;
  if (previous) rx[index].lastGapUs = now - previous;
  rx[index].lastUs = now;
  ++rx[index].count;
  rx[index].error = message.buf[7];
  if (message.buf[7] != 0) {
    const uint32_t esr = index == 0 ? FLEXCANb_ESR1(CAN1) : FLEXCANb_ESR1(CAN2);
    const uint32_t ecr = index == 0 ? FLEXCANb_ECR(CAN1) : FLEXCANb_ECR(CAN2);
    latchFault(kMotorError, expectedId, 0, esr, ecr, 0);
  }
}

void configureBus(Can1 &bus) {
  bus.begin();
  bus.setBaudRate(kCanBaud);
  bus.setMaxMB(16);
  bus.setMB(MB0, RX, STD);
  bus.setMBFilter(REJECT_ALL);
  bus.setMBFilter(MB0, ACCEPT_ALL);
}
void configureBus(Can2 &bus) {
  bus.begin();
  bus.setBaudRate(kCanBaud);
  bus.setMaxMB(16);
  bus.setMB(MB0, RX, STD);
  bus.setMBFilter(REJECT_ALL);
  bus.setMBFilter(MB0, ACCEPT_ALL);
}

CAN_message_t zeroTorqueFrame(uint8_t id) {
  CAN_message_t message = {};
  message.id = id;
  message.len = 8;
  // MIT format: position=0, velocity=0, kp=0, kd=0, torque=0.
  message.buf[0] = 0x80;
  message.buf[1] = 0x00;
  message.buf[2] = 0x80;
  message.buf[3] = 0x00;
  message.buf[4] = 0x00;
  message.buf[5] = 0x00;
  message.buf[6] = 0x08;
  message.buf[7] = 0x00;
  return message;
}

CAN_message_t specialFrame(uint8_t id, uint8_t command) {
  CAN_message_t message = {};
  message.id = id;
  message.len = 8;
  for (uint8_t i = 0; i < 7; ++i) message.buf[i] = 0xff;
  message.buf[7] = command;
  return message;
}

uint32_t rxAgeUs(uint8_t index, uint32_t now) {
  const uint32_t last = rx[index].lastUs;
  return last ? now - last : UINT32_MAX;
}

void controlISR() {
  const uint32_t enteredUs = micros();
  if (faultLatched) return;

  const uint32_t tickInterval = previousTickUs ? enteredUs - previousTickUs : 0;
  previousTickUs = enteredUs;
  if (tickInterval) isrIntervalMaxUs = max(isrIntervalMaxUs, tickInterval);

  const uint32_t age1 = rxAgeUs(0, enteredUs);
  const uint32_t age2 = rxAgeUs(1, enteredUs);
  const uint32_t esr1 = FLEXCANb_ESR1(CAN1);
  const uint32_t ecr1 = FLEXCANb_ECR(CAN1);
  const uint32_t esr2 = FLEXCANb_ESR1(CAN2);
  const uint32_t ecr2 = FLEXCANb_ECR(CAN2);

  if (!rx[0].count && uint32_t(enteredUs - startUs) > kStartupFeedbackTimeoutUs)
    latchFault(kFeedbackStartupTimeout, 1, 0, esr1, ecr1, age1);
  else if (rx[0].count && age1 > kFeedbackTimeoutUs)
    latchFault(kFeedbackTimeout, 1, 0, esr1, ecr1, age1);
  if (!rx[1].count && uint32_t(enteredUs - startUs) > kStartupFeedbackTimeoutUs)
    latchFault(kFeedbackStartupTimeout, 2, 0, esr2, ecr2, age2);
  else if (rx[1].count && age2 > kFeedbackTimeoutUs)
    latchFault(kFeedbackTimeout, 2, 0, esr2, ecr2, age2);
  if (ecr1 != 0) latchFault(kCanErrorCounter, 1, 0, esr1, ecr1, age1);
  if (ecr2 != 0) latchFault(kCanErrorCounter, 2, 0, esr2, ecr2, age2);
  if (faultLatched) return;

  // Attempt both buses on every 500 us tick. CAN1 and CAN2 transmit in parallel
  // at the hardware level despite these two API calls being sequential.
  const int result1 = can1.write(zeroTorqueFrame(1));
  const int result2 = can2.write(zeroTorqueFrame(2));
  ++txCount[0];
  ++txCount[1];

  const uint32_t afterUs = micros();
  const uint32_t duration = afterUs - enteredUs;
  isrMaxUs = max(isrMaxUs, duration);
  const uint16_t queue1 = uint16_t(can1.getTXQueueCount());
  const uint16_t queue2 = uint16_t(can2.getTXQueueCount());
  const uint16_t row = historyNext;
  Sample &sample = history[row];
  sample.timeUs = enteredUs - startUs;
  sample.tickIntervalUs = tickInterval;
  sample.rxAge1Us = age1;
  sample.rxAge2Us = age2;
  sample.rxGap1Us = rx[0].lastGapUs;
  sample.rxGap2Us = rx[1].lastGapUs;
  sample.rxCount1 = rx[0].count;
  sample.rxCount2 = rx[1].count;
  sample.can1Esr = esr1;
  sample.can1Ecr = ecr1;
  sample.can2Esr = esr2;
  sample.can2Ecr = ecr2;
  sample.write1 = int16_t(result1);
  sample.write2 = int16_t(result2);
  sample.motorError1 = rx[0].error;
  sample.motorError2 = rx[1].error;
  sample.txQueue1 = queue1;
  sample.txQueue2 = queue2;
  sample.isrDurationUs = uint16_t(min(duration, uint32_t(UINT16_MAX)));
  sample.flags = 0;
  historyNext = uint16_t((row + 1) % kHistoryRows);
  if (historyCount < kHistoryRows) ++historyCount;

  if (result1 != 1) latchFault(kTxResult, 1, int16_t(result1), esr1, ecr1, age1);
  if (result2 != 1) latchFault(kTxResult, 2, int16_t(result2), esr2, ecr2, age2);
  if (duration >= kControlOverrunUs)
    latchFault(kControlOverrun, 0, 0, esr1, ecr1, 0);
}

const char *faultName(uint8_t code) {
  switch (code) {
    case kTxResult: return "TX_RESULT_NOT_DIRECT";
    case kMotorError: return "MOTOR_ERROR_BYTE";
    case kFeedbackStartupTimeout: return "NO_STARTUP_FEEDBACK";
    case kFeedbackTimeout: return "FEEDBACK_TIMEOUT";
    case kCanErrorCounter: return "CAN_ECR_NONZERO";
    case kCanErrorEvent: return "CAN_ERROR_EVENT";
    case kRxFrameInvalid: return "RX_FRAME_INVALID_OR_OVERRUN";
    case kControlOverrun: return "CONTROL_ISR_OVERRUN";
    case kShutdownTx: return "SHUTDOWN_TX_RESULT";
    default: return "UNKNOWN";
  }
}

void printHistory() {
  Serial.println(F("HISTORY_COLUMNS,time_us,tick_interval_us,rx_age_m1_us,rx_age_m2_us,rx_gap_m1_us,rx_gap_m2_us,rx_count_m1,rx_count_m2,write_result_m1,write_result_m2,tx_queue_m1,tx_queue_m2,motor_error_m1,motor_error_m2,can1_esr,can1_ecr,can2_esr,can2_ecr,isr_duration_us"));
  const uint16_t count = historyCount;
  const uint16_t first = historyCount == kHistoryRows ? historyNext : 0;
  Serial.print(F("HISTORY_BEGIN,")); Serial.println(count);
  for (uint16_t n = 0; n < count; ++n) {
    const Sample &s = history[(first + n) % kHistoryRows];
    Serial.print(s.timeUs); Serial.write(',');
    Serial.print(s.tickIntervalUs); Serial.write(',');
    Serial.print(s.rxAge1Us); Serial.write(',');
    Serial.print(s.rxAge2Us); Serial.write(',');
    Serial.print(s.rxGap1Us); Serial.write(',');
    Serial.print(s.rxGap2Us); Serial.write(',');
    Serial.print(s.rxCount1); Serial.write(',');
    Serial.print(s.rxCount2); Serial.write(',');
    Serial.print(s.write1); Serial.write(',');
    Serial.print(s.write2); Serial.write(',');
    Serial.print(s.txQueue1); Serial.write(',');
    Serial.print(s.txQueue2); Serial.write(',');
    Serial.print(s.motorError1); Serial.write(',');
    Serial.print(s.motorError2); Serial.write(',');
    Serial.print(s.can1Esr, HEX); Serial.write(',');
    Serial.print(s.can1Ecr, HEX); Serial.write(',');
    Serial.print(s.can2Esr, HEX); Serial.write(',');
    Serial.print(s.can2Ecr, HEX); Serial.write(',');
    Serial.println(s.isrDurationUs);
  }
  Serial.println(F("HISTORY_END"));
}

bool isFlexCanError(const CAN_error_t &error) {
  return error.BIT1_ERR || error.BIT0_ERR || error.ACK_ERR || error.CRC_ERR ||
         error.FRM_ERR || error.STF_ERR || error.TX_WRN || error.RX_WRN ||
         error.TX_ERR_COUNTER || error.RX_ERR_COUNTER ||
         (error.ESR1 & 0x30U) == 0x20U;
}

void inspectErrorQueue() {
  CAN_error_t error;
  for (uint8_t i = 0; i < 8 && can1.error(error, false); ++i) {
    if (isFlexCanError(error))
      latchFault(kCanErrorEvent, 1, 0, error.ESR1, error.ECR, rxAgeUs(0, micros()));
  }
  for (uint8_t i = 0; i < 8 && can2.error(error, false); ++i) {
    if (isFlexCanError(error))
      latchFault(kCanErrorEvent, 2, 0, error.ESR1, error.ECR, rxAgeUs(1, micros()));
  }
}

void printHeartbeat(uint32_t now) {
  Serial.print(F("LIVE,")); Serial.print(now - startUs);
  Serial.print(F(",TX_M1,")); Serial.print(txCount[0]);
  Serial.print(F(",TX_M2,")); Serial.print(txCount[1]);
  Serial.print(F(",RX_M1,")); Serial.print(rx[0].count);
  Serial.print(F(",RX_AGE_M1_us,")); Serial.print(rxAgeUs(0, now));
  Serial.print(F(",RX_GAP_M1_us,")); Serial.print(rx[0].lastGapUs);
  Serial.print(F(",MOTOR_ERROR_M1,")); Serial.print(rx[0].error);
  Serial.print(F(",RX_M2,")); Serial.print(rx[1].count);
  Serial.print(F(",RX_AGE_M2_us,")); Serial.print(rxAgeUs(1, now));
  Serial.print(F(",RX_GAP_M2_us,")); Serial.print(rx[1].lastGapUs);
  Serial.print(F(",MOTOR_ERROR_M2,")); Serial.print(rx[1].error);
  Serial.print(F(",CAN1_ESR,")); Serial.print(FLEXCANb_ESR1(CAN1), HEX);
  Serial.print(F(",CAN1_ECR,")); Serial.print(FLEXCANb_ECR(CAN1), HEX);
  Serial.print(F(",CAN2_ESR,")); Serial.print(FLEXCANb_ESR1(CAN2), HEX);
  Serial.print(F(",CAN2_ECR,")); Serial.print(FLEXCANb_ECR(CAN2), HEX);
  Serial.print(F(",TX_QUEUE_M1,")); Serial.print(can1.getTXQueueCount());
  Serial.print(F(",TX_QUEUE_M2,")); Serial.print(can2.getTXQueueCount());
  Serial.print(F(",ISR_MAX_us,")); Serial.print(isrMaxUs);
  Serial.print(F(",ISR_INTERVAL_MAX_us,")); Serial.println(isrIntervalMaxUs);
}

void reportAndHalt() {
  if (faultReported) return;
  faultReported = true;
  controlTimer.end();

  // Best-effort zero and disable after the first fault. Raw API results are
  // logged; the latched diagnostic run never restarts automatically.
  const int zero1 = can1.write(zeroTorqueFrame(1));
  const int zero2 = can2.write(zeroTorqueFrame(2));
  const int disable1 = can1.write(specialFrame(1, 0xfd));
  const int disable2 = can2.write(specialFrame(2, 0xfd));

  Serial.print(F("FAULT,")); Serial.print(faultName(faultCode));
  Serial.print(F(",BUS,")); Serial.print(faultBus);
  Serial.print(F(",WRITE_RESULT,")); Serial.print(faultWriteResult);
  Serial.print(F(",TIME_us,")); Serial.print(faultTimeUs - startUs);
  Serial.print(F(",ESR,")); Serial.print(faultEsr, HEX);
  Serial.print(F(",ECR,")); Serial.print(faultEcr, HEX);
  Serial.print(F(",RX_AGE_us,")); Serial.print(faultRxAgeUs);
  Serial.print(F(",ZERO_RESULT_M1,")); Serial.print(zero1);
  Serial.print(F(",ZERO_RESULT_M2,")); Serial.print(zero2);
  Serial.print(F(",DISABLE_RESULT_M1,")); Serial.print(disable1);
  Serial.print(F(",DISABLE_RESULT_M2,")); Serial.println(disable2);
  Serial.print(F("FINAL,LIVE_TIME_us,")); Serial.print(micros() - startUs);
  Serial.print(F(",TX_M1,")); Serial.print(txCount[0]);
  Serial.print(F(",TX_M2,")); Serial.print(txCount[1]);
  Serial.print(F(",RX_M1,")); Serial.print(rx[0].count);
  Serial.print(F(",RX_M2,")); Serial.print(rx[1].count);
  Serial.print(F(",CAN1_ESR,")); Serial.print(FLEXCANb_ESR1(CAN1), HEX);
  Serial.print(F(",CAN1_ECR,")); Serial.print(FLEXCANb_ECR(CAN1), HEX);
  Serial.print(F(",CAN2_ESR,")); Serial.print(FLEXCANb_ESR1(CAN2), HEX);
  Serial.print(F(",CAN2_ECR,")); Serial.print(FLEXCANb_ECR(CAN2), HEX);
  Serial.print(F(",ISR_MAX_us,")); Serial.print(isrMaxUs);
  Serial.print(F(",ISR_INTERVAL_MAX_us,")); Serial.println(isrIntervalMaxUs);
  printHistory();
  Serial.println(F("HALTED,RESET_REQUIRED"));
  Serial.flush();
}
} // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!Serial && uint32_t(millis() - serialWaitStart) < 3000) {}

  configureBus(can1);
  configureBus(can2);
  can1.onReceive(MB0, onCan1);
  can2.onReceive(MB0, onCan2);
  can1.enableMBInterrupt(MB0);
  can2.enableMBInterrupt(MB0);

  Serial.println(F("READY,DUAL_ZERO_DIAGNOSTIC,PERIOD_us,500,CAN_BAUD,1000000,M1,ID1/CAN1,M2,ID2/CAN2"));
  Serial.println(F("WRITE_RESULT,1=MAILBOX,-1=QUEUED,0=REJECTED,-2=BUS_OFF; anything other than 1 halts"));

  startUs = micros();
  // Enable both motors once; zero torque is then sent on every 500 us tick.
  const int enable1 = can1.write(specialFrame(1, 0xfc));
  const int enable2 = can2.write(specialFrame(2, 0xfc));
  if (enable1 != 1) latchFault(kTxResult, 1, int16_t(enable1), FLEXCANb_ESR1(CAN1), FLEXCANb_ECR(CAN1), UINT32_MAX);
  if (enable2 != 1) latchFault(kTxResult, 2, int16_t(enable2), FLEXCANb_ESR1(CAN2), FLEXCANb_ECR(CAN2), UINT32_MAX);

  controlTimer.priority(192);
  if (!faultLatched && !controlTimer.begin(controlISR, kControlPeriodUs))
    latchFault(kControlOverrun, 0, 0, FLEXCANb_ESR1(CAN1), FLEXCANb_ECR(CAN1), 0);
}

void loop() {
  inspectErrorQueue();
  if (faultLatched) {
    reportAndHalt();
    while (true) delay(1000);
  }

  const uint32_t now = micros();
  if (uint32_t(now - lastHeartbeatUs) >= kHeartbeatPeriodUs) {
    lastHeartbeatUs = now;
    printHeartbeat(now);
  }
}
