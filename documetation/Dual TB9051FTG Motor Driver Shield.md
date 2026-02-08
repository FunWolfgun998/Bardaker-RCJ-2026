Questa è la **Documentazione Tecnica Definitiva** per l'integrazione del driver **Pololu Dual TB9051FTG** con **ESP32-S3 (N16R8)**. Questa guida sintetizza tutte le analisi fatte, risolvendo le ambiguità su pinout, livelli logici e gestione software.

---

# Dual TB9051FTG & ESP32-S3

## 1. Specifiche Hardware & Elettriche

Il TB9051FTG è un bridge H di classe automotive, ideale per motori DC ad alta potenza.

### 1.1 Range Operativi
| Parametro | Valore | Note |
| :--- | :--- | :--- |
| **Tensione Motori (VIN)** | 4.5V – 28V | Connessione via morsetti blu grandi. |
| **Corrente Continua** | 2.6 A per canale | Con picchi fino a 5A (brevi periodi). |
| **Logica (VCC)** | 3.0V – 5.5V | Alimentare a **5V** dall'ESP32. |
| **Livello Segnali (IOREF)** | **3.3V** | **CRITICO:** Collega al pin 3.3V dell'ESP32. |
| **PWM Frequency** | Fino a 20 kHz | Raccomandato per evitare rumore udibile. |

### 1.2 Avvertenze Critiche di Sicurezza
1.  **Condensatore:** Salda il condensatore elettrolitico fornito tra **VIN** e **GND** sui morsetti di potenza (osserva la polarità: striscia grigia = GND).
2.  **Massa Comune:** Il GND della batteria (morsetti) e il GND dell'ESP32 **devono** essere collegati insieme.
3.  **Surriscaldamento:** Sopra i 2A il chip richiede un dissipatore o una ventola. Il driver integra uno shutdown termico, ma lo stress ripetuto può danneggiarlo.

---

## 2. Wiring Diagram (ESP32-S3 N16R8)

L'S3 N16R8 ha pin riservati (GPIO 26-32) per la PSRAM Octal. Lo schema seguente evita conflitti e usa i canali ADC1 per compatibilità con il Wi-Fi.

### 2.1 Connessioni Logiche (Header Laterale & Pin Header)

| Pin Shield Pololu | Funzione | GPIO ESP32-S3 | Logica di Funzionamento | Descrizione |
| :--- | :--- | :--- | :--- | :--- |
| **VCC** | Logic Supply | **5V / VIN** | Alimentazione | Alimenta la logica del chip TB9051FTG. |
| **IOREF** | I/O Reference | **3.3V** | Riferimento | **CRITICO:** Imposta i segnali a 3.3V per non bruciare l'S3. |
| **GND** | Ground | **GND** | Massa | Deve essere in comune tra ESP32 e Driver. |
| **M1PWM** | Speed M1 | **GPIO 1** | PWM (0-400) | Controlla la velocità del Motore 1. |
| **M1DIR** | Direction M1 | **GPIO 2** | Digital Out | HIGH = Avanti, LOW = Indietro. |
| **M1EN** | Enable M1 | **GPIO 42** | **HIGH** | Deve essere HIGH per attivare il Motore 1. |
| **M1ENB** | Enable Bar M1| **GPIO 41** | **LOW** | Deve essere LOW per attivare il Motore 1. |
| **M2PWM** | Speed M2 | **GPIO 4** | PWM (0-400) | Controlla la velocità del Motore 2. |
| **M2DIR** | Direction M2 | **GPIO 5** | Digital Out | HIGH = Avanti, LOW = Indietro. |
| **M2EN** | Enable M2 | **GPIO 6** | **HIGH** | Deve essere HIGH per attivare il Motore 2. |
| **M2ENB** | Enable Bar M2| **GPIO 7** | **LOW** | Deve essere LOW per attivare il Motore 2. |
| **DIAG / 13** | Fault Signal | **GPIO 14** | Input (Pull-up)| Se va a **LOW**, c'è un errore (Overheat/Short). |
| **M1CS (A0)** | Current M1 | **GPIO 10** | Analog In | Legge la corrente di M1 (~500mV/A). |
| **M2CS (A1)** | Current M2 | **GPIO 11** | Analog In | Legge la corrente di M2 (~500mV/A). |

---

### Note Tecniche per il Cablaggio:

1.  **I due Enable (EN e ENB):**
    *   Affinché il driver funzioni, l'ESP32 deve "tenere premuti" entrambi i pin virtualmente: **EN alto** e **ENB basso**.
    *   Nel codice, la funzione `md.init()` si occupa di impostare GPIO 42 e 6 su HIGH, e GPIO 41 e 7 su LOW. Se lo fai manualmente, ricorda questa inversione.

2.  **Pin 13 (DIAG):**
    *   Sulla tua scheda lo trovi etichettato come **DIAG** o semplicemente sul pin **13** della fila laterale nera. È un segnale "comune": se uno dei due motori fallisce, questo pin scende a 0V.

3.  **IOREF (Attenzione):**
    *   Non dimenticare mai il ponte tra il pin **3.3V dell'ESP32-S3** e il pin **IOREF** dello shield. Senza questo, i segnali digitali potrebbero non essere interpretati correttamente o, peggio, lo shield potrebbe tentare di comunicare a 5V.

4.  **Alimentazione (VCC vs VIN):**
    *   **VCC (Pin header):** Va al 5V dell'ESP32 (alimentazione chip).
    *   **VIN (Morsetti blu):** Va al positivo della batteria (alimentazione motori).

### 2.2 Collegamenti di Potenza (Morsetti Blu)
*   **M1A / M1B**: Cavi Motore 1.
*   **M2A / M2B**: Cavi Motore 2.
*   **VIN**: Polo Positivo Batteria (+).
*   **GND**: Polo Negativo Batteria (-).

---

## 3. Setup Software (PlatformIO)

Dato che la libreria ufficiale non è sempre reperibile via registry, usiamo il link diretto GitHub.

### 3.1 `platformio.ini`
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi ; Indispensabile per N16R8
lib_deps =
    https://github.com/pololu/dual-tb9051ftg-motor-shield.git
```

---

## 4. API Reference (Metodi Principali)

| Metodo | Argomenti | Descrizione |
| :--- | :--- | :--- |
| `init()` | Nessuno | Configura i pin e i timer PWM. |
| `setM1Speed(speed)` | `int16_t` (-400 a 400) | Imposta velocità motore 1. |
| `setM2Speed(speed)` | `int16_t` (-400 a 400) | Imposta velocità motore 2. |
| `setSpeeds(s1, s2)` | `int16_t, int16_t` | Imposta entrambi contemporaneamente. |
| `getM1CurrentMilliamps()`| Nessuno | Ritorna il consumo di M1 in mA. |
| `getFault()` | Nessuno | Ritorna `true` se DIAG/Pin 13 è LOW (Errore). |

---

## 5. Esempio di Codice Professionale (Non-Blocking)

Questo esempio gestisce i motori, monitora i guasti e legge la corrente senza mai fermare l'esecuzione (`no delay`).

```c++
#include <Arduino.h>
#include <DualTB9051FTGShield.h>

// Definizione Pinout ESP32-S3
#define M1EN 42
#define M1ENB -1 // Non usato, collegato a GND hardware
#define M1PWM 1
#define M1DIR 2
#define M2EN 6
#define M2ENB -1 // Non usato, collegato a GND hardware
#define M2PWM 4
#define M2DIR 5
#define M1CS 10
#define M2CS 11
#define nFAULT 14

DualTB9051FTGShield md(M1EN, M1ENB, M1PWM, M1DIR, M2EN, M2ENB, M2PWM, M2DIR, M1CS, M2CS, nFAULT);

unsigned long lastUpdate = 0;
const int updateInterval = 100; // 10 Hz

void setup() {
    Serial.begin(115200);
    md.init();
    Serial.println("Driver TB9051FTG Inizializzato...");
}

void loop() {
    if (millis() - lastUpdate >= updateInterval) {
        lastUpdate = millis();

        // 1. Controllo Errori
        if (md.getFault()) {
            Serial.println("!!! ERRORE CRITICO RILEVATO !!!");
            md.setSpeeds(0, 0);
            return; 
        }

        // 2. Lettura Telemetria
        unsigned int currentM1 = md.getM1CurrentMilliamps();
        unsigned int currentM2 = md.getM2CurrentMilliamps();

        // 3. Esempio logica di movimento (rampa fittizia)
        static int speed = 0;
        static int step = 10;
        speed += step;
        if (abs(speed) >= 400) step *= -1;

        md.setSpeeds(speed, speed);

        Serial.printf("Speed: %d | M1: %u mA | M2: %u mA\n", speed, currentM1, currentM2);
    }
}
```

---

## 6. Best Practices & Risoluzione Problemi

### 6.1 Problemi Comuni
*   **I motori fischiano ma non girano:** La batteria è scarica o il pin `VIN` non è collegato bene. Controlla che `IOREF` sia a 3.3V.
*   **DIAG è sempre attivo (LOW):** Potrebbe esserci un cortocircuito sui cavi del motore o la tensione batteria è sotto i 4.5V.
*   **Lettura Corrente Instabile:** L'ADC dell'ESP32-S3 è rumoroso. Se i mA saltano troppo, usa una media mobile:
    ```c++
    float filteredCurrent = (oldValue * 0.9) + (newValue * 0.1);
    ```

### 6.2 Ottimizzazione per Robotica
*   **Frequenza PWM:** Se senti un fischio fastidioso, puoi modificare la frequenza PWM. La libreria Pololu usa i default di sistema, ma su ESP32 puoi usare `ledcSetup` per forzare 20kHz se necessario.
*   **Braking (Frenata):** Impostando la velocità a `0`, il driver mette in corto i motori (frenata rigida). Per lasciarli scorrere (coast), devi portare `M1EN` e `M2EN` a `LOW`.

### 6.3 Note sul Pin 13 / DIAG
Nello shield Pololu, il LED integrato sul pin 13 potrebbe interferire con il segnale di Fault se usato con logica a 3.3V molto sensibile. Se riscontri letture errate di Fault, scollega il pin 13 e usa direttamente i pin **M1DIAG** o **M2DIAG** sull'header laterale.