// ============================================================
// ota_manager.h — Suporte a Atualizações via WiFi (Over-The-Air)
// ============================================================
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <WiFi.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "data_store.h"

bool otaInitialized = false;

void initOTA() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    carData.wifiConnected = false;
    Serial.println(F("[OTA] Iniciado em modo Standby"));
}

void checkWiFiForOTA() {
    // Só tenta conectar ao WiFi para o OTA se o motor estiver parado (RPM == 0)
    // Isso evita travamentos (lag) na tela e no CAN enquanto você dirige.
    if (carData.rpm > 0) {
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
            carData.wifiConnected = false;
        }
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[WIFI] Tentando conexao para OTA..."));
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        unsigned long start = millis();
        // Espera curta e não-bloqueante pesada
        while (WiFi.status() != WL_CONNECTED && millis() - start < 3000) {
            delay(50);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("[WIFI] Conectado! (OTA Pronto)"));
            carData.wifiConnected = true;
        } else {
            Serial.println(F("[WIFI] Nao encontrou rede."));
            carData.wifiConnected = false;
        }
    }
}

void handleOTA() {
    if (carData.wifiConnected) {
        if (!otaInitialized) {
            ArduinoOTA.setHostname("E36-Dashboard");
            ArduinoOTA.setPassword("21092009");
            ArduinoOTA.begin();
            otaInitialized = true;
            Serial.println(F("[OTA] Servico Pronto para Atualizacao via WiFi"));
        }
        ArduinoOTA.handle();
    }
}

#endif // OTA_MANAGER_H
