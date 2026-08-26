// ============================================================
// bt_controller.h — Bluetooth Clássico (Serial SPP)
// ============================================================
#ifndef BT_CONTROLLER_H
#define BT_CONTROLLER_H

#include "BluetoothSerial.h"
#include "config.h"
#include "data_store.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

extern void nextScreen();
extern void prevScreen();
extern void checkWiFiAndUpload();

void sendBLENotification(String msg) {
    if (SerialBT.hasClient()) {
        SerialBT.println(msg);
    }
}

void initBLE() {
    // Inicia o Bluetooth Clássico com o nome definido
    SerialBT.begin(BLE_DEVICE_NAME);
    Serial.println(F("[BT] Bluetooth Clássico iniciado!"));
}

void checkBLE() {
    carData.bleConnected = SerialBT.hasClient();

    if (SerialBT.available()) {
        String cmd = SerialBT.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        Serial.print(F("[BT] Comando recebido: "));
        Serial.println(cmd);

        if (cmd == "NEXT") {
            nextScreen();
            sendBLENotification("SCREEN:" + String(carData.currentScreen));
        } 
        else if (cmd == "PREV") {
            prevScreen();
            sendBLENotification("SCREEN:" + String(carData.currentScreen));
        }
        else if (cmd.startsWith("SCREEN:")) {
            int s = cmd.substring(7).toInt();
            if (s >= 0 && s < NUM_SCREENS) {
                carData.currentScreen = s;
                sendBLENotification("SCREEN:" + String(carData.currentScreen));
            }
        }
        else if (cmd == "LOG:START") {
            carData.loggingActive = true;
            sendBLENotification("LOG:ON");
        }
        else if (cmd == "LOG:STOP") {
            carData.loggingActive = false;
            sendBLENotification("LOG:OFF");
        }
        else if (cmd == "UPLOAD") {
            sendBLENotification("UPLOAD:STARTED");
            checkWiFiAndUpload(); // Força tentativa de upload
        }
        else if (cmd == "STATUS") {
            char json[256];
            snprintf(json, sizeof(json), 
                     "{\"rpm\":%u,\"map\":%.1f,\"tps\":%.1f,\"gear\":%d,\"engTemp\":%.1f,\"oilTemp\":%.1f,\"log\":%d,\"wifi\":%d,\"can\":%d}",
                     carData.rpm, carData.map, carData.tps, carData.gear, 
                     carData.engineTemp, carData.oilTemp, 
                     carData.loggingActive, carData.wifiConnected, carData.canActive);
            sendBLENotification(String(json));
        }
    }
}

#endif // BT_CONTROLLER_H
