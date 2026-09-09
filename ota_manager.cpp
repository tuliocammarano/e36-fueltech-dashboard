// ============================================================
// ota_manager.cpp — OTA On-Demand (assíncrono, não-blocante)
// ============================================================
#include "ota_manager.h"
#include "data_store.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

// ============================================================
// Estado global
// ============================================================
bool ota_mode_active = false;

static bool otaInitialized = false;
static bool otaInProgress  = false;
static bool wifiStarted    = false;

// ============================================================
// setupOTAHandlers() — Callbacks (chamado 1x no init)
// ============================================================
static void setupOTAHandlers() {
    ArduinoOTA.setHostname("E36-Dashboard");
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.setPort(3232);

    ArduinoOTA.onStart([]() {
        otaInProgress = true;
        String type = (ArduinoOTA.getCommand() == U_FLASH)
                      ? "Firmware" : "SPIFFS";
        Serial.printf("\n[OTA] >>> Atualizando %s <<<\n", type.c_str());
        Serial.printf("[OTA] Free heap: %u bytes\n", ESP.getFreeHeap());
    });

    ArduinoOTA.onEnd([]() {
        otaInProgress = false;
        Serial.println(F("\n[OTA] >>> Atualização completa! Reiniciando... <<<"));
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static int lastPct = -1;
        int pct = (progress * 100) / total;
        if (pct != lastPct) {
            Serial.printf("[OTA] %d%%\r", pct);
            lastPct = pct;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        otaInProgress = false;
        Serial.printf("\n[OTA] ERRO [%u]: ", error);
        switch (error) {
            case OTA_AUTH_ERROR:    Serial.println("Auth falhou"); break;
            case OTA_BEGIN_ERROR:   Serial.println("Begin falhou"); break;
            case OTA_CONNECT_ERROR: Serial.println("Connect falhou"); break;
            case OTA_RECEIVE_ERROR: Serial.println("Receive falhou"); break;
            case OTA_END_ERROR:     Serial.println("End falhou"); break;
        }
    });
}

// ============================================================
// initOTA() — Boot com WiFi OFF, configura callbacks
// ============================================================
void initOTA() {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_OFF);

    setupOTAHandlers();

    Serial.printf("[OTA] Sketch: %u bytes, OTA space: %u bytes\n",
                  ESP.getSketchSize(), ESP.getFreeSketchSpace());
    Serial.println(F("[OTA] Modo On-Demand — WiFi OFF"));
}

// ============================================================
// startOTAMode() — Ativa WiFi + OTA sob demanda (assíncrono)
// ============================================================
// Não usa while/delay blocante. WiFi.begin() é assíncrono;
// handleOTA() verifica o status a cada ciclo do loop().
// ============================================================
void startOTAMode() {
    if (ota_mode_active) return;

    Serial.printf("[OTA] Free heap antes: %u bytes\n", ESP.getFreeHeap());

    // Libera ~30KB: desliga BT Classic (rádio compartilhado)
    btStop();
    Serial.println(F("[OTA] BT Classic desligado"));

    // Suspende CAN Task — telemetria desnecessária durante flash
    extern TaskHandle_t canTaskHandle;
    if (canTaskHandle != NULL) {
        vTaskSuspend(canTaskHandle);
        Serial.println(F("[OTA] CAN Task suspensa"));
    }

    Serial.printf("[OTA] Free heap após cleanup: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("[OTA] Conectando a '%s'...\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    wifiStarted    = true;
    otaInitialized = false;
    ota_mode_active = true;

    if (lockCarData()) {
        carData.trackMode = false;
        carData.btConnected = false;
        unlockCarData();
    }
}

// ============================================================
// handleOTA() — Loop não-blocante: verifica WiFi → inicia OTA
// ============================================================
void handleOTA() {
    if (!ota_mode_active) return;

    // Aguarda conexão WiFi de forma assíncrona (sem while)
    if (wifiStarted && !otaInitialized) {
        if (WiFi.status() == WL_CONNECTED) {
            ArduinoOTA.begin();
            otaInitialized = true;

            if (lockCarData()) {
                carData.wifiConnected = true;
                unlockCarData();
            }

            Serial.printf("[OTA] Pronto! IP: %s  RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
        return;
    }

    // Processa pacotes OTA
    if (otaInitialized) {
        ArduinoOTA.handle();
    }
}
