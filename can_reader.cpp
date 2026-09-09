// ============================================================
// can_reader.cpp — FTCAN 2.0 Decoder Engine + Fuel Calculator
// ============================================================
// Roda exclusivamente no Core 1 (CAN Task, alta prioridade).
// Todas as escritas em carData são protegidas por mutex.
// ============================================================
#include "can_reader.h"
#include "data_store.h"
#include <SPI.h>
#include <mcp2515.h>

// ============================================================
// Instâncias file-scoped (não expostas via header)
// ============================================================
static MCP2515 mcp2515(MCP2515_CS_PIN);
volatile unsigned long lastCanActivityMs = 0;

// ---- FTCAN 2.0 Segmented Packet Buffer ----
static uint8_t  ftPayload[2048];
static uint16_t ftPayloadLength   = 0;
static uint16_t ftPayloadExpected = 0;
static uint8_t  ftNextSegment     = 0;
static unsigned long ftSegmentTimestamp = 0;

// ---- Estado do EMA para cálculo de consumo ----
static float emaFilteredLPH = 0.0f;

// ============================================================
// initCAN() — Inicializa MCP2515 via Hardware SPI
// ============================================================
void initCAN() {
    SPI.begin(18, 19, 23, MCP2515_CS_PIN); // SCLK, MISO, MOSI, CS
    mcp2515.reset();
    mcp2515.setBitrate(CAN_BITRATE, CAN_OSC); // 1 Mbps, cristal 8 MHz
    mcp2515.setNormalMode();
    Serial.println(F("[CAN] MCP2515 OK — 1 Mbps / 8 MHz"));
}

// ============================================================
// FTCAN 2.0 Segmented Payload Parser
// ============================================================
// Protocolo proprietário FuelTech para dados que excedem 8 bytes.
// Segmentos: 0xFF=single, 0x00=first, 0x01..N=consecutive.
// Cada medida no payload = 4 bytes: [MeasureID_hi][MeasureID_lo][Value_hi][Value_lo]
// Bit 0 do MeasureID = flag de status (ignorado).
// ============================================================

static void parseMeasures(const uint8_t* payload, uint16_t length) {
    for (uint16_t i = 0; i + 3 < length; i += 4) {
        uint16_t measureId = (payload[i] << 8) | payload[i + 1];
        int16_t  value     = (payload[i + 2] << 8) | payload[i + 3];

        // Bit 0 = status byte → pular
        if (measureId & 0x01) continue;

        uint16_t dataId = measureId >> 1;

        if      (dataId == 0x0009) carData.battery     = value * 0.01f;   // Tensão (×0.01V)
        else if (dataId == 0x0047) carData.advance     = value * 0.1f;    // Avanço (×0.1°)
        else if (dataId == 0x0045) carData.dutyA       = value * 0.1f;    // Duty A (×0.1%)
        else if (dataId == 0x0048) carData.twoStepState = value;          // 2-Step (0/1)
        else if (dataId == 0x0183) carData.wgPressure  = value * 0.001f;  // WG (×0.001 bar)
    }
}

static void processFTCAN20(const uint8_t* d, uint8_t len) {
    if (len == 0) return;
    uint8_t segment = d[0];

    if (segment == 0xFF) {
        // ---- Single-frame packet ----
        parseMeasures(&d[1], len - 1);
    }
    else if (segment == 0x00) {
        // ---- First segment — inicializa buffer ----
        if (len < 3) return;
        ftPayloadExpected = ((d[1] & 0x07) << 8) | d[2];
        if (ftPayloadExpected > 2048) ftPayloadExpected = 2048;
        ftPayloadLength = 0;

        for (uint8_t i = 3; i < len; i++) {
            ftPayload[ftPayloadLength++] = d[i];
        }
        ftNextSegment = 1;
        ftSegmentTimestamp = millis();
    }
    else if (segment == ftNextSegment) {
        // ---- Consecutive segment ----
        for (uint8_t i = 1; i < len; i++) {
            if (ftPayloadLength < 2048) {
                ftPayload[ftPayloadLength++] = d[i];
            }
        }
        ftNextSegment++;
        ftSegmentTimestamp = millis();

        // Payload completo → decodificar
        if (ftPayloadLength >= ftPayloadExpected) {
            parseMeasures(ftPayload, ftPayloadExpected);
            ftPayloadLength = 0;
        }
    }
    else {
        // ---- Out of order — abort ----
        ftPayloadLength = 0;
        ftNextSegment = 0;
    }
}

// ============================================================
// decodeFT600() — Simplified Packets (8 bytes, CAN IDs fixos)
// ============================================================
static void decodeFT600(uint32_t canId, const uint8_t* d) {
    switch (canId) {

        // 0x14080600 — TPS, MAP, Air Temp, Engine Temp
        case FTCAN_ID_0x600:
            carData.tps        = decodeS16BE(d[0], d[1]) * 0.1f;     // %
            carData.map        = decodeS16BE(d[2], d[3]) * 0.001f;   // bar
            carData.airTemp    = decodeS16BE(d[4], d[5]) * 0.1f;     // °C
            carData.engineTemp = decodeS16BE(d[6], d[7]) * 0.1f;     // °C
            break;

        // 0x14080601 — Oil Press, Fuel Press, Water Press, Gear
        case FTCAN_ID_0x601:
            carData.oilPressure   = decodeS16BE(d[0], d[1]) * 0.001f; // bar
            carData.fuelPressure  = decodeS16BE(d[2], d[3]) * 0.001f; // bar
            carData.waterPressure = decodeS16BE(d[4], d[5]) * 0.001f; // bar
            carData.gear = (int8_t)decodeS16BE(d[6], d[7]);
            break;

        // 0x14080602 — Exhaust O2, RPM, Oil Temp
        case FTCAN_ID_0x602:
            carData.exhaustO2 = decodeS16BE(d[0], d[1]) * 0.001f;    // lambda
            carData.rpm       = decodeU16BE(d[2], d[3]);               // RPM direto
            carData.oilTemp   = decodeS16BE(d[4], d[5]) * 0.1f;      // °C
            break;

        // 0x14080603 — Wheel Speeds (RR nos bytes 4-5)
        case FTCAN_ID_0x603: {
            uint16_t rr = decodeU16BE(d[4], d[5]);
            if (rr != 0xFFFF && rr != 0x7FFF && rr > 0) {
                carData.speed = rr * 0.1f; // Escala oficial FuelTech
            } else if (rr == 0) {
                carData.speed = 0.0f;
            }
            // Valores 0xFFFF/0x7FFF = sem sensor → mantém último valor válido
            break;
        }

        // 0x14080607 — Lambda Corr, Inj Time A (campos úteis)
        case FTCAN_ID_0x607:
            carData.lambdaCorrection = decodeS16BE(d[0], d[1]) * 0.1f;   // %
            carData.injTimeA         = decodeS16BE(d[4], d[5]) * 0.01f;  // ms
            break;

        // 0x14080608 — Oil Temp (fallback), Trans Temp (campos úteis)
        case FTCAN_ID_0x608: {
            float oilTemp608 = decodeS16BE(d[0], d[1]) * 0.1f;
            if (carData.oilTemp == 0.0f) {
                carData.oilTemp = oilTemp608;
            }
            carData.transTemp = decodeS16BE(d[2], d[3]) * 0.1f;
            break;
        }

        default:
            break;
    }
}

// ============================================================
// decodeEGT() — Pacotes EGT-4 (protocolo FuelTech)
//   Valor × 0.125 = °C
//   0x20D0 = termopar desconectado / circuito aberto
// ============================================================
static void decodeEGT(uint32_t canId, const uint8_t* d) {
    uint8_t chOffset;

    if      (canId == EGT4_MODEL_A) chOffset = 0; // Canais 1-4 → índices 0-3
    else if (canId == EGT4_MODEL_B) chOffset = 4; // Canais 5-8 → índices 4-7
    else return;

    for (uint8_t i = 0; i < 4; i++) {
        uint16_t raw = decodeU16BE(d[i * 2], d[i * 2 + 1]);
        uint8_t ch = chOffset + i;

        if (raw == EGT_OPEN_CIRCUIT_RAW) {
            // 0x20D0 → circuito aberto (termopar desconectado)
            carData.egt[ch]      = EGT_ERROR_VALUE;
            carData.egtError[ch] = true;
        } else {
            carData.egt[ch]      = raw * 0.125f;  // °C
            carData.egtError[ch] = false;
        }
    }
}

// ============================================================
// calculateFuelConsumption() — M50 6 Cilindros, 994 cc/min
// ============================================================
// Fórmula:
//   Duty Cycle = PW_ms × RPM / 120000
//   Fluxo (cc/min) = NUM_CYL × Duty × FLOW_cc_min
//   L/h = cc/min × 60 / 1000
//
// Verificação numérica (3000 RPM, 5ms PW):
//   Duty = 5 × 3000 / 120000 = 0.125 (12.5%)
//   Fluxo = 6 × 0.125 × 994 = 745.5 cc/min
//   L/h = 745.5 × 0.06 = 44.73 L/h ✓
// ============================================================

static void calculateFuelConsumption() {
    static unsigned long lastIntegrationMs = 0;
    unsigned long now = millis();

    if (carData.rpm < 100 || carData.injTimeA < 0.1f) {
        // Motor desligado ou RPM muito baixo → zerar instantâneo
        emaFilteredLPH = 0.0f;
        carData.fuelFlowLPH = 0.0f;
        carData.fuelEffKML  = 0.0f;
        lastIntegrationMs = now; // Evita dt gigante ao religar
        return;
    }

    // Duty Cycle (adimensional, 0.0 a 1.0)
    float duty = carData.injTimeA * (float)carData.rpm / 120000.0f;

    // Fluxo total de todos os cilindros (cc/min)
    float flowCcMin = (float)NUM_CYLINDERS * duty * INJECTOR_FLOW_CC_MIN;

    // Conversão para L/h: cc/min × 60 / 1000 = cc/min × 0.06
    float instantLPH = flowCcMin * 0.06f;

    // ---- EMA Filter 80/20 (suaviza DFCO / Decel) ----
    emaFilteredLPH = emaFilteredLPH * (1.0f - EMA_ALPHA)
                   + instantLPH    * EMA_ALPHA;

    carData.fuelFlowLPH = emaFilteredLPH;

    // ---- Eficiência instantânea km/L ----
    if (emaFilteredLPH > 0.5f && carData.speed >= 1.0f) {
        float kml = carData.speed / emaFilteredLPH;
        carData.fuelEffKML = (kml > 99.9f) ? 99.9f : kml;
    } else {
        carData.fuelEffKML = 0.0f;
    }

    // ============================================================
    // Trip Average — Integração numérica (Euler, dt em horas)
    // ============================================================
    // Distância: ∫ speed(km/h) · dt(h) = km
    // Combustível: ∫ fuelFlow(L/h) · dt(h) = L
    // Média: totalDistance / totalFuel = km/L
    // ============================================================
    if (lastIntegrationMs > 0 && now > lastIntegrationMs) {
        float dt_h = (float)(now - lastIntegrationMs) / 3600000.0f;

        // Acumula distância (só com veículo em movimento)
        if (carData.speed > 0.5f) {
            carData.totalDistanceKm += carData.speed * dt_h;
        }

        // Acumula combustível (só com motor consumindo)
        if (emaFilteredLPH > 0.1f) {
            carData.totalFuelLiters += emaFilteredLPH * dt_h;
        }

        // Calcula média (proteção contra divisão por zero)
        if (carData.totalFuelLiters > 0.001f) {
            float avg = carData.totalDistanceKm / carData.totalFuelLiters;
            carData.avgKmL = (avg > 99.9f) ? 99.9f : avg;
        } else {
            carData.avgKmL = 0.0f;
        }
    }
    lastIntegrationMs = now;
}

// ============================================================
// processCANMessages() — Drena o buffer RX do MCP2515
// ============================================================
// Chamado pelo CAN Task (Core 1) a cada ~1ms.
// Toma o mutex, decodifica todas as mensagens pendentes,
// recalcula consumo, e libera o mutex.
// ============================================================

void processCANMessages() {
    struct can_frame message;

    if (!lockCarData()) return;

    bool hadData = false;

    while (mcp2515.readMessage(&message) == MCP2515::ERROR_OK) {
        hadData = true;

        // Remove flag de Extended Frame (bit 31) da biblioteca
        uint32_t id = message.can_id & 0x1FFFFFFF;
        const uint8_t* d = message.data;

        // Decodifica baseado no CAN ID
        switch (id) {
            case FTCAN_ID_0x600:
            case FTCAN_ID_0x601:
            case FTCAN_ID_0x602:
            case FTCAN_ID_0x603:
            case FTCAN_ID_0x607:
            case FTCAN_ID_0x608:
                decodeFT600(id, d);
                break;

            case EGT4_MODEL_A:
            case EGT4_MODEL_B:
                decodeEGT(id, d);
                break;

            default: {
                // Verifica se é FTCAN 2.0 Segmentado (DataFieldID = 0x02)
                // Bits 13-11 = 0x02 → 0x1000
                // Message IDs de broadcast: 0x0FF, 0x1FF, 0x2FF, 0x3FF
                uint16_t dataFieldAndMsg = id & 0x3FFF;
                if (dataFieldAndMsg == 0x10FF || dataFieldAndMsg == 0x11FF ||
                    dataFieldAndMsg == 0x12FF || dataFieldAndMsg == 0x13FF) {
                    processFTCAN20(d, message.can_dlc);
                }
                break;
            }
        }

        carData.lastCanUpdateMs = millis();
        carData.canActive = true;
        lastCanActivityMs = millis();
    }

    // Recalcula consumo após processar todas as mensagens
    if (hadData) {
        calculateFuelConsumption();
    }

    // CAN timeout — marca offline se sem dados por CAN_TIMEOUT_MS
    if (millis() - carData.lastCanUpdateMs > CAN_TIMEOUT_MS) {
        carData.canActive = false;
    }

    // Timeout de segmentos parciais (evita buffer preso indefinidamente)
    if (ftPayloadLength > 0 && (millis() - ftSegmentTimestamp > SEGMENT_TIMEOUT_MS)) {
        ftPayloadLength = 0;
        ftNextSegment = 0;
    }

    unlockCarData();
}

// ============================================================
// sendVirtualButtons() — SwitchPanel → FuelTech via CAN
// ============================================================
// Transmite o bitmask dos 8 botões virtuais (controlados via RealDash)
// para a FuelTech no Byte 2 do frame CAN.
// ID descoberto via sniffer: 0x12200320 (Extended Frame).
// ============================================================

void sendVirtualButtons() {
    // Lê switchState com mutex (escrito pelo BT Controller no Core 0)
    uint8_t sw = 0;
    if (lockCarData(5)) {
        sw = carData.switchState;
        unlockCarData();
    }

    struct can_frame frame;
    frame.can_dlc = 8;
    frame.can_id  = SWITCHPANEL_TX_ID | CAN_EFF_FLAG;
    memset(frame.data, 0, 8);
    frame.data[2] = sw; // Byte 2 = bitmask dos botões

    mcp2515.sendMessage(&frame);
}

void prepareCANForSleep() {
    mcp2515.reset();
    mcp2515.setBitrate(CAN_BITRATE, CAN_OSC);
    // CANINTE = 0x03 (RX0IE + RX1IE) via SPI direto (setRegister é privado)
    digitalWrite(MCP2515_CS_PIN, LOW);
    SPI.transfer(0x02);   // MCP2515 SPI WRITE instruction
    SPI.transfer(0x2B);   // CANINTE register address
    SPI.transfer(0x03);   // RX0IE + RX1IE = INT pin fires on RX
    digitalWrite(MCP2515_CS_PIN, HIGH);
    mcp2515.setListenOnlyMode();
    Serial.println(F("[CAN] MCP2515 em Listen Mode — Wake-on-CAN ativo"));
}
