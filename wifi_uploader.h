// ============================================================
// wifi_uploader.h — Auto-connect WiFi e FTP Upload (NAS)
// ============================================================
#ifndef WIFI_UPLOADER_H
#define WIFI_UPLOADER_H

#include <WiFi.h>
#include <WiFiClient.h>
#include "config.h"
#include "data_store.h"
#include "data_logger.h"

// Função auxiliar para ler resposta do FTP
int readFTPResponse(WiFiClient& client, String& responseStr) {
    long timeout = millis() + 5000;
    responseStr = "";
    while (!client.available() && millis() < timeout) { delay(10); }
    if (!client.available()) return -1;
    
    while (client.available()) {
        String line = client.readStringUntil('\n');
        responseStr += line + "\n";
        if (line.length() >= 3 && isDigit(line[0]) && isDigit(line[1]) && isDigit(line[2]) && line[3] == ' ') {
            return line.substring(0, 3).toInt();
        }
    }
    return -1;
}

bool ftpUploadFile(String localPath, String remoteDir) {
    WiFiClient cmdClient;
    WiFiClient dataClient;
    String response;
    int code;

    Serial.print(F("[FTP] Conectando ao NAS... ")); Serial.println(FTP_HOST);
    if (!cmdClient.connect(FTP_HOST, FTP_PORT)) {
        Serial.println(F("[FTP] Falha ao conectar"));
        return false;
    }

    code = readFTPResponse(cmdClient, response);
    if (code != 220) goto fail;

    cmdClient.print("USER "); cmdClient.println(FTP_USER);
    code = readFTPResponse(cmdClient, response);
    if (code != 331) goto fail;

    cmdClient.print("PASS "); cmdClient.println(FTP_PASSWORD);
    code = readFTPResponse(cmdClient, response);
    if (code != 230) goto fail;

    cmdClient.print("CWD "); cmdClient.println(remoteDir);
    code = readFTPResponse(cmdClient, response);
    if (code != 250) {
        // Tenta criar diretório
        cmdClient.print("MKD "); cmdClient.println(remoteDir);
        readFTPResponse(cmdClient, response);
        cmdClient.print("CWD "); cmdClient.println(remoteDir);
        code = readFTPResponse(cmdClient, response);
        if (code != 250) goto fail;
    }

    cmdClient.println("TYPE I");
    code = readFTPResponse(cmdClient, response);
    if (code != 200) goto fail;

    cmdClient.println("PASV");
    code = readFTPResponse(cmdClient, response);
    if (code != 227) goto fail;

    // Parse PASV IP and Port
    {
        int pasvStart = response.indexOf('(');
        int pasvEnd = response.indexOf(')');
        if (pasvStart == -1 || pasvEnd == -1) goto fail;
        
        String pasvStr = response.substring(pasvStart + 1, pasvEnd);
        int commas[5];
        int cIdx = 0;
        for (int i=0; i<pasvStr.length() && cIdx<5; i++) {
            if (pasvStr[i] == ',') commas[cIdx++] = i;
        }
        if (cIdx != 5) goto fail;

        String ip = pasvStr.substring(0, commas[0]) + "." + 
                    pasvStr.substring(commas[0]+1, commas[1]) + "." +
                    pasvStr.substring(commas[1]+1, commas[2]) + "." +
                    pasvStr.substring(commas[2]+1, commas[3]);
        
        int p1 = pasvStr.substring(commas[3]+1, commas[4]).toInt();
        int p2 = pasvStr.substring(commas[4]+1).toInt();
        int port = (p1 * 256) + p2;

        if (!dataClient.connect(ip.c_str(), port)) {
            Serial.println(F("[FTP] Falha na conexão de dados (PASV)"));
            goto fail;
        }
    }

    // Pega o nome do arquivo (tira o /logs/)
    {
        String fileName = localPath;
        int lastSlash = localPath.lastIndexOf('/');
        if (lastSlash >= 0) fileName = localPath.substring(lastSlash + 1);
        
        cmdClient.print("STOR "); cmdClient.println(fileName);
        code = readFTPResponse(cmdClient, response);
        if (code != 150) goto fail;
    }

    // Envia o arquivo
    {
        File file = LittleFS.open(localPath, "r");
        if (!file) goto fail;
        
        Serial.print(F("[FTP] Enviando arquivo... "));
        uint8_t buf[512];
        while (file.available()) {
            size_t bytes = file.read(buf, sizeof(buf));
            dataClient.write(buf, bytes);
        }
        file.close();
        dataClient.stop();
        Serial.println(F("Done."));
    }

    code = readFTPResponse(cmdClient, response);
    cmdClient.println("QUIT");
    readFTPResponse(cmdClient, response);
    cmdClient.stop();
    
    return (code == 226); // Transfer complete

fail:
    Serial.println(F("[FTP] Erro na transferência:"));
    Serial.println(response);
    cmdClient.println("QUIT");
    cmdClient.stop();
    dataClient.stop();
    return false;
}

void syncNTPTime() {
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // BRT (UTC-3)
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
        Serial.println(F("[WIFI] NTP Sync OK"));
    }
}

void initWiFiUploader() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    carData.wifiConnected = false;
    carData.uploading = false;
}

void checkWiFiAndUpload() {
    // Se não há arquivos, desconecta pra poupar bateria
    if (!hasPendingLogs()) {
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
            carData.wifiConnected = false;
            Serial.println(F("[WIFI] Nenhum log pendente. WiFi desligado."));
        }
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[WIFI] Tentando reconexão..."));
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
            delay(100);
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("[WIFI] Conectado!"));
            carData.wifiConnected = true;
            syncNTPTime();
        } else {
            Serial.println(F("[WIFI] Não encontrou rede."));
            carData.wifiConnected = false;
            return;
        }
    }

    // Processa upload de 1 arquivo por vez
    String path = getNextPendingLogPath();
    if (path != "") {
        carData.uploading = true;
        flushLog(); // Garante que o que está na RAM foi para o arquivo atual
        
        if (ftpUploadFile(path, FTP_UPLOAD_PATH)) {
            markLogUploaded(path);
        }
        carData.uploading = false;
    }
}

#endif // WIFI_UPLOADER_H
