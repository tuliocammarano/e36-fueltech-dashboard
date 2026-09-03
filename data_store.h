// ============================================================
// data_store.h — Estrutura centralizada de dados do veículo
// ============================================================
#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <Arduino.h>
#include "config.h"

// Estrutura com todos os dados decodificados do barramento CAN
struct CarData {
    // ---- Simplified Packet 0x14080600 ----
    float tps;              // Throttle Position (%)
    float map;              // Manifold Absolute Pressure (kPa) — convertido de bar
    float airTemp;          // Temperatura do ar (°C)
    float engineTemp;       // Temperatura do motor (°C)

    // ---- Simplified Packet 0x14080601 ----
    float oilPressure;      // Pressão do óleo (bar)
    float fuelPressure;     // Pressão de combustível (bar)
    float waterPressure;    // Pressão da água (bar)
    int8_t gear;            // Marcha (-2=Park, -1=Ré, 0=Neutro, 1-10=marchas)

    // ---- Simplified Packet 0x14080602 ----
    float exhaustO2;        // Lambda do escapamento
    uint16_t rpm;           // Rotação do motor
    float oilTemp;          // Temperatura do óleo (°C) — vem no 0x602

    // ---- Simplified Packet 0x14080603 ----
    float advance;          // Avanço de Ignição (graus)

    // ---- Simplified Packet 0x14080606 ----
    float speed;            // Velocidade (km/h)
    float speed1;           // Debug: 0x605 Right
    float speed2;           // Debug: 0x605 Left
    float speed3;           // Debug: 0x606 Driven

    // ---- Simplified Packet 0x14080604 ----
    // Novas métricas extraídas dos pacotes fragmentados (0x0FF, 0x1FF...)
    float dutyA;              // %
    float dutyB;              // %
    uint8_t fanState;         // 0 = Off, 1 = On
    float wgPressure;         // Bar
    float diffFuelPressure;   // Bar
    uint16_t genericOutputs;  // Bitmask de saídas genéricas (Para o VANOS)
    float battery;          // Tensão da Bateria (V)
    uint8_t twoStepState;     // 0 = Off, 1 = On
    uint8_t threeStepState;   // 0 = Off, 1 = On

    // ---- Simplified Packet 0x14080607 ----
    float lambdaCorrection; // Correção de lambda
    float fuelFlowTotal;    // Fluxo total de combustível (L/min)
    float injTimeA;         // Tempo de injeção banco A (ms)
    float injTimeB;         // Tempo de injeção banco B (ms)

    // ---- Simplified Packet 0x14080608 ----
    float transTemp;        // Temperatura da transmissão (°C)
    float fuelConsumption;  // Consumo de combustível
    float brakePressure;    // Pressão do freio (bar)

    // ---- EGT (Arduino emulando EGT-4 FuelTech) ----
    float egt[8];           // Temperatura EGT canais 1-8 (°C)
    bool  egtError[8];      // true = termopar desconectado (leitura 1050°C)

    // ---- Metadata e Controles Externos ----
    unsigned long lastCanUpdateMs;  // Último timestamp de mensagem CAN
    bool canActive;                 // CAN bus respondendo?
    uint8_t switchState;            // Estado dos botões virtuais (Bitmask)

    // ---- Estado do sistema ----
    uint8_t currentScreen;          // Tela atual do display (0 a NUM_SCREENS-1)
    bool loggingActive;             // Logging habilitado?
    bool bleConnected;              // BLE conectado?
    bool wifiConnected;             // WiFi conectado?
    bool uploading;                 // Upload em andamento?
};

// Instância global dos dados
extern CarData carData;

// Inicializa todos os campos com valores padrão
inline void initCarData() {
    memset(&carData, 0, sizeof(CarData));
    carData.gear = 0;           // Neutro
    carData.currentScreen = 0;  // Primeira tela
    carData.loggingActive = true; // Log ativo por padrão
    
    // Inicializa EGTs com 0 e sem erro
    for (int i = 0; i < 8; i++) {
        carData.egt[i] = 0.0f;
        carData.egtError[i] = false;
    }
}

// Helper: decodifica signed 16-bit big-endian de 2 bytes
inline int16_t decodeS16BE(uint8_t highByte, uint8_t lowByte) {
    return (int16_t)((highByte << 8) | lowByte);
}

// Helper: decodifica unsigned 16-bit big-endian de 2 bytes
inline uint16_t decodeU16BE(uint8_t highByte, uint8_t lowByte) {
    return (uint16_t)((highByte << 8) | lowByte);
}

// Helper: nome da marcha para display
inline const char* gearName(int8_t gear) {
    switch (gear) {
        case -2: return "P";
        case -1: return "R";
        case  0: return "N";
        default:
            static char buf[3];
            snprintf(buf, sizeof(buf), "%d", gear);
            return buf;
    }
}

// Helper: verifica se algum alerta está ativo
inline bool hasActiveAlert() {
    if (carData.engineTemp > ENGINE_TEMP_WARNING) return true;
    if (carData.oilPressure > 0 && carData.oilPressure < OIL_PRESS_MIN && carData.rpm > 1000) return true;
    if (carData.oilTemp > OIL_TEMP_WARNING) return true;
    for (int i = 0; i < 8; i++) {
        if (carData.egt[i] > EGT_WARNING && !carData.egtError[i]) return true;
    }
    return false;
}

#endif // DATA_STORE_H
