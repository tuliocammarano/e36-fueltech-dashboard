// ============================================================
// display_manager.cpp — HMI State Machine (U8g2 SSD1305 128x32)
// ============================================================
// Redesign v2.1: Fontes grandes, labels mnemônicos (2 chars),
// hierarquia visual consistente em todas as 6 telas.
//
// Padrão de layout (128×32, 2 linhas):
//   Linha 1 (y=13): Valores primários em helvB14 (grande)
//   Linha 2 (y=30): Valores secundários em helvB12 (médio)
//   Labels: u8g2_font_5x7_tr (2-3 chars, mnemônico)
//   Status bar: 14px direita (dots, ícones)
//
// Mnemonics:
//   MT=Motor Temp  OT=Oil Temp  AT=Air Temp  CT=Câmbio Temp
//   OP=Oil Press   FP=Fuel Press  WP=Water Press  BP=Brake Press
//   LC=Lambda Corr  AV=Avanço  DC=Duty Cycle  INJ=Injeção
// ============================================================
#include "display_manager.h"
#include "data_store.h"
#include "ota_manager.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <SPI.h>

// ============================================================
// Display SSD1305 128x32 — Software SPI
// ============================================================
static U8G2_SSD1305_128X32_ADAFRUIT_F_4W_SW_SPI u8g2(
    U8G2_R2, OLED_CLK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RESET
);

// ============================================================
// BMW Logo 32x32 (boot)
// ============================================================
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

// ============================================================
// Helpers de renderização
// ============================================================

// Label mnemônico (2-3 chars) em fonte tiny, seguido de espaço
static void drawLabel(int16_t x, int16_t y, const char* label) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setCursor(x, y);
    u8g2.print(label);
}

// Valor inteiro em fonte grande (helvB14)
static int16_t drawBigInt(int16_t x, int16_t y, int value) {
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(x, y);
    u8g2.print(value);
    return u8g2.tx; // retorna posição X após o texto
}

// Valor float em fonte grande (helvB14)
static int16_t drawBigFloat(int16_t x, int16_t y, float value, uint8_t decimals) {
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(x, y);
    u8g2.print(value, decimals);
    return u8g2.tx;
}

// Valor inteiro em fonte média (helvB12)
static int16_t drawMedInt(int16_t x, int16_t y, int value) {
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(x, y);
    u8g2.print(value);
    return u8g2.tx;
}

// Valor float em fonte média (helvB12)
static int16_t drawMedFloat(int16_t x, int16_t y, float value, uint8_t decimals) {
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(x, y);
    u8g2.print(value, decimals);
    return u8g2.tx;
}

// Sufixo pequeno (unidade) logo após o valor
static void drawSuffix(int16_t x, int16_t y, const char* suffix) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setCursor(x, y);
    u8g2.print(suffix);
}

// ============================================================
// Status Bar (14px direita — ícones + indicador de página)
// ============================================================
static void drawStatusBar(const CarData &d) {
    // Dots indicadores de página
    int startY = (SCREEN_HEIGHT - (NUM_SCREENS * 5)) / 2;
    for (int i = 0; i < NUM_SCREENS; i++) {
        if (i == d.currentScreen)
            u8g2.drawBox(126, startY + (i * 5), 2, 2);
        else
            u8g2.drawPixel(126, startY + (i * 5));
    }

    u8g2.setFont(u8g2_font_5x7_tr);

    // BT conectado
    if (d.btConnected) {
        u8g2.setCursor(118, 7);
        u8g2.print("B");
    }

    // 2-Step ativo
    if (d.twoStepState) {
        u8g2.setCursor(115, 15);
        u8g2.print("2S");
    }

    // Anti-Lag ativo (botão virtual bit 2)
    if (d.switchState & (1 << 2)) {
        u8g2.setCursor(115, 23);
        u8g2.print("AL");
    }

    // EGT HOT (≥700°C em qualquer canal válido)
    for (int i = 0; i < 6; i++) {
        if (!d.egtError[i] && d.egt[i] >= 700.0f) {
            if ((millis() / 100) % 2 == 0) {
                u8g2.setCursor(115, 31);
                u8g2.print("!!");
            }
            break;
        }
    }
}

// ============================================================
// Tela 0 — PRINCIPAL (RPM · Gear · MAP · Lambda)
// ============================================================
static void drawScreen0_Principal(const CarData &d) {
    // Linha 1: RPM (grande) + Gear (grande, direita)
    int16_t px = drawBigInt(0, 13, d.rpm);
    drawSuffix(px + 1, 7, "rpm");

    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(97, 13);
    u8g2.print(gearName(d.gear));

    // Linha 2: MAP (médio) + Lambda (médio)
    px = drawMedFloat(0, 30, d.map, 2);
    drawSuffix(px + 1, 30, "bar");

    px = drawMedFloat(60, 30, d.exhaustO2, 2);
    drawSuffix(px + 1, 30, "O2");
}

// ============================================================
// Tela 1 — TEMPERATURAS (MT · OT · AT)
// ============================================================
static void drawScreen1_Temperaturas(const CarData &d) {
    // Linha 1: Motor Temp + Oil Temp (grandes)
    drawLabel(0, 7, "et");
    drawBigInt(14, 13, (int)d.engineTemp);

    drawLabel(60, 7, "ot");
    drawBigInt(74, 13, (int)d.oilTemp);

    // Linha 2: Air Temp (médio) + Warning
    drawLabel(0, 24, "at");
    drawMedInt(14, 30, (int)d.airTemp);

    // Warning: temp motor ou óleo acima do limiar
    if (d.engineTemp > ENGINE_TEMP_WARNING || d.oilTemp > OIL_TEMP_WARNING) {
        u8g2.setFont(u8g2_font_helvB14_tr);
        u8g2.setCursor(90, 30);
        if ((millis() / 300) % 2 == 0) u8g2.print("!");
    }
}

// ============================================================
// Tela 2 — EGT (6 canais para M50)
// ============================================================
static void drawScreen2_EGT(const CarData &d) {
    // 6 canais distribuídos em 2 linhas × 3 colunas
    // Layout: "N:NNN" usando helvB10 para valores
    for (int row = 0; row < 2; row++) {
        int16_t y = (row == 0) ? 12 : 28;
        for (int col = 0; col < 3; col++) {
            int ch = row * 3 + col;
            int16_t x = col * 38;

            // Número do canal (tiny)
            u8g2.setFont(u8g2_font_5x7_tr);
            u8g2.setCursor(x, y);
            u8g2.print(ch + 1);

            // Valor EGT (médio-grande)
            u8g2.setFont(u8g2_font_helvB10_tr);
            u8g2.setCursor(x + 7, y);
            if (d.egtError[ch]) {
                u8g2.print("--");
            } else {
                u8g2.print((int)d.egt[ch]);
            }
        }
    }
}

// ============================================================
// Tela 3 — CONSUMO + TRIP (L/h · km/L · AVG · USED · INJ)
// ============================================================
static void drawScreen3_Consumo(const CarData &d) {
    int16_t px;

    // Linha 1: Consumo instantâneo (grande) + Média Trip (grande)
    if (d.speed >= 1.0f && d.fuelEffKML > 0.1f) {
        px = drawBigFloat(0, 13, d.fuelEffKML, 1);
        drawSuffix(px + 1, 7, "kml");
    } else {
        px = drawBigFloat(0, 13, d.fuelFlowLPH, 1);
        drawSuffix(px + 1, 7, "L/h");
    }

    drawLabel(68, 7, "avg");
    if (d.avgKmL > 0.1f) {
        drawBigFloat(86, 13, d.avgKmL, 1);
    } else {
        u8g2.setFont(u8g2_font_helvB14_tr);
        u8g2.setCursor(86, 13);
        u8g2.print("--");
    }

    // Linha 2: Total consumido + Tempo de injeção (secundários)
    drawLabel(0, 24, "used");
    px = drawMedFloat(24, 30, d.totalFuelLiters, 1);
    drawSuffix(px, 30, "L");

    drawLabel(68, 24, "inj");
    px = drawMedFloat(86, 30, d.injTimeA, 1);
    drawSuffix(px, 30, "ms");
}

// ============================================================
// Tela 4 — PRESSÕES (OP · FP · WP)
// ============================================================
static void drawScreen4_Pressoes(const CarData &d) {
    // Linha 1: Oil Press + Fuel Press (grandes)
    drawLabel(0, 7, "op");
    drawBigFloat(14, 13, d.oilPressure, 1);

    drawLabel(60, 7, "fp");
    drawBigFloat(74, 13, d.fuelPressure, 1);

    // Linha 2: Water Press (médio)
    drawLabel(0, 24, "wp");
    drawMedFloat(14, 30, d.waterPressure, 1);
}

// ============================================================
// Tela 5 — MOTOR (Speed · Battery · Advance · Duty Cycle)
// ============================================================
static void drawScreen5_Motor(const CarData &d) {
    // Linha 1: Velocidade + Bateria (grandes)
    int16_t px = drawBigFloat(0, 13, d.speed, 0);
    drawSuffix(px + 1, 7, "km/h");

    px = drawBigFloat(68, 13, d.battery, 1);
    drawSuffix(px + 1, 7, "V");

    // Linha 2: Avanço de ignição + Duty Cycle (médios)
    drawLabel(0, 24, "av");
    px = drawMedFloat(14, 30, d.advance, 1);
    drawSuffix(px, 30, "o");

    drawLabel(60, 24, "dc");
    px = drawMedFloat(74, 30, d.dutyA, 0);
    drawSuffix(px, 30, "%");
}

// ============================================================
// Tela 6 — SYSTEM / OTA UPDATE
// ============================================================
static void drawScreen6_OTA(const CarData &d) {
    u8g2.setFont(u8g2_font_helvB12_tr);

    if (!ota_mode_active) {
        // OTA inativo — instrução para o usuário
        u8g2.setCursor(0, 13);
        u8g2.print("OTA OFF");
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setCursor(0, 30);
        u8g2.print("Long press p/ ativar");
    } else if (!d.wifiConnected) {
        u8g2.setCursor(0, 13);
        u8g2.print("WiFi...");
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setCursor(0, 24);
        u8g2.print("Aguardando conexao");
        u8g2.setCursor(0, 32);
        u8g2.print("Press btn to reboot");
    } else {
        u8g2.setCursor(0, 13);
        u8g2.print("OTA READY");
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setCursor(0, 24);
        u8g2.print(WiFi.localIP().toString());
        u8g2.setCursor(0, 32);
        u8g2.print("Press btn to reboot");
    }
}

// ============================================================
// initDisplay()
// ============================================================
void initDisplay() {
    pinMode(MCP2515_CS_PIN, OUTPUT);
    digitalWrite(MCP2515_CS_PIN, HIGH);

    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.drawXBMP(48, 0, 32, 32, bmw_logo_bits);
    u8g2.sendBuffer();
    delay(2000);

    Serial.println(F("[DISPLAY] SSD1305 128x32 OK"));
}

// ============================================================
// updateDisplay()
// ============================================================
void updateDisplay() {
    CarData d = readCarData();

    u8g2.clearBuffer();

    // ---- Overlay "TRIP RESET" por 500ms após long press ----
    if (d.tripResetFlag && (millis() - d.tripResetTimestamp < 500)) {
        u8g2.setFont(u8g2_font_helvB12_tr);
        u8g2.setCursor(8, 20);
        u8g2.print("TRIP RESET");
        u8g2.sendBuffer();
        if (millis() - d.tripResetTimestamp >= 500) {
            if (lockCarData(5)) {
                carData.tripResetFlag = false;
                unlockCarData();
            }
        }
        return;
    }

    if (d.tripResetFlag && (millis() - d.tripResetTimestamp >= 500)) {
        if (lockCarData(5)) {
            carData.tripResetFlag = false;
            unlockCarData();
        }
    }

    if (!d.canActive && d.currentScreen != 6) {
        u8g2.setFont(u8g2_font_helvB12_tr);
        u8g2.setCursor(18, 20);
        u8g2.print("sleeping...");
    } else {
        switch (d.currentScreen) {
            case 0: drawScreen0_Principal(d);    break;
            case 1: drawScreen1_Temperaturas(d); break;
            case 2: drawScreen2_EGT(d);          break;
            case 3: drawScreen3_Consumo(d);      break;
            case 4: drawScreen4_Pressoes(d);     break;
            case 5: drawScreen5_Motor(d);        break;
            case 6: drawScreen6_OTA(d);          break;
            default: break;
        }
        drawStatusBar(d);
    }

    u8g2.sendBuffer();
}

void powerSaveDisplay() {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    u8g2.setPowerSave(1);
}
