// ============================================================
// display_manager.h — Gerenciador de Telas OLED (U8g2 128x32)
// ============================================================
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <SPI.h>
#include "config.h"
#include "data_store.h"

// Inicializa o display (U8g2 com o driver específico pro seu SSD1305)
U8G2_SSD1305_128X32_ADAFRUIT_F_4W_SW_SPI u8g2(U8G2_R2, OLED_CLK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RESET);

// Função auxiliar para desenhar a barra de status
void drawStatusBar() {
    int dotSpacing = 5;
    int startY = (SCREEN_HEIGHT - (NUM_SCREENS * dotSpacing)) / 2;
    for (int i = 0; i < NUM_SCREENS; i++) {
        if (i == carData.currentScreen) {
            u8g2.drawBox(126, startY + (i * dotSpacing), 2, 2);
        } else {
            u8g2.drawPixel(126, startY + (i * dotSpacing));
        }
    }

    u8g2.setFont(u8g2_font_5x7_tr);
    if (carData.bleConnected) {
        u8g2.setCursor(118, 7);
        u8g2.print("B");
    }
    if (carData.loggingActive && ((millis() / 500) % 2 == 0)) {
        u8g2.setCursor(118, 17);
        u8g2.print("R");
    }
}

// ============================================================
// Telas otimizadas para 128x32 e U8g2
// ============================================================

void drawScreen0_Principal() {
    u8g2.setFont(u8g2_font_helvB12_tr); 
    u8g2.setCursor(0, 14);
    u8g2.print(carData.rpm);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.print(" rpm");

    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(85, 14);
    u8g2.print(gearName(carData.gear));

    u8g2.setFont(u8g2_font_helvB08_tr); // Fonte um pouco mais gordinha e forte
    u8g2.setCursor(0, 31);
    u8g2.print("MAP:"); u8g2.print((int)carData.map);
    
    u8g2.setCursor(65, 31);
    u8g2.print("O2:"); u8g2.print(carData.exhaustO2, 2);
}

void drawScreen1_Temperaturas() {
    u8g2.setFont(u8g2_font_6x10_tr);
    
    u8g2.setCursor(0, 12);
    u8g2.print("MOT:"); u8g2.print((int)carData.engineTemp);
    
    u8g2.setCursor(60, 12);
    u8g2.print("OLE:"); u8g2.print((int)carData.oilTemp);

    u8g2.setCursor(0, 28);
    u8g2.print(" AR:"); u8g2.print((int)carData.airTemp);

    bool hasWarning = (carData.engineTemp > ENGINE_TEMP_WARNING || carData.oilTemp > OIL_TEMP_WARNING);
    if (hasWarning) {
        u8g2.setFont(u8g2_font_helvB10_tr);
        u8g2.setCursor(95, 28);
        u8g2.print("!");
    }
}

void drawScreen2_EGT() {
    u8g2.setFont(u8g2_font_5x7_tr);
    
    // 6 cilindros, numerados de 1 a 6
    // Linha 1: 1 a 3
    u8g2.setCursor(0, 12);
    for(int i=0; i<3; i++) {
        u8g2.print(i+1); u8g2.print(":");
        if(carData.egtError[i]) u8g2.print("ER  ");
        else { u8g2.print((int)carData.egt[i]); u8g2.print("  "); }
    }
    
    // Linha 2: 4 a 6
    u8g2.setCursor(0, 28);
    for(int i=3; i<6; i++) {
        u8g2.print(i+1); u8g2.print(":");
        if(carData.egtError[i]) u8g2.print("ER  ");
        else { u8g2.print((int)carData.egt[i]); u8g2.print("  "); }
    }
}

void drawScreen3_Injecao() {
    u8g2.setFont(u8g2_font_6x10_tr);
    
    u8g2.setCursor(0, 12);
    u8g2.print("INJ:"); u8g2.print(carData.injTimeA, 2);
    
    u8g2.setCursor(64, 12);
    u8g2.print("LCor:"); u8g2.print(carData.lambdaCorrection, 2);
    
    u8g2.setCursor(0, 28);
    u8g2.print("FLW:"); u8g2.print((int)carData.fuelFlowTotal);
    
    u8g2.setCursor(64, 28);
    u8g2.print("CNS:"); u8g2.print(carData.fuelConsumption, 1);
}

void drawScreen4_Pressoes() {
    u8g2.setFont(u8g2_font_6x10_tr);
    
    u8g2.setCursor(0, 12);
    u8g2.print("OLE:"); u8g2.print(carData.oilPressure, 1);
    
    u8g2.setCursor(64, 12);
    u8g2.print("CMB:"); u8g2.print(carData.fuelPressure, 1);

    u8g2.setCursor(0, 28);
    u8g2.print("H2O:"); u8g2.print(carData.waterPressure, 1);
}

void drawScreen5_Status() {
    u8g2.setFont(u8g2_font_6x10_tr);
    
    u8g2.setCursor(0, 12);
    u8g2.print("BT:"); u8g2.print(carData.bleConnected ? "ON" : "OFF");
    
    u8g2.setCursor(60, 12);
    u8g2.print("WIFI:"); u8g2.print(carData.wifiConnected ? "ON" : "OFF");

    u8g2.setCursor(0, 28);
    u8g2.print("LOG:"); u8g2.print(carData.loggingActive ? "ON" : "OFF");
    
    u8g2.setCursor(60, 28);
    u8g2.print("CAN:"); u8g2.print(carData.canActive ? "OK" : "OFF");
}

// ============================================================
// Funções Públicas
// ============================================================

void nextScreen() {
    carData.currentScreen = (carData.currentScreen + 1) % NUM_SCREENS;
}

void prevScreen() {
    if (carData.currentScreen == 0) {
        carData.currentScreen = NUM_SCREENS - 1;
    } else {
        carData.currentScreen--;
    }
}

void initDisplay() {
    pinMode(MCP2515_CS_PIN, OUTPUT);
    digitalWrite(MCP2515_CS_PIN, HIGH);

    u8g2.begin();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvB10_tr);
    u8g2.setCursor(0, 12);
    u8g2.print("E36 Dash");
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(0, 28);
    u8g2.print("Booting...");
    u8g2.sendBuffer();
    delay(1500);
}

void updateDisplay() {
    if (millis() - carData.lastCanUpdateMs > CAN_TIMEOUT_MS) {
        carData.canActive = false;
    }

    digitalWrite(MCP2515_CS_PIN, HIGH);
    u8g2.clearBuffer();

    if (!carData.canActive) {
        u8g2.setFont(u8g2_font_helvB12_tr);
        u8g2.setCursor(10, 20);
        u8g2.print("SEM SINAL CAN");
    } else {
        switch (carData.currentScreen) {
            case 0: drawScreen0_Principal(); break;
            case 1: drawScreen1_Temperaturas(); break;
            case 2: drawScreen2_EGT(); break;
            case 3: drawScreen3_Injecao(); break;
            case 4: drawScreen4_Pressoes(); break;
            case 5: drawScreen5_Status(); break;
            default: carData.currentScreen = 0; break;
        }
    }

    drawStatusBar();
    u8g2.sendBuffer();
}

#endif // DISPLAY_MANAGER_H
