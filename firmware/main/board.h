// Poket board pin map.
//
// Every number here was pulled out of poket.kicad_sch with
//   kicad-cli sch export netlist --format kicadxml
// rather than typed from memory. If the schematic moves, re-export and fix
// this file; nothing else in the firmware should hard-code a GPIO.
//
// REV B: the module changed from ESP32-S3-WROOM-1 to ESP32-WROOM-32E-R2.
// The S3 has Bluetooth LE only - no BR/EDR - so A2DP source, which is the
// whole point of "sends to Bluetooth headphones", could never have worked on
// it. The classic ESP32 in the -32E-R2 keeps its 2 MB PSRAM in-package, so
// unlike a WROVER it does not eat GPIO16/17 to get it.
#pragma once

#define POKET_FW_VERSION "0.2.0"
#include "driver/gpio.h"

// ---- rotary encoder (EC11, detented, with push) -------------------------
#define PIN_ENC_A        GPIO_NUM_32
#define PIN_ENC_B        GPIO_NUM_33
#define PIN_ENC_SW       GPIO_NUM_35     // input-only: needs the board's 10k

// ---- transport buttons, all active low ---------------------------------
// GPIO34/36/39 are input-only and have NO internal pull-ups, so R20-R22 on
// the board are not optional the way they were on the S3.
#define PIN_BTN_PREV     GPIO_NUM_34
#define PIN_BTN_PLAY     GPIO_NUM_36
#define PIN_BTN_NEXT     GPIO_NUM_39

// ---- I2S to the PCM5102A -----------------------------------------------
// No MCLK line exists: the DAC runs off its internal PLL from BCK/LRCK.
// Configuring an MCLK pin here would be a silent bug - there is no trace.
#define PIN_I2S_BCK      GPIO_NUM_26
#define PIN_I2S_LRCK     GPIO_NUM_25
#define PIN_I2S_DIN      GPIO_NUM_27
#define PIN_DAC_XSMT     GPIO_NUM_14     // soft mute, high = unmuted

// ---- I2C: SSD1306 OLED + BQ27441 fuel gauge ----------------------------
#define PIN_I2C_SDA      GPIO_NUM_21
#define PIN_I2C_SCL      GPIO_NUM_22
#define I2C_ADDR_OLED    0x3C
#define I2C_ADDR_GAUGE   0x55

// ---- microSD over SPI ---------------------------------------------------
// NOT the SDMMC peripheral. On the classic ESP32, slot 1 is hard-wired to
// GPIO12 for DAT2, and GPIO12 is MTDI - the flash voltage strap. A microSD
// card's internal pull-up on DAT2 holds it high at reset, which tells the
// bootloader the flash is 1.8 V and bricks the boot. SPI costs bandwidth
// nobody here needs: 320 kbps audio is 40 kB/s and the Wi-Fi link is the
// bottleneck on uploads regardless.
#define PIN_SD_SCK       GPIO_NUM_18
#define PIN_SD_MISO      GPIO_NUM_19
#define PIN_SD_MOSI      GPIO_NUM_23
#define PIN_SD_CS        GPIO_NUM_5
#define PIN_SD_CD        GPIO_NUM_13     // low = card present
#define SD_SPI_HOST      SPI2_HOST       // HSPI
#define SD_SPI_HZ        (20 * 1000 * 1000)

// ---- misc ---------------------------------------------------------------
#define PIN_AMP_EN       GPIO_NUM_4      // PAM8908 enable, high = on
#define PIN_JACK_DET     GPIO_NUM_15     // low = plug inserted
#define PIN_LED_DIN      GPIO_NUM_2      // WS2812B-2020, single pixel

// Strapping pins and what this board does about them:
//   GPIO0  BOOT button, pulled up. Also driven by the CP2102N auto-reset pair.
//   GPIO2  WS2812 data. Idles low, which is what the strap wants. Safe.
//   GPIO12 MTDI / flash voltage. LEFT UNCONNECTED - see the microSD note.
//   GPIO15 MTDO / boot-log enable, internal pull-up. Jack detect pulls it low
//          when a plug is in, so booting with headphones plugged in is silent
//          on U0TXD. Cosmetic only.
// GPIO6-11 are the module's SPI flash and are not brought out.
// There is no battery-sense divider: ADC1 is fully consumed by the encoder
// and buttons, and the BQ27441 reports voltage and current over I2C anyway.

#define BATT_FULL_MV         4200
#define BATT_EMPTY_MV        3300
