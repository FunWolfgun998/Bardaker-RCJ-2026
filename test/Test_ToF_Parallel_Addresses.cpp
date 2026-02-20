#include <Arduino.h>
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

void loop() {}