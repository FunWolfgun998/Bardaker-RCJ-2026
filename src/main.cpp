/**
 * @file main.cpp
 * @brief Test e verifica array sensori VL53L4CX
 * @details Inizializza il ToFManager e stampa le letture sulla seriale.
 */

#include <Arduino.h>
#include <Wire.h>
#include "ToFManager.h"

// Includiamo Pins.h per avere accesso a PIN_SDA e PIN_SCL
#include "Pins.h"

// Istanza globale del Manager
ToFManager tofManager;

// Timer per la stampa seriale (per non rallentare il loop di lettura)
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL_MS = 100; // Stampa ogni 100ms

void printSensorValue(const char* label, int16_t distance, bool isValid) {
    Serial.print("[");
    Serial.print(label);
    Serial.print(":");

    if (distance == -1) Serial.print("OFF"); // Sensore rotto/non avviato
    else if (distance == 8888){ Serial.print("---"); // Sensore OK, ma vuoto davanti
    } else if (!isValid) {
        // Il sensore è online, ma la lettura non è affidabile (Sigma fail, Target perso, etc.)
        // Spesso accade se si punta verso il vuoto o superfici nere opache
        Serial.print("INV");
        // Opzionale: stampa comunque la distanza grezza per debug
        // Serial.print("("); Serial.print(distance); Serial.print(")");
    } else {
        // Lettura valida
        if (distance < 100) Serial.print(" "); // Allineamento estetico
        if (distance < 10)  Serial.print(" ");
        Serial.print(distance);
    }
    Serial.print("] ");
}

void setup() {
    // 1. Inizializzazione Seriale
    Serial.begin(115200);
    // Attesa opzionale per vedere i messaggi di boot se connesso via USB nativa
    delay(2000);

    Serial.println("\n\n=============================================");
    Serial.println("   VL53L4CX Array - System Verification");
    Serial.println("=============================================");

    // 2. Inizializzazione I2C
    // È fondamentale definire i pin corretti qui
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000); // 400kHz Fast Mode raccomandato per 5 sensori

    // 3. Avvio del Manager dei Sensori
    // Questo metodo gestirà la sequenza XSHUT -> Indirizzamento
    Serial.println(">> Inizializzazione Sensori in corso...");
    bool systemStatus = tofManager.begin(&Wire);

    if (systemStatus) {
        Serial.println(">> Inizializzazione completata (Almeno un sensore attivo).");
    } else {
        Serial.println(">> ERRORE CRITICO: Nessun sensore rilevato!");
        Serial.println("   Controllare cablaggio, alimentazione e pin XSHUT.");
        // In un sistema reale, qui potresti fermare tutto o attivare un LED di errore
    }

    Serial.println("=============================================\n");
    Serial.println("Legenda: [Distanza mm] | 'OFF' = Offline | 'INV' = Misura non valida (Out of range/Noisy)");
    delay(1000);
}

void loop() {
    // ---------------------------------------------------------
    // 1. FASE CRITICA: Aggiornamento Macchina a Stati
    // ---------------------------------------------------------
    // Questa funzione DEVE essere chiamata il più spesso possibile.
    // Non contiene delay() e controlla solo se i dati sono pronti.
    tofManager.update();

    // ---------------------------------------------------------
    // 2. FASE DI REPORTING (Ogni 100ms)
    // ---------------------------------------------------------
    if (millis() - lastPrintTime >= PRINT_INTERVAL_MS) {
        lastPrintTime = millis();

        // Recupera l'ultima istantanea dei dati
        ToFData readings = tofManager.getReadings();

        // Stampa formattata
        Serial.print("Distances (mm): ");

        // Iteriamo sui 5 sensori basandoci sull'Enum definito in ToFManager.h
        // Ordine: FL -> FR -> BL -> BR -> Center

        // Front Left
        printSensorValue("FL", readings.distance_mm[TOF_FRONT_LEFT], readings.valid[TOF_FRONT_LEFT]);

        // Front Right
        printSensorValue("FR", readings.distance_mm[TOF_FRONT_RIGHT], readings.valid[TOF_FRONT_RIGHT]);

        // Back Left
        printSensorValue("BL", readings.distance_mm[TOF_BACK_LEFT], readings.valid[TOF_BACK_LEFT]);

        // Back Right
        printSensorValue("BR", readings.distance_mm[TOF_BACK_RIGHT], readings.valid[TOF_BACK_RIGHT]);

        // Center
        printSensorValue("CT", readings.distance_mm[TOF_CENTER], readings.valid[TOF_CENTER]);

        Serial.println(); // Nuova riga
    }
}

/**
 * @brief Helper per stampare in modo ordinato il valore di un sensore
 */
