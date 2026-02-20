#include "ColorManager.h"

ColorManager::ColorManager() {
    for(int i=0; i<6; i++) _channelValues[i] = 0.0f;
    _calData.calibrated = false;
    _calData.global_intensity_white = 1000.0f;
}

bool ColorManager::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // Tentativo di connessione
    if (!_as7262.begin(&Wire)) {
        Serial.println("ERRORE: Sensore AS7262 non trovato!");
        return false;
    }

    // Configurazione Sensore
    _as7262.setConversionType(MODE_2);

    // Imposta tempo integrazione (valore * 2.8ms)
    // Se AS7262_INTEGRATION_VALUE è 10 -> 28ms
    _as7262.setIntegrationTime(AS7262_INTEGRATION_VALUE);

    // FIX: Rimosso casting errato. La funzione accetta uint8_t (0, 1, 2, 3)
    // 0=1x, 1=3.7x, 2=16x, 3=64x
    _as7262.setGain(AS7262_GAIN_VALUE);

    // LED Default ON
    setLedCurrent(0); // 0 = 12.5mA (Default safe)
    enableLed(true);

    loadCalibration();
    return true;
}

void ColorManager::enableLed(bool enable) {
    if (enable) {
        _as7262.drvOn(); // Accende il driver LED
    } else {
        _as7262.drvOff();
    }
}

void ColorManager::setLedCurrent(uint8_t current_level) {
    // Mapping: 0=12.5mA, 1=25mA, 2=50mA, 3=100mA
    if (current_level > 3) current_level = 3;

    // FIX: Rimosso casting errato. La funzione accetta uint8_t.
    _as7262.setDrvCurrent(current_level);
}

void ColorManager::update() {
    // Controllo asincrono
    if (!_as7262.dataReady()) {
        return;
    }

    // Lettura dei 6 canali
    uint16_t rawReadings[6];
    _as7262.readRawValues(rawReadings);

    // Filtro EMA (Exponential Moving Average)
    for(int i=0; i<6; i++) {
        _channelValues[i] = (EMA_ALPHA * (float)rawReadings[i]) + ((1.0f - EMA_ALPHA) * _channelValues[i]);
    }
}

void ColorManager::calibrate(ColorType colorTarget) {
    // Calibrazione bloccante (si fa solo a robot fermo)
    float avgSum = 0;
    int samples = 20;

    for(int i=0; i<samples; i++) {
        // Attesa attiva dei dati
        while(!_as7262.dataReady()) { delay(5); }

        uint16_t temp[6];
        _as7262.readRawValues(temp);
        for(int k=0; k<6; k++) avgSum += temp[k];
    }

    avgSum /= samples; // Media della somma totale

    if (colorTarget == COLOR_WHITE) {
        _calData.global_intensity_white = avgSum;
        _calData.calibrated = true;
        saveCalibration();
    }
}

void ColorManager::loadCalibration() {
    _prefs.begin("color_cal", true); // Read-only
    _calData.calibrated = _prefs.getBool("is_cal", false);
    if(_calData.calibrated) {
        _calData.global_intensity_white = _prefs.getFloat("int_w", 1000.0f);
    }
    _prefs.end();
}

void ColorManager::saveCalibration() {
    _prefs.begin("color_cal", false); // Read-write
    _prefs.putBool("is_cal", true);
    _prefs.putFloat("int_w", _calData.global_intensity_white);
    _prefs.end();
}

float ColorManager::getSum() {
    float sum = 0;
    for(int i=0; i<6; i++) sum += _channelValues[i];
    return sum;
}

float ColorManager::getNormalized(int channelIndex) {
    float sum = getSum();
    if(sum < 1.0f) return 0.0f; // Evita divisione per zero
    return _channelValues[channelIndex] / sum;
}

bool ColorManager::isBlack() {
    // Se la luce totale è sotto la soglia minima -> Buco/Nero
    return getSum() < DEFAULT_BLACK_THRESHOLD;
}

bool ColorManager::isSilver() {
    float sum = getSum();

    // 1. Saturazione Hardware (Riflesso speculare estremo)
    if (sum > 50000.0f) return true;

    // 2. Controllo rispetto alla calibrazione del bianco
    if (_calData.calibrated) {
        // L'argento riflette molto più del bianco diffuso (es. > 1.5x)
        if (sum > (_calData.global_intensity_white * DEFAULT_SILVER_RATIO)) {
            return true;
        }
    }
    return false;
}

bool ColorManager::isRed() {
    if (isBlack() || isSilver()) return false;

    // Canali: 0=V, 1=B, 2=G, 3=Y, 4=O, 5=R
    float normR = getNormalized(5); // Red
    float normB = getNormalized(1); // Blue

    // Il rosso ha una componente dominante nel canale R e bassa nel B
    return (normR > 0.35f) && (normR > normB * 2.0f);
}

bool ColorManager::isBlue() {
    if (isBlack() || isSilver()) return false;

    float normB = getNormalized(1);
    float normR = getNormalized(5);

    // Il blu/ciano domina i canali bassi
    return (normB > 0.30f) && (normB > normR * 1.5f);
}

bool ColorManager::isWhite() {
    if (isBlack()) return false;

    float normR = getNormalized(5);
    float normB = getNormalized(1);

    // Bianco = Spettro piatto (differenza tra R e B bassa) E non è argento
    return (abs(normR - normB) < 0.15f) && !isSilver();
}

ColorType ColorManager::getDominantColor() {
    if (isBlack()) return COLOR_BLACK;
    if (isSilver()) return COLOR_SILVER; // Priorità alta (checkpoint)
    if (isRed()) return COLOR_RED;
    if (isBlue()) return COLOR_BLUE;
    if (isWhite()) return COLOR_WHITE;
    return COLOR_UNKNOWN;
}

float ColorManager::getTemperature() {
    float normR = getNormalized(5);
    float normB = getNormalized(1);

    // Formula approssimativa per CCT
    float ratio = normB / (normR + 0.001f);
    return ratio * 3000.0f + 2000.0f;
}