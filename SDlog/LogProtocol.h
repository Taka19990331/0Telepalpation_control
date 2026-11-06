#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// Shared stop-and-wait serial transport. The payload is the exact SD file;
// interpretation and CSV conversion stay keyed by the schema identifier.
namespace SDlog {
constexpr uint16_t BLOCK_BYTES = 4096;
constexpr uint8_t MAX_RETRIES = 25;
constexpr uint32_t ACK_TIMEOUT_MS = 30000;

using FetchHandler = void (*)(bool previous);

// Intercept line-oriented common commands in older sketches that otherwise
// consume single-character commands directly (s/x/d). Returns false for
// ordinary input so the sketch can preserve its original control keys.
inline bool consumeCommandByte(char c, const char *firmware, const char *schema,
                               FetchHandler fetch) {
  static char line[64];
  static uint8_t length = 0;
  static bool discarding = false;
  if (!length && !discarding && c != 'L') return false;
  if (c == '\r' || c == '\n') {
    if (!discarding) line[length] = 0;
    if (!discarding && !strcmp(line, "LOG,1,IDENTIFY")) {
      Serial.print("LOG,1,IDENTIFY,"); Serial.print(firmware); Serial.print(','); Serial.println(schema);
    } else if (!discarding && !strcmp(line, "LOG,1,FETCH,LATEST")) {
      if (fetch) fetch(false);
    } else if (!discarding && !strcmp(line, "LOG,1,FETCH,PREVIOUS")) {
      if (fetch) fetch(true);
      else Serial.println("LOG,1,ERROR,PREVIOUS_UNSUPPORTED");
    }
    length = 0; discarding = false;
    return true;
  }
  if (discarding) return true;
  if (length < sizeof(line)-1) line[length++] = c;
  else { length = 0; discarding = true; }
  return true;
}

inline uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length) {
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}

template <typename FileT>
uint32_t crc32File(FileT &file) {
  uint32_t crc = 0xffffffffu;
  uint8_t buffer[512];
  file.seekSet(0);
  for (;;) {
    int count = file.read(buffer, sizeof(buffer));
    if (count <= 0) break;
    crc = crc32Update(crc, buffer, size_t(count));
  }
  file.seekSet(0);
  return ~crc;
}

template <typename FileT>
bool sendFile(FileT &file, const char *firmware, const char *schema,
              const char *filename, uint32_t epoch = 0) {
  Serial.println("LOG,1,PROGRESS,CRC_BEGIN"); Serial.flush();
  const uint64_t size = uint64_t(file.fileSize());
  const uint32_t wholeCrc = crc32File(file);
  Serial.println("LOG,1,PROGRESS,CRC_READY"); Serial.flush();
  Serial.print("LOG,1,FILE,"); Serial.print(firmware); Serial.print(',');
  Serial.print(schema); Serial.print(','); Serial.print(filename); Serial.print(',');
  Serial.print((unsigned long long)size); Serial.print(','); Serial.print(wholeCrc, HEX); Serial.print(',');
  Serial.println(epoch); Serial.flush();

  uint8_t buffer[BLOCK_BYTES];
  uint64_t offset = 0;
  while (offset < size) {
    const uint16_t length = uint16_t((size - offset) < sizeof(buffer) ? size - offset : sizeof(buffer));
    uint16_t readCount = 0;
    while (readCount < length) {
      int n = file.read(buffer + readCount, length - readCount);
      if (n <= 0) { Serial.println("LOG,1,ERROR,SD_READ"); return false; }
      readCount += uint16_t(n);
    }
    const uint32_t blockCrc = ~crc32Update(0xffffffffu, buffer, length);
    bool accepted = false;
    for (uint8_t attempt = 0; attempt < MAX_RETRIES && !accepted; ++attempt) {
      Serial.print("LOG,1,BLOCK,"); Serial.print((unsigned long long)offset); Serial.print(',');
      Serial.print(length); Serial.print(','); Serial.println(blockCrc, HEX); Serial.flush();
      size_t sent = 0;
      while (sent < length) {
        size_t n = Serial.write(buffer + sent, length - sent);
        if (!n) { yield(); continue; }
        sent += n;
      }
      const uint32_t started = millis();
      while (uint32_t(millis() - started) < ACK_TIMEOUT_MS) {
        if (Serial.available()) {
          const int response = Serial.read();
          if (response == '+') { accepted = true; break; }
          if (response == '-') break;
        }
        yield();
      }
    }
    if (!accepted) { Serial.println("LOG,1,ERROR,ACK_TIMEOUT"); return false; }
    offset += length;
  }
  Serial.print("LOG,1,END,"); Serial.print((unsigned long long)size); Serial.print(',');
  Serial.println(wholeCrc, HEX); Serial.flush();
  return true;
}
}
