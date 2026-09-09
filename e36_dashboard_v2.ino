// ============================================================
// E36 FuelTech Dashboard & Gateway v2.0
// ============================================================
//
// Arquitetura Dual-Core FreeRTOS:
//
//   Core 1 (Alta Prioridade):
//     CAN Task — Polling assíncrono do MCP2515, decoder FTCAN 2.0,
//     cálculo de consumo com EMA, emulação SwitchPanel.
//
//   Core 0 (Menor Prioridade):
//     HMI Task — Display OLED SSD1305 128x32 (~20 FPS),
//     Bluetooth Classic SPP (RealDash Gateway), botões físicos.
//     Loop Arduino — ArduinoOTA, gerenciamento WiFi Track/Pit Mode.
//
//   Sincronização:
//     CarData (Single Source of Truth) protegida por FreeRTOS Mutex.
//     CAN Task escreve → HMI Task e Loop leem via snapshot.
//
// Hardware:
//   ESP32 DevKit V1 (WROOM, 4MB Flash)
//   MCP2515 + TJA1050 (Hardware SPI: GPIO 18/19/23/5)
//   OLED SSD1305 128x32 (Software SPI: GPIO 13/14/15/27/33)
//   2x Push Buttons (GPIO 26/25)
//
// Protocolo CAN: FTCAN 2.0 (FuelTech proprietário) @ 1 Mbps
// Protocolo BT:  RealDash CAN (Protocol 44 — Binary Native)
//
// ============================================================

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config.h"
#include "data_store.h"
#include "can_reader.h"
#include "display_manager.h"
#include "bt_controller.h"
#include "ota_manager.h"

// ============================================================
// Task Handles
// ============================================================
TaskHandle_t canTaskHandle = NULL;
TaskHandle_t hmiTaskHandle = NULL;

// ============================================================
// CAN Task — Core 1 (Alta Prioridade)
// ============================================================
void canTask(void *parameter) {
    unsigned long lastButtonSendMs = 0;

    for (;;) {
        processCANMessages();

        unsigned long now = millis();
        if (now - lastButtonSendMs >= 100) {
            lastButtonSendMs = now;
            sendVirtualButtons();
        }

        vTaskDelay(1);
    }
}

// ============================================================
// HMI Task — Core 0 (Menor Prioridade)
// ============================================================
void hmiTask(void *parameter) {
    unsigned long lastDisplayMs  = 0;
    unsigned long lastBtStreamMs = 0;
    unsigned long lastBtCheckMs  = 0;
    static bool     btnNextDown    = false;
    static unsigned long btnPressAt = 0;

    for (;;) {
        unsigned long now = millis();

        bool btnState = (digitalRead(BTN_NEXT_PIN) == LOW);
        bool btnPrev  = (digitalRead(BTN_PREV_PIN) == LOW);

        // ---- Guarda OTA: qualquer botão → reboot ----
        if (ota_mode_active && (btnState || btnPrev)) {
            Serial.println(F("[OTA] Abortado pelo usuário — reiniciando..."));
            delay(100);
            ESP.restart();
        }

        if (btnState && !btnNextDown) {
            btnNextDown = true;
            btnPressAt  = now;
        }
        else if (!btnState && btnNextDown) {
            unsigned long held = now - btnPressAt;
            if (held > 1500) {
                CarData snap = readCarData();
                if (snap.currentScreen == 6) {
                    startOTAMode();
                } else {
                    resetTripAverage();
                }
            } else if (held > 50 && held < 1000) {
                advanceScreen(1);
            }
            btnNextDown = false;
        }

        if (now - lastDisplayMs >= DISPLAY_UPDATE_MS) {
            lastDisplayMs = now;
            updateDisplay();
        }

        if (now - lastBtStreamMs >= BT_LOG_INTERVAL_MS) {
            lastBtStreamMs = now;
            streamTelemetryBT();
        }

        if (now - lastBtCheckMs >= BLE_CHECK_MS) {
            lastBtCheckMs = now;
            checkBTCommands();
        }

        vTaskDelay(1);
    }
}

// ============================================================
// setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    lastCanActivityMs = millis();

    Serial.println(F("\n\n========================================"));
    Serial.println(F("   E36 Dashboard v2.1 — Dual-Core"));
    Serial.println(F("========================================"));

    esp_sleep_enable_ext0_wakeup((gpio_num_t)CAN_INT_WAKEUP_PIN, 0);
    Serial.printf("[BOOT] Wakeup source: GPIO %d (CAN INT)\n", CAN_INT_WAKEUP_PIN);

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println(F("[BOOT] Acordou via CAN interrupt!"));
    }

    initDataStore();
    loadTripFromNVS();
    Serial.println(F("[BOOT] DataStore + Mutex + NVS OK"));

    initCAN();

    xTaskCreatePinnedToCore(
        canTask, "CAN_Task", CAN_TASK_STACK, NULL,
        CAN_TASK_PRIORITY, &canTaskHandle, 1
    );
    Serial.println(F("[BOOT] CAN Task (Core 1) OK"));

    initDisplay();
    initBluetooth();
    initOTA();

    pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
    pinMode(BTN_PREV_PIN, INPUT_PULLUP);

    xTaskCreatePinnedToCore(
        hmiTask, "HMI_Task", HMI_TASK_STACK, NULL,
        HMI_TASK_PRIORITY, &hmiTaskHandle, 0
    );
    Serial.println(F("[BOOT] Todas as tasks iniciadas\n"));
}

// ============================================================
// loop() — OTA on-demand + Deep Sleep watchdog
// ============================================================
void loop() {
    unsigned long now = millis();

    if (ota_mode_active) {
        handleOTA();
    }

    // ---- Deep Sleep: 30s sem atividade CAN — bloqueado se OTA ativo ----
    if (!ota_mode_active &&
        (now - lastCanActivityMs) > DEEP_SLEEP_TIMEOUT_MS) {
        Serial.println(F("[POWER] 30s inativo — deep sleep..."));
        saveTripToNVS();
        powerSaveDisplay();
        prepareCANForSleep();
        rtc_gpio_pullup_en((gpio_num_t)CAN_INT_WAKEUP_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)CAN_INT_WAKEUP_PIN);
        delay(100);
        esp_deep_sleep_start();
    }

    delay(10);
}

