#include "ToFManager.h"

ToFManager::ToFManager() {
    _i2c = nullptr;

    // Configurazione Mappatura
    _sensors[TOF_FRONT_LEFT]  = {nullptr, PIN_XSHUT_FRONT_LEFT,  ADDR_TOF_FL, false, -1, false, "Front_Left"};
    _sensors[TOF_FRONT_RIGHT] = {nullptr, PIN_XSHUT_FRONT_RIGHT, ADDR_TOF_FR, false, -1, false, "Front_Right"};
    _sensors[TOF_BACK_LEFT]   = {nullptr, PIN_XSHUT_BACK_LEFT,   ADDR_TOF_BL, false, -1, false, "Back_Left"};
    _sensors[TOF_BACK_RIGHT]  = {nullptr, PIN_XSHUT_BACK_RIGHT,  ADDR_TOF_BR, false, -1, false, "Back_Right"};
    _sensors[TOF_CENTER]      = {nullptr, PIN_XSHUT_CENTER,      ADDR_TOF_C,  false, -1, false, "Center"};

    for (int i = 0; i < TOF_COUNT; i++) {
        pinMode(_sensors[i].xshutPin, OUTPUT);
    }
}

ToFManager::~ToFManager() {
    for (int i = 0; i < TOF_COUNT; i++) {
        if (_sensors[i].driver) delete _sensors[i].driver;
    }
}

void ToFManager::shutdownAll() {
    for (int i = 0; i < TOF_COUNT; i++) {
        digitalWrite(_sensors[i].xshutPin, LOW);
    }
    delay(20);
}

bool ToFManager::begin(TwoWire* wireBus) {
    _i2c = wireBus;
    shutdownAll();

    int activeSensors = 0;
    Serial.println("[ToF] Init Sequence Started...");

    for (int i = 0; i < TOF_COUNT; i++) {
        // Accensione singolo sensore
        digitalWrite(_sensors[i].xshutPin, HIGH);
        delay(10);

        // Istanziazione Driver
        _sensors[i].driver = new VL53L4CX(_i2c, -1); // -1 perché gestiamo XSHUT noi

        if (!_sensors[i].driver) continue;

        // Inizializzazione (Address 0x29)
        Serial.printf("[ToF] Init %s... ", _sensors[i].name);

        // InitSensor restituisce 0 (VL53L4CX_ERROR_NONE) se successo
        if (_sensors[i].driver->InitSensor(0x29) == VL53L4CX_ERROR_NONE) {

            // Cambio Indirizzo (API CORRETTA)
            _sensors[i].driver->VL53L4CX_SetDeviceAddress(_sensors[i].targetAddr);

            // Avvio Misura (API CORRETTA)
            _sensors[i].driver->VL53L4CX_StartMeasurement();

            _sensors[i].isOnline = true;
            activeSensors++;
            Serial.println("OK");
        } else {
            Serial.println("FAIL");
            _sensors[i].isOnline = false;
            digitalWrite(_sensors[i].xshutPin, LOW); // Spegni per non disturbare i prossimi
            delete _sensors[i].driver;
            _sensors[i].driver = nullptr;
        }
    }

    return (activeSensors > 0);
}

void ToFManager::update() {
    VL53L4CX_MultiRangingData_t data;
    uint8_t ready = 0;

    for (int i = 0; i < TOF_COUNT; i++) {
        if (!_sensors[i].isOnline || !_sensors[i].driver) continue;

        // Controllo Data Ready (API CORRETTA)
        _sensors[i].driver->VL53L4CX_GetMeasurementDataReady(&ready);

        if (ready) {
            // Lettura Dati
            _sensors[i].driver->VL53L4CX_GetMultiRangingData(&data);

            // Clear Interrupt (API CORRETTA: ClearInterruptAndStartMeasurement)
            _sensors[i].driver->VL53L4CX_ClearInterruptAndStartMeasurement();

            // Elaborazione semplice (prendiamo il primo oggetto)
            if (data.NumberOfObjectsFound > 0) {
                // RangeStatus 0 = Valid
                if (data.RangeData[0].RangeStatus == 0) {
                    _sensors[i].lastDistance = data.RangeData[0].RangeMilliMeter;
                    _sensors[i].dataValid = true;
                } else {
                    _sensors[i].lastDistance = data.RangeData[0].RangeMilliMeter;
                    _sensors[i].dataValid = false; // Misura presente ma noisy
                }
            } else {
                _sensors[i].lastDistance = -1;
                _sensors[i].dataValid = false;
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