#include "board.h"
#include <Wire.h>

static uint8_t pcaAddr = 0;

static bool readReg(uint8_t addr, uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  val = Wire.read();
  return true;
}

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool expanderBegin() {
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  // PCA9557 lives at 0x18-0x1F depending on its address pins. The ES8311 codec is also at 0x18,
  // so skip any address that answers with the ES8311 chip id (reg 0xFD = 0x83).
  // Seengreat's demo uses 0x19; try that first, then scan the rest of the range.
  const uint8_t order[] = {0x19, 0x18, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
  for (uint8_t a : order) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() != 0) continue;
    uint8_t cfg = 0;
    if (!readReg(a, 0x03, cfg)) continue;              // PCA9557 config register
    uint8_t id = 0;
    if (a == 0x18 && readReg(a, 0xFD, id) && id == 0x83) continue;  // that's the ES8311 codec
    pcaAddr = a;
    break;
  }
  if (!pcaAddr) {
    Serial.println("[board] PCA9557 expander not found");
    return false;
  }
  writeReg(pcaAddr, 0x02, 0x00);                       // no input polarity inversion
  writeReg(pcaAddr, 0x01, 1 << PCA_AUDIO_PWR);         // audio power ON
  writeReg(pcaAddr, 0x03, (uint8_t)~(1 << PCA_AUDIO_PWR));  // IO5 output, rest inputs
  Serial.printf("[board] PCA9557 at 0x%02X, audio power on\n", pcaAddr);
  return true;
}

bool expanderFound() { return pcaAddr != 0; }

uint8_t expanderRead() {
  uint8_t v = 0xFF;
  if (pcaAddr) readReg(pcaAddr, 0x00, v);
  return v;
}
