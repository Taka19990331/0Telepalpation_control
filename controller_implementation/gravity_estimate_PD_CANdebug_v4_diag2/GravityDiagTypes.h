#pragma once

struct Feedback {
  float angleRad = 0.0f;
  float velocityRadS = 0.0f;
  float torqueNm = 0.0f;
  uint32_t rxUs = 0;
  uint8_t error = 0;
  bool valid = false;
};

struct Sample {
  uint32_t timeUs;
  float angleRad[2];
  float velocityRadS[2];
  float commandNm[2];
  float feedbackNm[2];
};

enum StartupCause : uint8_t {
  START_OK = 0, START_ENABLE_TX_FAIL, START_ZERO_TX_FAIL,
  START_RX_TIMEOUT, START_MOTOR_ERROR, START_INVALID_ANGLE,
  START_SD_HEADER_FAIL, START_INTERRUPTED, START_TIMER_FAIL
};

struct StartupSnapshot {
  StartupCause cause;
  uint32_t elapsedMs, snapshotUs;
  Feedback f[2];
  uint32_t raw[2], accepted[2], rejected[2], overrun[2];
  uint32_t txOk[2], txFail[2], rejectId[2], txQueue[2];
  uint32_t ecr[2], esr[2];
  int16_t txLast[2];
  uint8_t rejectPayload[2], rejectDlc[2];
};
