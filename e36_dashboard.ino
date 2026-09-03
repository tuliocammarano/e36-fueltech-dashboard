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
#include "bt_controller.h"
#include "ota_manager.h"

// Timers para multitarefa cooperativa
unsigned long lastDisplayMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastWifiMs = 0;
unsigned long lastBleMs = 0;
unsigned long lastBtnNextMs = 0;
unsigned long lastBtnPrevMs = 0;
unsigned long lastCanSendMs = 0;

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

    // 4. Inicializa OTA (Modo Standby)
    initOTA();

    // 5. Inicializa BLE (Para streaming e celular)
    initBLE();
    
    // 6. Configura Botões Físicos
    pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
    pinMode(BTN_PREV_PIN, INPUT_PULLUP);
    
    // 7. Força a primeira checagem de WiFi agora mesmo no boot 
    // (Ajuda o Arduino IDE a achar a placa via mDNS mais rápido)
    checkWiFiForOTA();
    
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

    // Transmite o estado do painel virtual para a FuelTech (100Hz = 10ms)
    // O amigo usou 10ms no EGT, então vamos igualar a frequência!
    if (currentMs - lastCanSendMs >= 10) { 
        lastCanSendMs = currentMs;
        sendVirtualButtons();
    }

    // 2.5 Botões Físicos (Debounce 300ms)
    if (digitalRead(BTN_NEXT_PIN) == LOW && currentMs - lastBtnNextMs > 300) {
        lastBtnNextMs = currentMs;
        nextScreen();
    }
    if (digitalRead(BTN_PREV_PIN) == LOW && currentMs - lastBtnPrevMs > 300) {
        lastBtnPrevMs = currentMs;
        prevScreen();
    }

    // 3. Display Update - 20 FPS (50ms)
    if (currentMs - lastDisplayMs >= DISPLAY_UPDATE_MS) {
        lastDisplayMs = currentMs;
        updateDisplay();
    }

    // 4. Bluetooth Data Streamer (20Hz)
    if (currentMs - lastLogMs >= BT_LOG_INTERVAL_MS) {
        lastLogMs = currentMs;
        if (carData.loggingActive) {
            streamLogToBT();
        }
    }

    // 5. Verificador WiFi para OTA (Apenas quando motor parar)
    if (currentMs - lastWifiMs >= WIFI_CHECK_MS) {
        lastWifiMs = currentMs;
        checkWiFiForOTA();
    }

    // 6. OTA Check (Precisa rodar rápido e contínuo)
    handleOTA();
}
