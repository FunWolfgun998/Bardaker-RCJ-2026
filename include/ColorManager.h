#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS726x.h>
#include <Preferences.h>
#include "Pins.h"
#include "Constants.h"

// Fallback in case they are not in Constants.h
#ifndef DEFAULT_BLACK_THRESHOLD
#define DEFAULT_BLACK_THRESHOLD 50.0f
#endif
#ifndef DEFAULT_SILVER_RATIO
#define DEFAULT_SILVER_RATIO 1.5f
#endif
#ifndef EMA_ALPHA
#define EMA_ALPHA 0.6f
#endif

// Indici dei canali spettrali
enum AS_CH { V = 0, B, G, Y, O, R, CH_COUNT };

enum ColorType {
    COLOR_NONE = 0,
    COLOR_BLACK,
    COLOR_SILVER,
    COLOR_WHITE,
    COLOR_RED,
    COLOR_BLUE
};

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct SpectralData {
    float channels[CH_COUNT];
    float sum;
};

class ColorManager {
public:
    ColorManager();

    bool begin(TwoWire* i2cBus = &Wire, bool ledOn = true);

    // Deve essere chiamato il più velocemente possibile nel loop/task
    void update();

    // Hardware Control
    void enableLed(bool state);
    void setLedCurrent(uint8_t currentLevel); // 0=12.5mA, 1=25mA, 2=50mA, 3=100mA

    // Calibration
    void calibrate(ColorType type);
    void exportCalibrationToSerial() const;

    // Getters
    bool isBlack();
    bool isSilver();
    bool isWhite();
    bool isRed();
    bool isBlue();

    ColorType getDominantColor();
    RGBColor getVisualRGB() const;

    float getTemperature();
    const SpectralData& getCurrentData() const;
    float getBlackThreshold() const;

private:
    Adafruit_AS726x _sensor;
    Preferences _prefs;
    TwoWire* _wire;

    SpectralData _currentData;

    // Calibrated Reference Profiles
    SpectralData _refWhite;
    SpectralData _refRed;
    SpectralData _refBlue;
    SpectralData _refBlack;

    bool _isMeasuring;
    unsigned long _lastUpdate;

    void loadCalibration();
    void saveCalibration(ColorType type, const SpectralData& data);

    // Algoritmo di classificazione spettrale
    float calculateSpectralDistance(const SpectralData& sample, const SpectralData& reference);
};