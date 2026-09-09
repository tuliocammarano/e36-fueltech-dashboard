// ============================================================
// data_store.h — Single Source of Truth (CarData + Mutex)
// ============================================================
// Struct global protegida por FreeRTOS Mutex.
// Escrita pelo CAN Task (Core 1), lida pelo HMI Task (Core 0).
// ============================================================
#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <Arduino.h>
#include "config.h"

// ============================================================
// Estrutura centralizada de dados do veículo
// ============================================================
struct CarData {
    // ---- Simplified Packet 0x14080600 ----
    float tps;              // Throttle Position (%)
    float map;              // Manifold Absolute Pressure (bar)
    float airTemp;          // Temperatura do ar (°C)
    float engineTemp;       // Temperatura do motor (°C)

    // ---- Simplified Packet 0x14080601 ----
    float oilPressure;      // Pressão do óleo (bar)
    float fuelPressure;     // Pressão de combustível (bar)
    float waterPressure;    // Pressão da água (bar)
    int8_t gear;            // Marcha (-2=P, -1=R, 0=N, 1-10)

    // ---- Simplified Packet 0x14080602 ----
    float exhaustO2;        // Lambda do escapamento
    uint16_t rpm;           // Rotação do motor
    float oilTemp;          // Temperatura do óleo (°C)

    // ---- Simplified Packet 0x14080603 ----
    float speed;            // Velocidade km/h (Wheel Speed RR)

    // ---- Segmented FTCAN 2.0 Packets ----
    float advance;          // Avanço de ignição (°)
    float dutyA;            // Duty Cycle Bank A (%)
    float wgPressure;       // Pressão da wastegate (bar)
    uint16_t twoStepState;  // Status do 2-Step (0 ou 1)
    float battery;          // Tensão da bateria (V)

    // ---- Simplified Packet 0x14080607 ----
    float lambdaCorrection; // Correção de lambda
    float fuelFlowTotal;    // Fluxo total (L/min — raw da ECU)
    float injTimeA;         // Tempo de injeção banco A (ms)
    float injTimeB;         // Tempo de injeção banco B (ms)

    // ---- Simplified Packet 0x14080608 ----
    float transTemp;        // Temperatura da transmissão (°C)
    float fuelConsumption;  // Consumo de combustível (raw ECU)
    float brakePressure;    // Pressão do freio (bar)

    // ---- EGT (Arduino emulando EGT-4 FuelTech) ----
    float egt[8];           // Temperatura EGT canais 1-8 (°C)
    bool  egtError[8];      // true = termopar desconectado

    // ---- Consumo Calculado (EMA filtrado) ----
    float fuelFlowLPH;      // Consumo instantâneo suavizado (L/h)
    float fuelEffKML;       // Eficiência (km/L)

    // ---- Metadata e Controles ----
    unsigned long lastCanUpdateMs;  // Timestamp da última msg CAN
    bool canActive;                 // CAN bus respondendo?
    uint8_t switchState;            // Bitmask dos botões virtuais

    // ---- Estado do Sistema ----
    uint8_t currentScreen;          // Tela atual (0 a NUM_SCREENS-1)
    bool loggingActive;             // Streaming BT ativo?
    bool btConnected;               // Bluetooth conectado?
    bool wifiConnected;             // WiFi conectado?
    bool trackMode;                 // true = WiFi OFF, BT full speed

    // ---- Trip Average (acumuladores — persistem até reset manual) ----
    float totalDistanceKm;          // Distância acumulada (km)
    float totalFuelLiters;          // Combustível acumulado (L)
    float avgKmL;                   // Média de consumo (km/L)
    bool  tripResetFlag;            // Feedback visual de "RESET" no display
    unsigned long tripResetTimestamp; // Quando o reset aconteceu
};

// ============================================================
// Instância global e Mutex (definidos em data_store.cpp)
// ============================================================
extern CarData carData;
extern SemaphoreHandle_t dataMutex;

// Inicialização (chamar no setup antes de criar tasks)
void initDataStore();

// Leitura thread-safe (retorna snapshot completo)
CarData readCarData();

// Lock/Unlock manual para escritas diretas
// Uso: if (lockCarData()) { carData.x = y; unlockCarData(); }
bool lockCarData(uint32_t timeoutMs = 10);
void unlockCarData();

// Navegação de tela (thread-safe)
void advanceScreen(int direction);

// Zera os acumuladores de trip average (thread-safe)
void resetTripAverage();

// Persistência NVS para trip data (deep sleep)
void saveTripToNVS();
void loadTripFromNVS();

// ============================================================
// Helpers de decodificação
// ============================================================
int16_t  decodeS16BE(uint8_t hi, uint8_t lo);
uint16_t decodeU16BE(uint8_t hi, uint8_t lo);
const char* gearName(int8_t gear);
bool hasActiveAlert(const CarData &d);

#endif // DATA_STORE_H
