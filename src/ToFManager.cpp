#include "ToFManager.h"

ToFManager::ToFManager() {
    _i2c = nullptr;

    // Configurazione Mappatura (Solo dati, niente hardware qui!)
    _sensors[TOF_FRONT_LEFT]  = {nullptr, PIN_XSHUT_FRONT_LEFT,  ADDR_TOF_FL, false, -1, false, "Front_Left"};
    _sensors[TOF_FRONT_RIGHT] = {nullptr, PIN_XSHUT_FRONT_RIGHT, ADDR_TOF_FR, false, -1, false, "Front_Right"};
    _sensors[TOF_BACK_LEFT]   = {nullptr, PIN_XSHUT_BACK_LEFT,   ADDR_TOF_BL, false, -1, false, "Back_Left"};
    _sensors[TOF_BACK_RIGHT]  = {nullptr, PIN_XSHUT_BACK_RIGHT,  ADDR_TOF_BR, false, -1, false, "Back_Right"};
    _sensors[TOF_CENTER]      = {nullptr, PIN_XSHUT_CENTER,      ADDR_TOF_C,  false, -1, false, "Center"};
}

ToFManager::~ToFManager() {
    for (int i = 0; i < TOF_COUNT; i++) {
        if (_sensors[i].driver) delete _sensors[i].driver;
    }
}

void ToFManager::shutdownAll() {
    Serial.println("[ToF] Forcing Hard Reset on all sensors...");

    // 1. Configura TUTTI i pin come OUTPUT e portali a LOW
    for (int i = 0; i < TOF_COUNT; i++) {
        pinMode(_sensors[i].xshutPin, OUTPUT);
        digitalWrite(_sensors[i].xshutPin, LOW);
    }

    // 2. Attendi scarica condensatori (Tempo critico: i sensori VL hanno condensatori interni)
    delay(50);
}

bool ToFManager::begin(TwoWire* wireBus) {
    _i2c = wireBus;

    // FIX 1: Usa 100kHz per la configurazione (più stabile)
    _i2c->setClock(100000);

    // FIX 2: Sequenza di spegnimento rigorosa
    shutdownAll();

    int activeSensors = 0;
    Serial.println("[ToF] Init Sequence Started...");

    for (int i = 0; i < TOF_COUNT; i++) {
        // --- FASE 1: Risveglio Sensore Corrente ---
        Serial.printf("[ToF] Booting %s (Pin %d)... ", _sensors[i].name, _sensors[i].xshutPin);

        digitalWrite(_sensors[i].xshutPin, HIGH);

        // Attendi boot firmware sensore (Datasheet dice 1.2ms, noi diamo 10ms per sicurezza)
        delay(10);

        // --- FASE 2: Verifica Preliminare ---
        // Prima di istanziare, controlliamo se QUALCOSA risponde a 0x29
        _i2c->beginTransmission(0x29);
        if (_i2c->endTransmission() != 0) {
            Serial.println("FAIL (No Ack at 0x29) - Check Wiring/XSHUT");
            digitalWrite(_sensors[i].xshutPin, LOW); // Spegnilo e passa oltre
            continue;
        }

        // --- FASE 3: Configurazione Driver ---
        _sensors[i].driver = new VL53L4CX(_i2c, -1);

        if (!_sensors[i].driver) {
            Serial.println("FAIL (Mem Error)");
            continue;
        }

        // InitSensor
        if (_sensors[i].driver->InitSensor(0x29) == VL53L4CX_ERROR_NONE) {

            _sensors[i].driver->VL53L4CX_SetDeviceAddress(_sensors[i].targetAddr);
            delay(2); // Breve pausa per stabilizzazione I2C interna

            // Avvio Misura
            _sensors[i].driver->VL53L4CX_StartMeasurement();

            _sensors[i].isOnline = true;
            activeSensors++;
            Serial.printf("OK -> Addr: 0x%02X\n", _sensors[i].targetAddr);
        } else {
            Serial.println("FAIL (Init Error)");
            _sensors[i].isOnline = false;
            digitalWrite(_sensors[i].xshutPin, LOW); // Hard Kill
            delete _sensors[i].driver;
            _sensors[i].driver = nullptr;
        }
    }

    // FIX 3: Ripristina velocità alta solo dopo aver configurato tutti gli indirizzi
    _i2c->setClock(400000);

    Serial.printf("[ToF] Init Complete. Active: %d/%d\n", activeSensors, TOF_COUNT);
    return (activeSensors > 0);
}

void ToFManager::update() {
    VL53L4CX_MultiRangingData_t data;
    uint8_t ready = 0;
    uint8_t status = 0;

    for (int i = 0; i < TOF_COUNT; i++) {
        if (!_sensors[i].isOnline || !_sensors[i].driver) continue;

        // Controllo non bloccante
        status = _sensors[i].driver->VL53L4CX_GetMeasurementDataReady(&ready);

        if (status == 0 && ready) { // Status 0 = VL53L4CX_ERROR_NONE

            status = _sensors[i].driver->VL53L4CX_GetMultiRangingData(&data);

            // Pulisci interrupt IMMEDIATAMENTE dopo la lettura
            _sensors[i].driver->VL53L4CX_ClearInterruptAndStartMeasurement();

            if (status == 0) {
                if (data.NumberOfObjectsFound > 0) {
                    // Validiamo lo status hardware del range (0 = Ok, 4 = Phase Fail, etc)
                    if (data.RangeData[0].RangeStatus == 0) {
                        _sensors[i].lastDistance = data.RangeData[0].RangeMilliMeter;
                        _sensors[i].dataValid = true;
                    } else {
                        // Oggetto visto ma lettura sporca
                        _sensors[i].lastDistance = data.RangeData[0].RangeMilliMeter;
                        _sensors[i].dataValid = false;
                    }
                } else {
                    // Nessun oggetto (Out of range)
                    _sensors[i].lastDistance = 8888; // <--- MODIFICA QUI
                    _sensors[i].dataValid = false;
                }
            }
        }
    }
}

ToFData ToFManager::getReadings() {
    ToFData d;
    for (int i = 0; i < TOF_COUNT; i++) {
        d.distance_mm[i] = _sensors[i].isOnline ? _sensors[i].lastDistance : -1;
        d.valid[i] = _sensors[i].isOnline ? _sensors[i].dataValid : false;
    }
    return d;
}