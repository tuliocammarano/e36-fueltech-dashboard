// ============================================================
// data_logger.h — Logger CSV para LittleFS
// ============================================================
#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <LittleFS.h>
#include <time.h>
#include "config.h"
#include "data_store.h"

String currentLogFile = "";
char logBuffer[LOG_FLUSH_BYTES];
int logBufferLen = 0;
int logFileCount = 0;

void countLogFiles() {
    logFileCount = 0;
    File dir = LittleFS.open("/logs");
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".csv")) {
            logFileCount++;
        }
        file = dir.openNextFile();
    }
}

int getLogFileCount() {
    return logFileCount;
}

void checkStorageSpace() {
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    
    if (totalBytes == 0) return;
    
    float usagePct = ((float)usedBytes / totalBytes) * 100.0f;
    
    // Se o uso passou do limite ou o número de arquivos excedeu o máximo, apaga o mais antigo
    while (usagePct > MAX_FS_USAGE_PCT || logFileCount > MAX_LOG_FILES) {
        File dir = LittleFS.open("/logs");
        if (!dir) break;
        
        String oldestFile = "";
        time_t oldestTime = 2147483647; // MAX_INT
        
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory() && String(file.name()).endsWith(".csv")) {
                time_t fileTime = file.getLastWrite();
                if (fileTime < oldestTime) {
                    oldestTime = fileTime;
                    oldestFile = String(file.name());
                }
            }
            file = dir.openNextFile();
        }
        
        if (oldestFile != "") {
            String path = "/logs/" + oldestFile;
            Serial.print(F("[LOG] Limpando espaço. Apagando: ")); Serial.println(path);
            LittleFS.remove(path);
            logFileCount--;
        }
        
        usedBytes = LittleFS.usedBytes();
        usagePct = ((float)usedBytes / totalBytes) * 100.0f;
    }
}

void flushLog() {
    if (currentLogFile == "" || logBufferLen == 0) return;
    
    File file = LittleFS.open(currentLogFile, FILE_APPEND);
    if (file) {
        file.print(logBuffer);
        file.close();
    }
    logBuffer[0] = '\0';
    logBufferLen = 0;
}

void startNewLogFile() {
    flushLog(); // Salva qualquer coisa pendente
    checkStorageSpace();
    
    // Cria nome do arquivo baseado no tempo ou ms se NTP não estiver syncado
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buf[32];
        strftime(buf, sizeof(buf), "/logs/log_%Y%m%d_%H%M%S.csv", &timeinfo);
        currentLogFile = String(buf);
    } else {
        currentLogFile = "/logs/log_" + String(millis()) + ".csv";
    }
    
    File file = LittleFS.open(currentLogFile, FILE_WRITE);
    if (file) {
        file.println("timestamp_ms,rpm,tps,map_kpa,air_temp,engine_temp,oil_press,fuel_press,water_press,gear,exhaust_o2,oil_temp,inj_time_a,inj_time_b,lambda_corr,fuel_flow,trans_temp,fuel_cons,brake_press,egt1,egt2,egt3,egt4,egt5,egt6,egt7,egt8");
        file.close();
        logFileCount++;
        Serial.print(F("[LOG] Novo arquivo de log iniciado: ")); Serial.println(currentLogFile);
    } else {
        Serial.println(F("[LOG] Falha ao criar arquivo de log!"));
        currentLogFile = "";
    }
}

void initLogger() {
    if (!LittleFS.begin(true)) {
        Serial.println(F("[LOG] Erro ao montar LittleFS"));
        return;
    }
    
    if (!LittleFS.exists("/logs")) {
        LittleFS.mkdir("/logs");
    }
    
    countLogFiles();
    startNewLogFile();
}

void logData() {
    if (!carData.loggingActive || !carData.canActive || currentLogFile == "") return;
    
    char line[256];
    snprintf(line, sizeof(line), 
             "%lu,%u,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%d,%.2f,%.1f,%.2f,%.2f,%.2f,%.1f,%.1f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
             millis(), carData.rpm, carData.tps, carData.map, carData.airTemp, carData.engineTemp,
             carData.oilPressure, carData.fuelPressure, carData.waterPressure, carData.gear,
             carData.exhaustO2, carData.oilTemp, carData.injTimeA, carData.injTimeB,
             carData.lambdaCorrection, carData.fuelFlowTotal, carData.transTemp,
             carData.fuelConsumption, carData.brakePressure,
             carData.egt[0], carData.egt[1], carData.egt[2], carData.egt[3], 
             carData.egt[4], carData.egt[5], carData.egt[6], carData.egt[7]);
             
    int lineLen = strlen(line);
    
    // Se não cabe no buffer, dá flush primeiro
    if (logBufferLen + lineLen >= LOG_FLUSH_BYTES) {
        flushLog();
    }
    
    strcat(logBuffer, line);
    logBufferLen += lineLen;
}

bool hasPendingLogs() {
    return (logFileCount > 0);
}

String getNextPendingLogPath() {
    File dir = LittleFS.open("/logs");
    if (!dir) return "";
    
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".csv")) {
            String path = "/logs/" + String(file.name());
            // Não faz upload do arquivo que está sendo escrito agora
            if (path != currentLogFile) {
                return path;
            }
        }
        file = dir.openNextFile();
    }
    return "";
}

void markLogUploaded(String path) {
    if (LittleFS.remove(path)) {
        logFileCount--;
        Serial.print(F("[LOG] Upload completo. Arquivo removido: ")); Serial.println(path);
    }
}

#endif // DATA_LOGGER_H
