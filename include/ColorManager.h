#ifndef COLOR_MANAGER_H
#define COLOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AS726x.h>
#include <Preferences.h>
#include "Constants.h" // Deve essere incluso qui
#include "Pins.h"

enum ColorType {
    COLOR_UNKNOWN = 0,
    COLOR_BLACK,
    COLOR_WHITE,
    COLOR_SILVER,
    COLOR_RED,
    COLOR_BLUE
};

struct ColorCalibrationData {
    float global_intensity_white;
    bool calibrated;
};

class ColorManager {
public:
    ColorManager();
    bool begin();
    void update();

    // Metodi LED corretti per Adafruit AS726x
    void enableLed(bool enable);
    void setLedCurrent(uint8_t current_level); // 0=12.5mA, 1=25mA, 2=50mA, 3=100mA

    void calibrate(ColorType colorTarget);

    bool isBlack();
    bool isSilver();
    bool isRed();
    bool isBlue();
    bool isWhite();

    ColorType getDominantColor();
    float getTemperature();

private:
    Adafruit_AS726x _as7262;
    Preferences _prefs;
    ColorCalibrationData _calData;

    float _channelValues[6]; // V, B, G, Y, O, R

    float getSum();
    float getNormalized(int channelIndex);
    void loadCalibration();
    void saveCalibration();
};

#endif