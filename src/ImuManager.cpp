#include "ImuManager.h"

// Indirizzo I2C standard quando AD0 è a GND
#define MPU_ADDR 0x68

ImuManager::ImuManager(uint8_t sdaPin, uint8_t sclPin)
    : _sda(sdaPin), _scl(sclPin), _mpu(MPU9250_WE(MPU_ADDR)),
      _lastUpdateMicros(0), _yaw(0.0f), _pitch(0.0f) {
}

bool ImuManager::begin() {
    Wire.begin(_sda, _scl);
    Wire.setClock(400000);
    delay(100);

    // --- DIAGNOSTICA AVANZATA ---
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x75); // Registro WHO_AM_I
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1);
    uint8_t chipID = Wire.read();

    Serial.printf("\n[DIAGNOSTICA] Chip ID letto: 0x%02X\n", chipID);
    Serial.println("Valori attesi: 0x71 (MPU9250), 0x70 (MPU6500), 0x73 (MPU9255)");

    // --- RESET FORZATO DEL SENSORE ---
    // Scriviamo nel registro PWR_MGMT_1 per resettare il chip
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0x80); // Reset bit
    Wire.endTransmission();
    delay(100); // Attesa dopo reset

    // --- SVEGLIA IL SENSORE ---
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0x00); // Sveglia (Clock interno)
    Wire.endTransmission();
    delay(100);

    // Ora proviamo l'init della libreria
    if (!_mpu.init()) {
        Serial.println("[ERRORE] La libreria rifiuta ancora il chip nonostante il reset.");
        // Non usciamo con false, proviamo a procedere comunque se l'ID è sensato
        if (chipID == 0x00 || chipID == 0xFF) return false;
    }

    Serial.println("[OK] Chip svegliato. Calibrazione offset...");
    _mpu.autoOffsets();

    configureFilters();

    _lastUpdateMicros = micros();
    return true;
}

void ImuManager::configureFilters() {
    /*
     * CONFIGURAZIONE PER ROBOT CINGOLATO (Vibrazioni meccaniche elevate)
     * Riferimento Datasheet MPU-9250:
     * DLPF_CFG | Gyro BW (Hz) | Accel BW (Hz)
     *    4     |      20      |      21.2
     */

    // Imposta il divisore della frequenza di campionamento a 0 (1kHz interno)
    _mpu.setSampleRateDivider(0);

    // RANGE:
    // Accel: 4G è l'ideale per i cingolati (2G è troppo sensibile ai colpi)
    _mpu.setAccRange(MPU9250_ACC_RANGE_8G);

    // Gyro: 500 gradi/secondo (sufficiente per rotazioni rapide del robot)
    _mpu.setGyrRange(MPU6050_GYRO_RANGE_2000);

    // FILTRI DLPF (Digital Low Pass Filter):
    // Usiamo MPU9250_DLPF_4 che corrisponde a:
    // -> Accelerometro: ~21.2 Hz Bandwidth
    // -> Giroscopio:    ~20 Hz Bandwidth
    // Questo taglierà drasticamente le vibrazioni dei motori e dei cingoli.

    _mpu.setAccDLPF(MPU9250_DLPF_4);
    _mpu.setGyrDLPF(MPU9250_DLPF_4);
}

void ImuManager::update() {
    // Controllo di sicurezza: se l'ultima lettura era fallata, non aggiornare
    xyzFloat gValue = _mpu.getGyrValues();

    // Se la libreria restituisce zero o valori assurdi, saltiamo il ciclo
    if (isnan(gValue.z)) return;

    unsigned long currentMicros = micros();
    _dt = (currentMicros - _lastUpdateMicros) / 1000000.0f;

    // Protezione contro dt troppo grandi (es. pause nel codice)
    if (_dt > 0.2f) _dt = 0.02f;

    _lastUpdateMicros = currentMicros;

    float gyroZ = gValue.z;
    if (abs(gyroZ) < 0.25f) gyroZ = 0.0f;

    _yaw += gyroZ * _dt;
    _pitch = _mpu.getPitch();
}

void ImuManager::resetYaw() {
    _yaw = 0.0f;
}

float ImuManager::getYaw() const {
    return _yaw;
}

float ImuManager::getPitch() const {
    return _pitch;
}

bool ImuManager::isConnected() {
    Wire.beginTransmission(0x68); // Indirizzo I2C del sensore
    byte error = Wire.endTransmission();
    return (error == 0); // Ritorna true se il sensore risponde (ACK)
}
/*
 * #include <Arduino.h>
#include <Wire.h>
#include "ImuManager.h"

// Definizione Pin definitivi per ESP32-S3 N16R8
#define I2C_SDA 8
#define I2C_SCL 9

ImuManager imu(I2C_SDA, I2C_SCL);

// Funzione di utilità per scansionare il bus I2C
void scanI2CBus() {
    Serial.println("\n--- SCANSIONE BUS I2C IN CORSO ---");
    byte error, address;
    int nDevices = 0;

    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf("Dispositivo I2C trovato all'indirizzo: 0x%02X", address);
            if (address == 0x68) Serial.println(" (MPU-9250 Standard)");
            else if (address == 0x69) Serial.println(" (MPU-9250 AD0=HIGH)");
            else Serial.println(" (Sconosciuto)");
            nDevices++;
        } else if (error == 4) {
            Serial.printf("Errore sconosciuto all'indirizzo 0x%02X\n", address);
        }
    }

    if (nDevices == 0) {
        Serial.println("ERRORE CRITICO: Nessun dispositivo I2C trovato!");
        Serial.println("Controlla: Cavi SDA/SCL invertiti, alimentazione 3.3V o saldature fredde.");
    }
    Serial.println("----------------------------------\n");
}

void setup() {
    // Il modulo USB-CDC dell'S3 richiede un istante per agganciare la seriale
    Serial.begin(9600);
    unsigned long startWait = millis();
    while (!Serial && (millis() - startWait < 3000));

    Serial.println("=== SYSTEM CHECK: ESP32-S3 N16R8 + MPU-9250 ===");

    // Inizializziamo il bus Wire prima dello scanner
    Wire.begin(I2C_SDA, I2C_SCL);

    // Passaggio 1: Scansione fisica degli indirizzi
    scanI2CBus();

    // Passaggio 2: Inizializzazione della Classe ImuManager
    Serial.println("Inizializzazione ImuManager...");
    if (!imu.begin()) {
        Serial.println("[FAILED] Errore inizializzazione sensore.");
        Serial.println("Possibili cause:");
        Serial.println("- Indirizzo I2C errato (il codice aspetta 0x68)");
        Serial.println("- Sensore guasto o clone non compatibile con registro WHO_AM_I");
        // Non blocchiamo tutto, lasciamo che il loop stampi errori se necessario
    } else {
        Serial.println("[OK] Sensore configurato con successo.");
    }

    Serial.println("\nInizio streaming dati tra 2 secondi...");
    delay(2000);
}

void loop() {
    static unsigned long lastCheck = 0;
    static unsigned long lastPrint = 0;

    // Aggiornamento dati (Eseguito alla massima velocità possibile)
    imu.update();

    // 1. Controllo Integrità ogni 2 secondi
    if (millis() - lastCheck > 2000) {
        if (!imu.isConnected()) {
            Serial.println("!!! ATTENZIONE: Connessione IMU persa !!!");
        }
        lastCheck = millis();
    }

    // 2. Output dei dati ogni 200ms
    if (millis() - lastPrint > 200) {
        float y = imu.getYaw();
        float p = imu.getPitch();

        Serial.print(">Yaw:");
        Serial.print(y);
        Serial.print(",>Pitch:");
        Serial.print(p);

        // Debug visivo semplice
        if (abs(p) > 15.0) {
            Serial.print(" | [RAMPA RILEVATA]");
        }
        if (abs(y) > 0.1) { // Indica se il robot sta ruotando
            Serial.print(" | [ROTAZIONE]");
        }

        Serial.println();
        lastPrint = millis();
    }
}
 */