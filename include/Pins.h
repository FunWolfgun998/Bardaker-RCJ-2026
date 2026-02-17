#pragma once
#include <Arduino.h>

#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

// XSHUT Pins per VL53L4CX
#define PIN_XSHUT_FRONT_LEFT  10
#define PIN_XSHUT_FRONT_RIGHT 11
#define PIN_XSHUT_BACK_LEFT   12
#define PIN_XSHUT_BACK_RIGHT  13
#define PIN_XSHUT_CENTER      14

/*
 Hardware Definition per ESP32-S3 DevKitC-1 (N16R8)
 NOTE DI SICUREZZA:
 - Evitare GPIO 0, 45, 46 (Strapping Pins) per output attivi al boot.
 - GPIO 26-37 riservati per Octal Flash/PSRAM (Non utilizzare).
 */
