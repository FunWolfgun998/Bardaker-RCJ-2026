/**
* @file ToFManager.h
 * @brief Driver Manager asincrono per array di 5 sensori VL53L4CX.
 * @author Senior Embedded Developer
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <vl53l4cx_class.h> // STM32duino library

// Inclusione rigorosa come da requisiti
#include "Pins.h"
#include "Constants.h"

// Numero di sensori
#define TOF_COUNT 5

// Enumerazione per indicizzare facilmente i sensori
enum ToFPosition {
    TOF_FRONT_LEFT = 0,
    TOF_FRONT_RIGHT,
    TOF_BACK_LEFT,
    TOF_BACK_RIGHT,
    TOF_CENTER
};

// Struttura dati per restituire le letture in blocco
struct ToFData {
    int16_t distance_mm[TOF_COUNT]; // -1 se offline o range error
    bool    valid[TOF_COUNT];       // true se la lettura è affidabile
};

class ToFManager {
public:
    ToFManager();
    ~ToFManager();

    /**
     * @brief Inizializza il bus e configura i sensori sequenzialmente (Best Effort).
     * @param wireBus Puntatore all'istanza TwoWire (es. &Wire).
     * @return true se almeno un sensore è stato inizializzato correttamente.
     */
    bool begin(TwoWire* wireBus);

    /*
     * @brief Macchina a stati per la lettura asincrona.
     * Da chiamare ciclicamente (Loop o Task FreeRTOS). Non bloccante.
     */
    void update();

    /**
     * @brief Restituisce l'ultima lettura valida disponibile.
     */
    ToFData getReadings();

private:
    TwoWire* _i2c;

    // Struttura interna per gestire il singolo sensore
    struct SensorUnit {
        VL53L4CX* driver;
        uint8_t   xshutPin;
        uint8_t   targetAddr;
        bool      isOnline;
        int16_t   lastDistance;
        bool      dataValid;
        const char* name; // Per debug
    };

    SensorUnit _sensors[TOF_COUNT];

    // Helper per resettare tutti i pin XSHUT
    void shutdownAll();
};

