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
    // Ícone do Bluetooth (Posição 1)
    if (carData.bleConnected) {
        u8g2.setCursor(118, 7);
        u8g2.print("B");
    }
    
    // Ícone 2-Step (Posição 2) - Lida pela ECU
    if (carData.twoStepState) {
        u8g2.setCursor(114, 15);
        u8g2.print("2S");
    } 
    
    // Ícone Anti-Lag (Posição 3) - Lido pelo estado do botão virtual (Bit 2)
    if (carData.switchState & (1 << 2)) {
        u8g2.setCursor(114, 23);
        u8g2.print("AL");
    }

    // Ícone EGT HOT (Posição 4)
    // Reduzido para 550°C para compensar o atraso de ~3 segundos do módulo EGT
    bool egtHot = false;
    for(int i=0; i<6; i++) {
        if (!carData.egtError[i] && carData.egt[i] >= 550.0f) {
            egtHot = true; break;
        }
    }
    
    if (egtHot) {
        u8g2.setCursor(114, 31);
        if ((millis() / 100) % 2 == 0) u8g2.print("!!!"); // Pisca rápido!
    } 
}

// ============================================================
// Telas otimizadas para 128x32 e U8g2
// ============================================================

void drawScreen0_Principal() {
    // --- LINHA SUPERIOR ---
    // RPM Bem grande
    u8g2.setFont(u8g2_font_helvB14_tr); 
    u8g2.setCursor(0, 14);
    u8g2.print(carData.rpm);
    // Legenda "rpm" pequena no pezinho
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.print("rpm");

    // Marcha gigante no canto superior direito
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(95, 14);
    u8g2.print(gearName(carData.gear));

    // --- LINHA INFERIOR ---
    // MAP Grande
    u8g2.setFont(u8g2_font_helvB12_tr); 
    u8g2.setCursor(0, 32);
    u8g2.print(carData.map, 2); // Ex: -0.60
    // Legenda "bar" pequena
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.print("bar");
    
    // Lambda Grande
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(50, 32);
    u8g2.print(carData.exhaustO2, 2);
    // Legenda "O2" pequena
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.print("o2");
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
    u8g2.print("Pnt:"); u8g2.print(carData.advance, 1);
    
    // Calcula Litros/Hora: (3 * RPM * Inj_ms * 994cc) / 60000 * 0.06 -> RPM * Inj_ms * 0.002982
    float lph = carData.rpm * carData.injTimeA * 0.002982f;
    
    u8g2.setCursor(64, 28);
    if (carData.speed > 3.0f) {
        u8g2.print("kmL:");
        if (lph > 0.5f) {
            float kml = carData.speed / lph;
            if (kml > 99.9f) kml = 99.9f;
            u8g2.print(kml, 1);
        } else {
            u8g2.print("99.9"); // Cut-off
        }
    } else {
        // Carro parado: Mostra L/h
        u8g2.print("L/h:");
        if (carData.rpm > 0) {
            u8g2.print(lph, 1);
        } else {
            u8g2.print("--");
        }
    }
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
    u8g2.print("BAT:"); u8g2.print(carData.battery, 1); u8g2.print("V");

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

static const unsigned char bmw_logo_bits[] U8X8_PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00,
  0x00, 0xfe, 0x7f, 0x00, 0x00, 0x4f, 0xf2, 0x00,
  0xc0, 0xc3, 0xe3, 0x03, 0xe0, 0x45, 0xa2, 0x07,
  0x70, 0x43, 0xe2, 0x0e, 0x30, 0xe5, 0xa0, 0x0c,
  0x18, 0xfb, 0x40, 0x18, 0x1c, 0xfc, 0x00, 0x38,
  0x0c, 0xfe, 0x00, 0x30, 0x0c, 0xff, 0x00, 0x30,
  0x06, 0xff, 0x00, 0x60, 0x86, 0xff, 0x00, 0x60,
  0x86, 0xff, 0x00, 0x60, 0x86, 0xff, 0x00, 0x60,
  0x06, 0x00, 0xff, 0x61, 0x06, 0x00, 0xff, 0x61,
  0x06, 0x00, 0xff, 0x61, 0x06, 0x00, 0xff, 0x60,
  0x0c, 0x00, 0xff, 0x30, 0x0c, 0x00, 0x7f, 0x30,
  0x1c, 0x00, 0x3f, 0x38, 0x18, 0x00, 0x1f, 0x18,
  0x30, 0x00, 0x07, 0x0c, 0x70, 0x00, 0x00, 0x0e,
  0xe0, 0x00, 0x00, 0x07, 0xc0, 0x03, 0xc0, 0x03,
  0x00, 0x0f, 0xf0, 0x00, 0x00, 0xfe, 0x7f, 0x00,
  0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void initDisplay() {
    pinMode(MCP2515_CS_PIN, OUTPUT);
    digitalWrite(MCP2515_CS_PIN, HIGH);

    u8g2.begin();

    u8g2.clearBuffer();
    
    // Desenha a logo da BMW centralizada na tela
    // A tela tem 128px. A logo tem 32px. (128 - 32) / 2 = 48
    u8g2.drawXBMP(48, 0, 32, 32, bmw_logo_bits);
    
    u8g2.sendBuffer();
    delay(2000);
}

void updateDisplay() {
    // Aumentei o timeout para 3 segundos para evitar falsos positivos
    if (millis() - carData.lastCanUpdateMs > 3000) {
        carData.canActive = false;
    }

    digitalWrite(MCP2515_CS_PIN, HIGH);
    u8g2.clearBuffer();

    if (!carData.canActive) {
        // Quando entra no modo Diagnóstico no FTManager, a FuelTech para de transmitir!
        // Em vez de apagar a tela (o que te fez achar que ele travou), 
        // agora ele mostra que a injeção está pausada/desligada.
        u8g2.setPowerSave(0);
        u8g2.setFont(u8g2_font_helvB10_tr);
        u8g2.setCursor(15, 20);
        u8g2.print("ECU OFFLINE");
    } else {
        // Com CAN (Carro Ligado) -> Liga a tela e renderiza
        u8g2.setPowerSave(0);
        u8g2.setFont(u8g2_font_helvB12_tr);
        
        switch (carData.currentScreen) {
            case 0: drawScreen0_Principal(); break;
            case 1: drawScreen1_Temperaturas(); break;
            case 2: drawScreen2_EGT(); break;
            case 3: drawScreen3_Injecao(); break;
            case 4: drawScreen4_Pressoes(); break;
            case 5: drawScreen5_Status(); break;
            default: carData.currentScreen = 0; break;
        }
        
        drawStatusBar();
    }

    u8g2.sendBuffer();
}

#endif // DISPLAY_MANAGER_H
