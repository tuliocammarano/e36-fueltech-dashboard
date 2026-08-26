// ============================================================
// can_reader.h — Leitura e decodificação de mensagens CAN
//                FuelTech ECU via MCP2515
// ============================================================
#ifndef CAN_READER_H
#define CAN_READER_H

#include <mcp2515.h>
#include "config.h"
#include "data_store.h"

// Instância global do MCP2515 (CS definido em config.h)
MCP2515 mcp2515(MCP2515_CS_PIN);

// ============================================================
// initCAN() — Inicializa o controlador MCP2515
// ============================================================
void initCAN() {
    SPI.begin(18, 19, 23, MCP2515_CS_PIN); // SCLK, MISO, MOSI, CS
    mcp2515.reset();
    mcp2515.setBitrate(CAN_BITRATE, CAN_OSC);  // 1 Mbps, cristal 8 MHz
    mcp2515.setNormalMode();
    Serial.println(F("[CAN] MCP2515 inicializado — 1 Mbps / 8 MHz"));
}

// ============================================================
// decodeFT600() — Decodifica pacotes Simplified do FT600
// ============================================================
static void decodeFT600(uint32_t canId, const uint8_t* d) {
    switch (canId) {

        // 0x14080600 — TPS, MAP, Air Temp, Engine Temp
        case FTCAN_ID_0x600: {
            carData.tps        = decodeS16BE(d[0], d[1]) * 0.1f;       // %
            // MAP vem em bar×1000; converte para kPa (×100)
            carData.map        = decodeS16BE(d[2], d[3]) * 0.001f * 100.0f;  // kPa
            carData.airTemp    = decodeS16BE(d[4], d[5]) * 0.1f;       // °C
            carData.engineTemp = decodeS16BE(d[6], d[7]) * 0.1f;       // °C
            break;
        }

        // 0x14080601 — Oil Press, Fuel Press, Water Press, Gear
        case FTCAN_ID_0x601: {
            carData.oilPressure   = decodeS16BE(d[0], d[1]) * 0.001f;  // bar
            carData.fuelPressure  = decodeS16BE(d[2], d[3]) * 0.001f;  // bar
            carData.waterPressure = decodeS16BE(d[4], d[5]) * 0.001f;  // bar
            // Marcha: -2=Park, -1=Ré, 0=Neutro, 1-10=marchas
            carData.gear = (int8_t)decodeS16BE(d[6], d[7]);
            break;
        }

        // 0x14080602 — Exhaust O2, RPM, Oil Temp, Pit Limit (ignorado)
        case FTCAN_ID_0x602: {
            carData.exhaustO2 = decodeS16BE(d[0], d[1]) * 0.001f;      // lambda
            carData.rpm       = decodeU16BE(d[2], d[3]);                // RPM direto
            carData.oilTemp   = decodeS16BE(d[4], d[5]) * 0.1f;        // °C
            // bytes 6-7: Pit Limit — ignorado
            break;
        }

        // 0x14080607 — Lambda Corr, Fuel Flow, Inj Time A, Inj Time B
        case FTCAN_ID_0x607: {
            carData.lambdaCorrection = decodeS16BE(d[0], d[1]) * 0.01f;   // fator
            carData.fuelFlowTotal    = decodeS16BE(d[2], d[3]) * 0.01f;   // L/min
            carData.injTimeA         = decodeS16BE(d[4], d[5]) * 0.01f;   // ms
            carData.injTimeB         = decodeS16BE(d[6], d[7]) * 0.01f;   // ms
            break;
        }

        // 0x14080608 — Oil Temp (fallback), Trans Temp, Fuel Cons, Brake Press
        case FTCAN_ID_0x608: {
            // Oil Temp alternativo — só usa se o valor do 0x602 for zero
            float oilTemp608 = decodeS16BE(d[0], d[1]) * 0.1f;         // °C
            if (carData.oilTemp == 0.0f) {
                carData.oilTemp = oilTemp608;
            }
            carData.transTemp        = decodeS16BE(d[2], d[3]) * 0.1f; // °C
            carData.fuelConsumption  = decodeS16BE(d[4], d[5]) * 0.01f;
            carData.brakePressure    = decodeS16BE(d[6], d[7]) * 0.001f; // bar
            break;
        }

        default:
            break;
    }
}

// ============================================================
// decodeEGT() — Decodifica pacotes EGT-4 (protocolo FuelTech)
//   Valor × 0.125 = °C
//   0x20D0 (8400 → 1050°C) = termopar com erro
// ============================================================
static void decodeEGT(uint32_t canId, const uint8_t* d) {
    // Offset do canal: 0 para Model A (ch1-4), 4 para Model B (ch5-8)
    uint8_t chOffset = 0;

    if (canId == EGT4_MODEL_A) {
        chOffset = 0;   // Canais 1-4 → índices 0-3
    } else if (canId == EGT4_MODEL_B) {
        chOffset = 4;   // Canais 5-8 → índices 4-7
    } else {
        return;         // ID desconhecido
    }

    // Decodifica 4 canais (2 bytes cada)
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t raw = decodeU16BE(d[i * 2], d[i * 2 + 1]);
        uint8_t ch = chOffset + i;

        if (raw == 0x20D0) {
            // Termopar desconectado ou com erro (1050°C)
            carData.egt[ch]      = EGT_ERROR_VALUE;
            carData.egtError[ch] = true;
        } else {
            carData.egt[ch]      = raw * 0.125f;    // °C
            carData.egtError[ch] = false;
        }
    }
}

// ============================================================
// readCAN() — Lê todas as mensagens disponíveis no buffer
//             do MCP2515 e decodifica cada uma
// ============================================================
void readCAN() {
    struct can_frame message;

    // Lê enquanto houver mensagens no buffer
    while (mcp2515.readMessage(&message) == MCP2515::ERROR_OK) {
        // Remove a flag de Extended Frame (bit 31) colocada pela biblioteca MCP2515
        uint32_t id = message.can_id & 0x1FFFFFFF;
        const uint8_t* d = message.data;

        // Tenta decodificar como pacote FT600
        switch (id) {
            case FTCAN_ID_0x600:
            case FTCAN_ID_0x601:
            case FTCAN_ID_0x602:
            case FTCAN_ID_0x607:
            case FTCAN_ID_0x608:
                decodeFT600(id, d);
                break;

            // Pacotes EGT-4
            case EGT4_MODEL_A:
            case EGT4_MODEL_B:
                decodeEGT(id, d);
                break;

            default:
                // Mensagem CAN não mapeada — ignora
                continue;
        }

        // Atualiza timestamp e flag de atividade
        carData.lastCanUpdateMs = millis();
        carData.canActive = true;
    }
}

#endif // CAN_READER_H
