#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include "Pins.h"
#include "Constants.h"
#include "ColorManager.h"

#define PIN_RGB_LED 48
#define NUM_PIXELS 1

Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
ColorManager colorMgr;

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 200;

void printMenu() {
    Serial.println("\n--- CLIMBER COLOR VISUALIZER ---");
    Serial.println("[w] -> Calibra BIANCO (Reference)");
    Serial.println("[n] -> Calibra NERO (Soglia dinamica)");
    Serial.println("[r] -> Calibra ROSSO");
    Serial.println("[b] -> Calibra BLU");
    Serial.println("[e] -> ESPORTA Calibrazioni per Constants.h"); // NUOVO COMANDO
    Serial.println("--------------------------------");
}

void setup() {
    delay(2000); // Essenziale per ESP32-S3 USB Nativa
    Serial.begin(115200);

    pixels.begin();
    pixels.setBrightness(20);
    pixels.clear();
    pixels.show();

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!colorMgr.begin(&Wire)) {
        Serial.println("ERRORE: AS7262 non trovato!");
        while (1) { delay(100); }
    }

    Serial.println("Sensore OK.");
    printMenu();
}

void handleSerialInput() {
    if (Serial.available()) {
        char cmd = Serial.read();
        while(Serial.available() && isSpace(Serial.peek())) Serial.read();

        switch (cmd) {
            case 'w': colorMgr.calibrate(COLOR_WHITE); Serial.println("BIANCO Calibrato."); break;
            case 'n': colorMgr.calibrate(COLOR_BLACK); Serial.println("NERO Calibrato."); break;
            case 'r': colorMgr.calibrate(COLOR_RED);   Serial.println("ROSSO Calibrato."); break;
            case 'b': colorMgr.calibrate(COLOR_BLUE);  Serial.println("BLU Calibrato."); break;
            case 'e':
                // NUOVO COMANDO: Stampa i valori su Seriale
                colorMgr.exportCalibrationToSerial();
                break;
        }
    }
}

void loop() {
    colorMgr.update();
    handleSerialInput();

    // ==========================================
    // LOGICA LED CON COLORI PURI ASSOLUTI
    // ==========================================
    ColorType detected = colorMgr.getDominantColor();

    if (detected == COLOR_BLACK) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Spento
    }
    else if (detected == COLOR_SILVER) {
        // Effetto lampeggiante per l'Argento
        if ((millis() / 100) % 2 == 0) pixels.setPixelColor(0, pixels.Color(255, 255, 255));
        else pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    }
    else if (detected == COLOR_RED) {
        pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // ROSSO PURO ASSOLUTO
    }
    else if (detected == COLOR_BLUE) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // BLU PURO ASSOLUTO
    }
    else if (detected == COLOR_WHITE) {
        pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // BIANCO PURO ASSOLUTO
    }
    else {
        // Se non è sicuro (es. sul parquet o in transizione),
        // usiamo la conversione RGB grezza riutilizzando il metodo isolato.
        RGBColor raw = colorMgr.getVisualRGB();
        pixels.setPixelColor(0, pixels.Color(raw.r, raw.g, raw.b));
    }

    pixels.show();

    // Debug Seriale
    if (millis() - lastPrintTime > PRINT_INTERVAL) {
        const SpectralData& data = colorMgr.getCurrentData();

        Serial.printf("SUM: %6.1f | Detect: ", data.sum);
        switch(detected) {
            case COLOR_BLACK: Serial.print("NERO (BUCO)"); break;
            case COLOR_SILVER: Serial.print("ARGENTO (CHECKPOINT)"); break;
            case COLOR_WHITE: Serial.print("BIANCO"); break;
            case COLOR_RED: Serial.print("ROSSO"); break;
            case COLOR_BLUE: Serial.print("BLU"); break;
            default: Serial.print("SCONOSCIUTO"); break;
        }
        Serial.println();
        lastPrintTime = millis();
    }
}