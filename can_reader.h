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
// FTCAN 2.0 Segmented Payload Parser
// ============================================================
uint8_t ftPayload[2048];
uint16_t ftPayloadLength = 0;
uint16_t ftPayloadExpected = 0;
uint8_t ftNextSegment = 0;

void parseMeasures(const uint8_t* payload, uint16_t length) {
    for (uint16_t i = 0; i + 3 < length; i += 4) {
        uint16_t measureId = (payload[i] << 8) | payload[i+1];
        int16_t value = (payload[i+2] << 8) | payload[i+3];
        
        uint16_t dataId = measureId >> 1;
        uint8_t isStatus = measureId & 0x01;
        
        if (isStatus) continue;
        
        if (dataId == 0x0009) { // Battery Voltage (0.01)
            carData.battery = value * 0.01f;
        }
        else if (dataId == 0x0047) { // Ignition Advance (0.1)
            carData.advance = value * 0.1f;
        }
        else if (dataId == 0x0045) { // Duty Cycle Bank A (0.1)
            carData.dutyA = value * 0.1f;
        }
        else if (dataId == 0x0046) { // Duty Cycle Bank B (0.1)
            carData.dutyB = value * 0.1f;
        }
        else if (dataId == 0x004D) { // Eletro Fan State (1)
            carData.fanState = value;
        }
        else if (dataId == 0x0048) { // 2-Step Signal
            carData.twoStepState = value;
        }
        else if (dataId == 0x0049) { // 3-Step Signal (Antilag)
            carData.threeStepState = value;
        }
        else if (dataId == 0x0183) { // Wastegate Pressure Input (assumindo 0.001)
            carData.wgPressure = value * 0.001f;
        }
        else if (dataId == 0x0266) { // Differential Fuel Pressure (0.001)
            carData.diffFuelPressure = value * 0.001f;
        }
        else if (dataId == 0x0152) { // Generic Outputs State (Bitmask)
            carData.genericOutputs = (uint16_t)value;
        }
    }
}

void processFTCAN20(const uint8_t* d, uint8_t len) {
    if (len == 0) return;
    uint8_t segment = d[0];
    
    if (segment == 0xFF) {
        // Single packet
        parseMeasures(&d[1], len - 1);
    } 
    else if (segment == 0x00) {
        // First segment
        if (len < 3) return;
        ftPayloadExpected = ((d[1] & 0x07) << 8) | d[2];
        if (ftPayloadExpected > 2048) ftPayloadExpected = 2048;
        ftPayloadLength = 0;
        
        for (uint8_t i = 3; i < len; i++) {
            ftPayload[ftPayloadLength++] = d[i];
        }
        ftNextSegment = 1;
    }
    else if (segment == ftNextSegment) {
        // Subsequent segments
        for (uint8_t i = 1; i < len; i++) {
            if (ftPayloadLength < 2048) {
                ftPayload[ftPayloadLength++] = d[i];
            }
        }
        ftNextSegment++;
        
        // Check if finished
        if (ftPayloadLength >= ftPayloadExpected) {
            parseMeasures(ftPayload, ftPayloadExpected);
            ftPayloadLength = 0; // Reset for next
        }
    } else {
        // Out of order, abort
        ftPayloadLength = 0;
        ftNextSegment = 0;
    }
}

// ============================================================
// decodeFT600() — Decodifica pacotes Simplified do FT600
// ============================================================
static void decodeFT600(uint32_t canId, const uint8_t* d) {
    switch (canId) {

        // 0x14080600 — TPS, MAP, Air Temp, Engine Temp
        case FTCAN_ID_0x600: {
            carData.tps        = decodeS16BE(d[0], d[1]) * 0.1f;       // %
            // MAP vem em bar*1000; mantemos em Bar para mostrar vácuo negativo
            carData.map        = decodeS16BE(d[2], d[3]) * 0.001f;     // Bar
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

        // 0x14080603 — Advance, Target RPM, Cam
        case FTCAN_ID_0x603: {
            carData.advance = decodeS16BE(d[0], d[1]) * 0.1f;          // graus
            break;
        }

        // 0x14080606 — Speed (Driven/Non-driven)
        case FTCAN_ID_0x606: {
            // Speed (x10). Bytes 0-1 = Driven Wheel Speed
            carData.speed = decodeU16BE(d[0], d[1]) * 0.1f;            // km/h
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

        // Verifica se é um pacote simplificado
        switch (id) {
            case FTCAN_ID_0x600:
            case FTCAN_ID_0x601:
            case FTCAN_ID_0x602:
            case FTCAN_ID_0x603:
            case FTCAN_ID_0x606:
            case FTCAN_ID_0x607:
            case FTCAN_ID_0x608:
                decodeFT600(id, d);
                break;

            case EGT4_MODEL_A:
            case EGT4_MODEL_B:
                decodeEGT(id, d);
                break;

            default:
                // Verifica se é um FTCAN 2.0 Segmentado (DataFieldID = 0x02)
                // Bits 13-11 = 0x02 -> 0x1000
                // Message IDs de Broadcast em Tempo Real: 0x0FF, 0x1FF, 0x2FF, 0x3FF
                uint16_t dataFieldAndMsg = id & 0x3FFF;
                if (dataFieldAndMsg == 0x10FF || dataFieldAndMsg == 0x11FF || 
                    dataFieldAndMsg == 0x12FF || dataFieldAndMsg == 0x13FF) {
                    processFTCAN20(d, message.can_dlc);
                }
                break;
        }

        // Atualiza timestamp e flag de atividade
        carData.lastCanUpdateMs = millis();
        carData.canActive = true;
    }
}

// ============================================================
// sendVirtualButtons() — Transmite o estado do painel do RealDash
//                        para a FuelTech continuamente (20Hz)
// ============================================================
void sendVirtualButtons() {
    struct can_frame frame;
    frame.can_dlc = 8;
    
    // Switchpad-8 (ID Secreto descoberto via Sniffer: 0x12200320)
    frame.can_id = 0x12200320 | CAN_EFF_FLAG;
    
    // MISTÉRIO RESOLVIDO: O Botão 1 acende quando o Byte 2 é alterado!
    // A FuelTech usa o Byte 2 do CAN frame como o Bitmask dos 8 botões.
    // Sendo assim, podemos mapear todos os 8 botões diretamente no Byte 2.
    frame.data[0] = 0x00;
    frame.data[1] = 0x00;
    frame.data[2] = carData.switchState; // O Bitmask do celular vai direto pro Byte 2!
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    mcp2515.sendMessage(&frame);
}

#endif // CAN_READER_H
