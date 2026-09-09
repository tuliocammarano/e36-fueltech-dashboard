// ============================================================
// ota_manager.h — OTA On-Demand (ativado via HMI)
// ============================================================
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

// Inicializa WiFi em OFF e configura callbacks OTA.
void initOTA();

// Inicia WiFi + ArduinoOTA de forma assíncrona (não-blocante).
// Chamado pelo HMI quando o usuário faz Long Press na tela OTA.
void startOTAMode();

// Processa OTA (chamar no loop apenas se ota_mode_active).
void handleOTA();

// Flag global: true enquanto modo OTA está ativo.
extern bool ota_mode_active;

#endif // OTA_MANAGER_H
