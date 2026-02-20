#pragma once
#include <Arduino.h>
//SDA e SLC in comune
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

// XSHUT Pins per ToF VL53L4CX
#define PIN_XSHUT_FRONT_LEFT  7
#define PIN_XSHUT_FRONT_RIGHT 15
#define PIN_XSHUT_BACK_LEFT   16
#define PIN_XSHUT_BACK_RIGHT  17
#define PIN_XSHUT_CENTER      18

/*
 Hardware Definition per ESP32-S3 DevKitC-1 (N16R8)
 NOTE DI SICUREZZA:
 - Evitare GPIO 0, 45, 46 (Strapping Pins) per output attivi al boot.
 - GPIO 26-37 riservati per Octal Flash/PSRAM (Non utilizzare).
 */
