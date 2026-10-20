#pragma once
#include "chirp_config.h"

// Each fresh CAN response is counted once, not repeatedly at the control rate.
struct ReferenceAverage {
  double sumRad = 0;
  uint32_t count = 0, lastSequence = 0;
  float minRad = 0, maxRad = 0;
  constexpr void reset() { sumRad = 0; count = lastSequence = 0; minRad = maxRad = 0; }
  constexpr bool observe(float positionRad, uint32_t sequence) {
    if (sequence == lastSequence || !sequence) return true;
    if (!__builtin_isfinite(positionRad) || count == UINT32_MAX) return false;
    if (!count) minRad = maxRad = positionRad;
    if (positionRad < minRad) minRad = positionRad;
    if (positionRad > maxRad) maxRad = positionRad;
    sumRad += positionRad; ++count; lastSequence = sequence;
    return __builtin_isfinite(sumRad);
  }
  constexpr bool valid() const { return count >= MIN_REFERENCE_SAMPLES && __builtin_isfinite(sumRad); }
  constexpr float meanRad() const { return count ? float(sumRad/count) : 0; }
};
