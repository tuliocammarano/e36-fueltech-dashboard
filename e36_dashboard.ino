// ============================================================
// e36_dashboard.ino — Main File
// ============================================================
#include <Arduino.h>

// Instância global dos dados do carro (definida como extern nos headers)
#include "data_store.h"
CarData carData;

// Inclui os módulos (a ordem é importante devido a dependências)
#include "config.h"
#include "can_reader.h"
#include "display_manager.h"
#include "data_logger.h"
#include "wifi_uploader.h"
#include "bt_controller.h"

// Timers para multitarefa cooperativa
unsigned long lastDisplayMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastWifiMs = 0;
unsigned long lastBleMs = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println(F("\n\n============================"));
    Serial.println(F(" E36 Dashboard v2.0 - Boot "));
    Serial.println(F("============================"));

    // 1. Inicializa o banco de dados interno
    initCarData();

    // 2. Inicializa o Display (Mostra logo de boot)
    initDisplay();

    // 3. Inicializa CAN Bus
    initCAN();

    // 4. Inicializa o LittleFS e Data Logger
    initLogger();

    // 5. Inicializa WiFi Uploader (em modo desconectado no boot)
    initWiFiUploader();

    // 6. Inicializa BLE (Para comunicação com o celular)
    initBLE();
    
    Serial.println(F("[SYS] Boot concluído. Entrando no loop principal."));
}

void loop() {
    unsigned long currentMs = millis();

    // 1. CAN Read - Polling contínuo (Prioridade máxima)
    // O buffer do MCP2515 tem apenas 2 mensagens, precisamos esvaziar rápido
    readCAN();

    // 2. BLE Check - Verifica comandos do app (100ms)
    if (currentMs - lastBleMs >= BLE_CHECK_MS) {
        lastBleMs = currentMs;
        checkBLE();
    }

    // 3. Display Update - 20 FPS (50ms)
    if (currentMs - lastDisplayMs >= DISPLAY_UPDATE_MS) {
        lastDisplayMs = currentMs;
        updateDisplay();
    }

    // 4. Data Logger - Salva no SD/LittleFS (1Hz)
    if (currentMs - lastLogMs >= LOG_INTERVAL_MS) {
        lastLogMs = currentMs;
        logData();
    }

    // 5. WiFi Uploader - Tenta reconectar e enviar logs pendentes (30s)
    if (currentMs - lastWifiMs >= WIFI_CHECK_MS) {
        lastWifiMs = currentMs;
        checkWiFiAndUpload();
    }
}
