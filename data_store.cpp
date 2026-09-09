// ============================================================
// data_store.cpp — Definições da CarData e Mutex
// ============================================================
#include "data_store.h"

// ============================================================
// Instância global (única definição — ODR compliant)
// ============================================================
CarData carData;
SemaphoreHandle_t dataMutex = NULL;

// ============================================================
// Inicialização
// ============================================================
void initDataStore() {
    dataMutex = xSemaphoreCreateMutex();
    memset(&carData, 0, sizeof(CarData));
    carData.gear = 0;
    carData.currentScreen = 0;
    carData.loggingActive = true;
    carData.trackMode = true;       // Boot em Track Mode (WiFi OFF)
    for (int i = 0; i < 8; i++) {
        carData.egt[i] = 0.0f;
        carData.egtError[i] = false;
    }
}

// ============================================================
// Acesso Thread-Safe
// ============================================================

CarData readCarData() {
    CarData snapshot;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        snapshot = carData;
        xSemaphoreGive(dataMutex);
    } else {
        // Fallback: retorna zerado se não conseguir o mutex
        memset(&snapshot, 0, sizeof(CarData));
    }
    return snapshot;
}

bool lockCarData(uint32_t timeoutMs) {
    return xSemaphoreTake(dataMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void unlockCarData() {
    xSemaphoreGive(dataMutex);
}

void advanceScreen(int direction) {
    if (lockCarData()) {
        int s = (int)carData.currentScreen + direction;
        if (s < 0) s = NUM_SCREENS - 1;
        if (s >= NUM_SCREENS) s = 0;
        carData.currentScreen = (uint8_t)s;
        unlockCarData();
    }
}

void resetTripAverage() {
    if (lockCarData()) {
        carData.totalDistanceKm = 0.0f;
        carData.totalFuelLiters = 0.0f;
        carData.avgKmL = 0.0f;
        carData.tripResetFlag = true;
        carData.tripResetTimestamp = millis();
        unlockCarData();
    }
    Serial.println(F("[TRIP] Média de consumo zerada"));
}

// ============================================================
// Persistência NVS (Preferences) para Trip Data
// ============================================================
#include <Preferences.h>
static Preferences tripNVS;

void saveTripToNVS() {
    tripNVS.begin("trip", false);
    tripNVS.putFloat("distKm", carData.totalDistanceKm);
    tripNVS.putFloat("fuelL",  carData.totalFuelLiters);
    tripNVS.end();
    Serial.println(F("[NVS] Trip salvo"));
}

void loadTripFromNVS() {
    tripNVS.begin("trip", true);
    float dist = tripNVS.getFloat("distKm", 0.0f);
    float fuel = tripNVS.getFloat("fuelL",  0.0f);
    tripNVS.end();

    if (lockCarData()) {
        carData.totalDistanceKm = dist;
        carData.totalFuelLiters = fuel;
        if (fuel > 0.001f) {
            carData.avgKmL = dist / fuel;
        }
        unlockCarData();
    }
    Serial.printf("[NVS] Trip restaurado: %.1f km, %.1f L\n", dist, fuel);
}

// ============================================================
// Helpers de Decodificação
// ============================================================

int16_t decodeS16BE(uint8_t hi, uint8_t lo) {
    return (int16_t)((hi << 8) | lo);
}

uint16_t decodeU16BE(uint8_t hi, uint8_t lo) {
    return (uint16_t)((hi << 8) | lo);
}

const char* gearName(int8_t gear) {
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

bool hasActiveAlert(const CarData &d) {
    if (d.engineTemp > ENGINE_TEMP_WARNING) return true;
    if (d.oilPressure > 0 && d.oilPressure < OIL_PRESS_MIN && d.rpm > 1000) return true;
    if (d.oilTemp > OIL_TEMP_WARNING) return true;
    for (int i = 0; i < 8; i++) {
        if (d.egt[i] > EGT_WARNING && !d.egtError[i]) return true;
    }
    return false;
}
