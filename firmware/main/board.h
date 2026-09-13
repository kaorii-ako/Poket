// Poket board pin map.
//
// Every number here was pulled out of poket.kicad_sch with
//   kicad-cli sch export netlist --format kicadxml
// rather than typed from memory. If the schematic moves, re-export and fix
// this file; nothing else in the firmware should hard-code a GPIO.
#pragma once
#include "driver/gpio.h"

// ---- rotary encoder (EC11, detented, with push) -------------------------
#define PIN_ENC_A        GPIO_NUM_4
#define PIN_ENC_B        GPIO_NUM_5
#define PIN_ENC_SW       GPIO_NUM_6      // active low, 10k pull-up on board

// ---- transport buttons, all active low with 10k pull-ups ---------------
#define PIN_BTN_PREV     GPIO_NUM_7
#define PIN_BTN_PLAY     GPIO_NUM_38
#define PIN_BTN_NEXT     GPIO_NUM_39

// ---- I2S to the PCM5102A -----------------------------------------------
// No MCLK line exists: the DAC runs off its internal PLL from BCK/LRCK.
// Configuring an MCLK pin here would be a silent bug - there is no trace.
#define PIN_I2S_BCK      GPIO_NUM_15
#define PIN_I2S_LRCK     GPIO_NUM_16
#define PIN_I2S_DIN      GPIO_NUM_17
#define PIN_DAC_XSMT     GPIO_NUM_40     // soft mute, high = unmuted

// ---- I2C: SSD1306 OLED + BQ27441 fuel gauge ----------------------------
#define PIN_I2C_SDA      GPIO_NUM_8
#define PIN_I2C_SCL      GPIO_NUM_9
#define I2C_ADDR_OLED    0x3C
#define I2C_ADDR_GAUGE   0x55

// ---- microSD, 4-bit SDIO (not SPI) -------------------------------------
#define PIN_SD_CLK       GPIO_NUM_12
#define PIN_SD_CMD       GPIO_NUM_11
#define PIN_SD_D0        GPIO_NUM_13
#define PIN_SD_D1        GPIO_NUM_14
#define PIN_SD_D2        GPIO_NUM_10
#define PIN_SD_D3        GPIO_NUM_18
#define PIN_SD_CD        GPIO_NUM_42     // low = card present

// ---- misc ---------------------------------------------------------------
#define PIN_AMP_EN       GPIO_NUM_21     // PAM8908 enable, high = on
#define PIN_JACK_DET     GPIO_NUM_41     // low = plug inserted
#define PIN_LED_DIN      GPIO_NUM_47     // WS2812B-2020, single pixel
#define PIN_FG_ALERT     GPIO_NUM_48     // BQ27441 GPOUT, open drain
#define PIN_VBAT_SENSE   GPIO_NUM_1      // ADC1_CH0, 100k/100k divider

// GPIO35/36/37 are consumed internally by the octal PSRAM on the -N16R8
// module and must never be driven. GPIO19/20 are the native USB pins.
// GPIO0/3/45/46 are strapping pins; only GPIO0 (BOOT) is used.

#define VBAT_DIVIDER_RATIO   2.0f        // 100k:100k
#define BATT_FULL_MV         4200
#define BATT_EMPTY_MV        3300
