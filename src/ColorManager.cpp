#include "ColorManager.h"

ColorManager::ColorManager() : _isMeasuring(false), _lastUpdate(0) {
    memset(&_currentData, 0, sizeof(SpectralData));
}

bool ColorManager::begin(TwoWire* i2cBus, bool ledOn) {
    _wire = i2cBus;

    if (!_sensor.begin(_wire)) {
        log_e("AS7262 non trovato!");
        return false;
    }

    // Configurazione Sensore
    _sensor.setIntegrationTime(AS7262_INTEGRATION_VALUE);
    _sensor.setGain(AS7262_GAIN_VALUE);

    // LED Always On di default per garantire stabilità termica e illuminazione
    enableLed(ledOn);
    setLedCurrent(3); // 25mA, bilanciamento tra luminosità e calore

    loadCalibration();

    // Avvia la prima misurazione asincrona
    _sensor.startMeasurement();
    _isMeasuring = true;

    return true;
}

void ColorManager::update() {
    // Controllo asincrono: il task non viene mai bloccato.
    if (_isMeasuring && _sensor.dataReady()) {

        // Lettura RAW calibrata (compensata internamente dal chip)
        float newChannels[CH_COUNT];
        newChannels[V] = _sensor.readCalibratedViolet();
        newChannels[B] = _sensor.readCalibratedBlue();
        newChannels[G] = _sensor.readCalibratedGreen();
        newChannels[Y] = _sensor.readCalibratedYellow();
        newChannels[O] = _sensor.readCalibratedOrange();
        newChannels[R] = _sensor.readCalibratedRed();

        float newSum = 0;

        // Applica Filtro EMA (Exponential Moving Average) e calcola Somma
        for(int i = 0; i < CH_COUNT; i++) {
            _currentData.channels[i] = (newChannels[i] * EMA_ALPHA) + (_currentData.channels[i] * (1.0f - EMA_ALPHA));
            newSum += _currentData.channels[i];
        }
        _currentData.sum = newSum;

        // Riavvia subito l'integrazione hardware per la prossima lettura (~28ms)
        _sensor.startMeasurement();
    }
}

void ColorManager::enableLed(bool state) {
    if (state) _sensor.drvOn();
    else _sensor.drvOff();
}

void ColorManager::setLedCurrent(uint8_t currentLevel) {
    // AS7262 limits: 0: 12.5mA, 1: 25mA, 2: 50mA, 3: 100mA
    if(currentLevel > 3) currentLevel = 3;
    _sensor.setDrvCurrent(currentLevel);
}

void ColorManager::calibrate(ColorType type) {
    switch(type) {
        case COLOR_WHITE: _refWhite = _currentData; break;
        case COLOR_RED:   _refRed = _currentData;   break;
        case COLOR_BLUE:  _refBlue = _currentData;  break;
        case COLOR_BLACK: _refBlack = _currentData; break; // <-- NUOVO
        default: return;
    }
    saveCalibration(type, _currentData);
}



void ColorManager::exportCalibrationToSerial() const {

    auto printData =[](const char* name, const SpectralData& d) {
        Serial.printf("const float CALIB_%s_SUM = %.2ff;\n", name, d.sum);
        Serial.printf("const float CALIB_%s_CH[6] = {%.2ff, %.2ff, %.2ff, %.2ff, %.2ff, %.2ff};\n\n",
            name, d.channels[0], d.channels[1], d.channels[2], d.channels[3], d.channels[4], d.channels[5]);
    };

    printData("WHITE", _refWhite);
    printData("BLACK", _refBlack);
    printData("RED", _refRed);
    printData("BLUE", _refBlue);

}

// ==========================================
// ALGORITMI DI CLASSIFICAZIONE
// ==========================================


ColorType ColorManager::getDominantColor() {
    // 1. ARGENTO: Basato sull'intensità estrema (Riflesso speculare)
    if (_currentData.sum > (_refWhite.sum * DEFAULT_SILVER_RATIO)) {
        return COLOR_SILVER;
    }

    // 2. NERO ASSOLUTO (Floor limit)
    // Se la luce è quasi inesistente (es. il robot è sollevato in aria), è rumore.
    // Nessun colore può essere calcolato con così poca luce.
    const float ABSOLUTE_MIN_SUM = 15.0f;
    if (_currentData.sum < ABSOLUTE_MIN_SUM) {
        return COLOR_BLACK;
    }

    // 3. Calcolo Distanze Spettrali (forma del colore normalizzata)
    float distWhite = calculateSpectralDistance(_currentData, _refWhite);
    float distRed   = calculateSpectralDistance(_currentData, _refRed);
    float distBlue  = calculateSpectralDistance(_currentData, _refBlue);

    // 4. SMART SHADOW LOGIC (Per leggere i colori a >4cm di altezza)
    float dynamicBlackThreshold = _refBlack.sum * 1.5f + 10.0f;

    if (_currentData.sum < dynamicBlackThreshold) {
        // ZONA D'OMBRA: C'è poca luce. Potrebbe essere nastro nero OPPURE un colore lontano.
        // Guardiamo la "firma spettrale". Se l'errore quadratico rispetto al rosso/blu
        // è molto basso (< 0.15), significa che la poca luce ha il colore purissimo.
        const float SHAPE_CONFIDENCE = 0.15f;

        if (distRed < SHAPE_CONFIDENCE && distRed < distBlue && distRed < distWhite) {
            return COLOR_RED; // È buio, ma la traccia è chiaramente ROSSA
        }
        if (distBlue < SHAPE_CONFIDENCE && distBlue < distRed && distBlue < distWhite) {
            return COLOR_BLUE; // È buio, ma la traccia è chiaramente BLU
        }

        // Se è buio e la luce non ha una firma netta (è "grigia" sporca), allora è vero NERO.
        return COLOR_BLACK;
    }

    // 5. ZONA NORMALE (Pavimento vicino)
    // Se la luce è abbondante, vince semplicemente chi ha la distanza matematica minore
    if (distWhite < distRed && distWhite < distBlue) return COLOR_WHITE;
    if (distRed < distWhite && distRed < distBlue) return COLOR_RED;
    return COLOR_BLUE;
}

RGBColor ColorManager::getVisualRGB() const {
    RGBColor output = {0, 0, 0};

    // Pesi per convertire i 6 canali spettrali in R, G, B umani
    float r_mix = _currentData.channels[R] * 1.0f + _currentData.channels[O] * 0.7f + _currentData.channels[Y] * 0.4f + _currentData.channels[V] * 0.3f;
    float g_mix = _currentData.channels[G] * 1.0f + _currentData.channels[Y] * 0.8f + _currentData.channels[B] * 0.3f;
    float b_mix = _currentData.channels[B] * 1.0f + _currentData.channels[V] * 0.8f + _currentData.channels[G] * 0.2f;

    // Auto-Gain per visualizzare il colore puro
    float maxVal = max(r_mix, max(g_mix, b_mix));

    if (maxVal > 5.0f) { // Evita rumore di fondo se è buio
        output.r = (uint8_t)((r_mix / maxVal) * 255.0f);
        output.g = (uint8_t)((g_mix / maxVal) * 255.0f);
        output.b = (uint8_t)((b_mix / maxVal) * 255.0f);
    }

    return output;
}
bool ColorManager::isBlack()  { return getDominantColor() == COLOR_BLACK; }
bool ColorManager::isSilver() { return getDominantColor() == COLOR_SILVER; }
bool ColorManager::isWhite()  { return getDominantColor() == COLOR_WHITE; }
bool ColorManager::isRed()    { return getDominantColor() == COLOR_RED; }
bool ColorManager::isBlue()   { return getDominantColor() == COLOR_BLUE; }
float ColorManager::getTemperature() {
    // Metodo della libreria per leggere la temp sul chip (compensa derive termiche)
    return _sensor.readTemperature();
}

const SpectralData& ColorManager::getCurrentData() const {
    return _currentData;
}

float ColorManager::getBlackThreshold() const {
    return _refBlack.sum * 1.5f + 10.0f;
}

// Calcola l'errore quadratico medio (distanza euclidea) su valori NORMALIZZATI.
// Questo rende il rilevamento del colore indipendente dall'altezza/luce assoluta.
float ColorManager::calculateSpectralDistance(const SpectralData& sample, const SpectralData& reference) {
    if (sample.sum == 0 || reference.sum == 0) return 9999.0f;

    float distance = 0.0f;
    for(int i = 0; i < CH_COUNT; i++) {
        float sampleNorm = sample.channels[i] / sample.sum;
        float refNorm = reference.channels[i] / reference.sum;
        float diff = sampleNorm - refNorm;
        distance += (diff * diff);
    }
    return distance;
}

// ==========================================
// NVM / FLASH MANAGEMENT
// ==========================================

void ColorManager::loadCalibration() {
    _prefs.begin("color_calib", true); // RO mode

    // Lettura (con fallback predefinito, simulato a 1.0/1000.0 se non presente per evitare divisioni per zero)
    _refWhite.sum = _prefs.getFloat("w_sum", 1000.0f);
    _refRed.sum   = _prefs.getFloat("r_sum", 1000.0f);
    _refBlue.sum  = _prefs.getFloat("b_sum", 1000.0f);
    _refBlack.sum = _prefs.getFloat("bk_sum", 30.0f);

    for(int i=0; i<CH_COUNT; i++) {
        String key = "ch_" + String(i);
        _refWhite.channels[i] = _prefs.getFloat(("w_"+key).c_str(), 1000.0f / CH_COUNT);
        _refRed.channels[i]   = _prefs.getFloat(("r_"+key).c_str(), 1000.0f / CH_COUNT);
        _refBlue.channels[i]  = _prefs.getFloat(("b_"+key).c_str(), 1000.0f / CH_COUNT);
        _refBlack.channels[i] = _prefs.getFloat(("bk_"+key).c_str(), 5.0f);
    }
    _prefs.end();
}

void ColorManager::saveCalibration(ColorType type, const SpectralData& data) {
    _prefs.begin("color_calib", false); // RW mode
    String prefix;
    if (type == COLOR_WHITE) prefix = "w_";
    else if (type == COLOR_RED) prefix = "r_";
    else if (type == COLOR_BLUE) prefix = "b_";
    else if (type == COLOR_BLACK) prefix = "bk_";
    else return;

    _prefs.putFloat((prefix + "sum").c_str(), data.sum);
    for(int i=0; i<CH_COUNT; i++) {
        _prefs.putFloat((prefix + "ch_" + String(i)).c_str(), data.channels[i]);
    }
    _prefs.end();
}