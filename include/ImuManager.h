#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <MPU9250_WE.h>

class ImuManager {
public:
    // Costruttore: accetta i pin I2C
    ImuManager(uint8_t sdaPin, uint8_t sclPin);

    // Inizializzazione hardware e calibrazione
    bool begin();

    // Loop di aggiornamento (da chiamare il più spesso possibile, NO DELAY)
    void update();

    // Getter per i dati elaborati
    float getYaw() const;   // Rotazione asse Z (Gradi)
    float getPitch() const; // Inclinazione rampe (Gradi)
    bool isConnected();

    // Funzione per azzerare lo Yaw corrente (utile all'avvio del robot)
    void resetYaw();

private:
    uint8_t _sda;
    uint8_t _scl;

    MPU9250_WE _mpu;

    // Variabili per il calcolo del Delta Time (dt)
    unsigned long _lastUpdateMicros;
    float _dt; // in secondi

    // Dati di orientamento
    float _yaw;
    float _pitch;

    // Configurazione filtri per vibrazioni
    void configureFilters();
};