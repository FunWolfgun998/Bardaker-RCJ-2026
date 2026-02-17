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

    /**
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

/*#include <Arduino.h>
#include <Wire.h>
#include "Pins.h" // Assicurati che i pin XSHUT siano corretti qui!

// Array dei pin per iterare facilmente
int xshutPins[] = {
    PIN_XSHUT_FRONT_LEFT, PIN_XSHUT_FRONT_RIGHT,
    PIN_XSHUT_BACK_LEFT, PIN_XSHUT_BACK_RIGHT, PIN_XSHUT_CENTER
};

const char* names[] = { "FL", "FR", "BL", "BR", "CC" };

void setup() {
    Serial.begin(115200);
    while(!Serial) delay(10);
    Serial.println("\n--- TEST DIAGNOSTICO XSHUT & I2C ---");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // 1. SPEGNIMENTO TOTALE
    Serial.println("1. Spengo TUTTI i sensori (XSHUT LOW)...");
    for(int i=0; i<5; i++) {
        pinMode(xshutPins[i], OUTPUT);
        digitalWrite(xshutPins[i], LOW);
    }
    delay(100);

    // VERIFICA: Ora il bus deve essere VUOTO.
    Wire.beginTransmission(0x29);
    if (Wire.endTransmission() == 0) {
        Serial.println("ERRORE GRAVE: Qualcuno risponde ancora a 0x29! Controlla i collegamenti XSHUT.");
        // Se vedi questo messaggio, hai un errore hardware: un XSHUT non è collegato bene.
        while(1);
    } else {
        Serial.println("OK: Nessun sensore rilevato (Corretto).");
    }

    // 2. TEST SINGOLO (Uno alla volta)
    for(int i=0; i<5; i++) {
        Serial.print("Test attivazione ");
        Serial.print(names[i]);
        Serial.print(" (Pin "); Serial.print(xshutPins[i]); Serial.print(")... ");

        // Accendi SOLO questo
        digitalWrite(xshutPins[i], HIGH);
        delay(50); // Tempo di boot sensore

        // Cerca a 0x29
        Wire.beginTransmission(0x29);
        if (Wire.endTransmission() == 0) {
            Serial.println("SUCCESSO! Sensore Trovato.");
        } else {
            Serial.println("FALLITO. Il sensore non risponde.");
        }

        // Spegni di nuovo prima di passare al prossimo
        digitalWrite(xshutPins[i], LOW);
        delay(50);
    }

    Serial.println("--- FINE TEST ---");
}

void loop() {}*/
