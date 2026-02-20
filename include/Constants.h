#pragma once

#include <Arduino.h>

/**
 System Constants & Configuration
 Single Source of Truth for Logic/Physics values.
 */
#define ADDR_TOF_FL 0x30
#define ADDR_TOF_FR 0x31
#define ADDR_TOF_BL 0x32
#define ADDR_TOF_BR 0x33
#define ADDR_TOF_C  0x34
#define AS7262_I2C_ADDR 0x49

// --- Configurazione Sensore ---
// Tempo integrazione: valore * 2.8ms. 10 * 2.8 = 28ms (~35Hz)
#define AS7262_INTEGRATION_VALUE 10
// Gain: 0=1x, 1=3.7x, 2=16x, 3=64x
#define AS7262_GAIN_VALUE 2

// --- Soglie e Parametri Algoritmo ---
// EMA Alpha: 0.0-1.0. Più basso = più filtro (più lento), Più alto = più reattivo
#define EMA_ALPHA 0.6f

// Soglia raw sum per considerare "Buco Nero"
#define DEFAULT_BLACK_THRESHOLD 50.0f

// Rapporto moltiplicativo: (Luce Attuale) > (Bianco Calibrato * Ratio) = Argento
#define DEFAULT_SILVER_RATIO 1.5f