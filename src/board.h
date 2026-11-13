// board.h - Seengreat "RGB Matrix HUB75 S3" (ESP32-S3-WROOM-1-N16R8) pin map.
// Source: https://seengreat.com/wiki/214/rgb-matrix-hub75-s3 + its V1.0 schematic.
#pragma once
#include <Arduino.h>

// HUB75 (through 74HCT245 level shifters)
#define P_R1 5
#define P_G1 4
#define P_B1 6
#define P_R2 15
#define P_G2 7
#define P_B2 17
#define P_A 8
#define P_B 18
#define P_C 10
#define P_D 9
#define P_E 16
#define P_CLK 12
#define P_LAT 11
#define P_OE 13

// I2C bus: PCA9557 expander, PCF85063 RTC, ES8311 codec (0x18), ES7210 mic ADC (0x40)
#define I2C_SDA 1
#define I2C_SCL 2

// Audio (ES8311 out -> NS4150 amp; ES7210 mics in)
#define I2S_MCLK 38
#define I2S_BCLK 48
#define I2S_LRCK 21
#define I2S_DOUT 14   // ESP32 -> codec (DSDIN)
#define I2S_DIN 47    // mics -> ESP32 (SDOUT)
#define AMP_EN 3      // NS4150 speaker amp, active high

// microSD (SPI)
#define SD_MISO 42
#define SD_CLK 41
#define SD_MOSI 40
#define SD_CS 39

#define BTN_BOOT 0

// PCA9557 expander bits
#define PCA_K1 1        // thumbwheel keys, active LOW (Seengreat demo 01: KEY1=IO1, KEY2=IO3, KEY3=IO2)
#define PCA_K3 2
#define PCA_K2 3
#define PCA_AUDIO_PWR 5 // Audio_PWR_EN -> TLV62569 regulator for the audio chips (on by default; we force it on)

// Power: one cable in the board USB-C feeds the panel too (Q2 ideal-diode path USB_VBUS -> RGB_5V).
// The separate POWER Type-C only adds current for the panel; it does not power the board.

bool expanderBegin();               // finds the PCA9557 on the bus, turns audio power on
uint8_t expanderRead();             // input register
bool expanderFound();
