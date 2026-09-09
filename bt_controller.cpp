// ============================================================
// bt_controller.cpp — Bluetooth Classic SPP + RealDash Gateway
// ============================================================
// Protocolo: RealDash CAN (Protocol 44 — Binary Native)
// Frame: [44 33 22 11] [4B frameId] [8B data] [1B checksum]
// Total: 17 bytes por frame.
//
// Downlink: Telemetria → RealDash (20Hz fast / 5Hz slow)
// Uplink: Comandos ← RealDash (troca de tela, botões virtuais)
// ============================================================
#include "bt_controller.h"
#include "data_store.h"
#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// ============================================================
// Instâncias file-scoped
// ============================================================
static BluetoothSerial SerialBT;
static unsigned long buttonReleaseTimers[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// ============================================================
// Inicialização
// ============================================================
void initBluetooth() {
    SerialBT.begin(BT_DEVICE_NAME);
    Serial.printf("[BT] Iniciado como '%s'\n", BT_DEVICE_NAME);
}

bool isBTConnected() {
    return SerialBT.hasClient();
}

// ============================================================
// RealDash CAN Binary Frame (Protocol 44)
// ============================================================
// Header: 0x44 0x33 0x22 0x11
// FrameID: 4 bytes little-endian (bit 31 = EFF flag para IDs > 0x7FF)
// Data: 8 bytes
// Checksum: soma de todos os 16 bytes anteriores (1 byte)
// ============================================================
static void sendRDBinaryFrame(uint32_t frameId, const uint8_t* data) {
    if (!SerialBT.hasClient()) return;

    // Seta EFF flag para Extended Frame IDs (29-bit)
    if (frameId > 0x7FF) {
        frameId |= 0x80000000;
    }

    uint8_t buf[17];

    // Header RealDash CAN
    buf[0] = 0x44; buf[1] = 0x33; buf[2] = 0x22; buf[3] = 0x11;

    // Frame ID (little-endian)
    buf[4] = frameId & 0xFF;
    buf[5] = (frameId >> 8) & 0xFF;
    buf[6] = (frameId >> 16) & 0xFF;
    buf[7] = (frameId >> 24) & 0xFF;

    // Data payload (8 bytes)
    memcpy(&buf[8], data, 8);

    // Checksum (soma simples dos 16 primeiros bytes)
    uint8_t chk = 0;
    for (int i = 0; i < 16; i++) chk += buf[i];
    buf[16] = chk;

    SerialBT.write(buf, 17);
}

// ============================================================
// Telemetry Streaming (Downlink → RealDash)
// ============================================================
// Frames rápidos (20Hz): dados críticos para o piloto
// Frames lentos (5Hz): temperaturas, EGT, bateria
// ============================================================
void streamTelemetryBT() {
    if (!SerialBT.hasClient()) return;

    // Lê snapshot thread-safe
    CarData d = readCarData();

    // Atualiza flag btConnected para o display
    if (lockCarData(2)) {
        carData.btConnected = true;
        unlockCarData();
    }

    static uint8_t slowCounter = 0;
    slowCounter++;
    if (slowCounter >= 4) slowCounter = 0; // Divide: 20Hz → 5Hz

    uint8_t payload[8];

    // ================================================================
    // FRAMES RÁPIDOS (20 Hz)
    // ================================================================

    // ID 100: RPM, MAP(×1000→bar), TPS(×10), Gear
    payload[0] = d.rpm & 0xFF;
    payload[1] = d.rpm >> 8;
    int16_t rdMap = d.map * 1000;
    payload[2] = rdMap & 0xFF;
    payload[3] = rdMap >> 8;
    int16_t rdTps = d.tps * 10;
    payload[4] = rdTps & 0xFF;
    payload[5] = rdTps >> 8;
    payload[6] = d.gear & 0xFF;
    payload[7] = (d.gear >> 8) & 0xFF;
    sendRDBinaryFrame(100, payload);

    // ID 102: OilPress, FuelPress, WaterPress, BrakePress (×1000→bar)
    int16_t op = d.oilPressure * 1000;
    payload[0] = op & 0xFF; payload[1] = op >> 8;
    int16_t fp = d.fuelPressure * 1000;
    payload[2] = fp & 0xFF; payload[3] = fp >> 8;
    int16_t wp = d.waterPressure * 1000;
    payload[4] = wp & 0xFF; payload[5] = wp >> 8;
    int16_t bp = d.brakePressure * 1000;
    payload[6] = bp & 0xFF; payload[7] = bp >> 8;
    sendRDBinaryFrame(102, payload);

    // ID 103: Lambda(×100), InjA(×100), LambdaCorr(×100), FuelFlow(×10)
    int16_t lam = d.exhaustO2 * 100;
    payload[0] = lam & 0xFF; payload[1] = lam >> 8;
    int16_t injA = d.injTimeA * 100;
    payload[2] = injA & 0xFF; payload[3] = injA >> 8;
    int16_t lCorr = d.lambdaCorrection * 100;
    payload[4] = lCorr & 0xFF; payload[5] = lCorr >> 8;
    int16_t fFlow = d.fuelFlowTotal * 10;
    payload[6] = fFlow & 0xFF; payload[7] = fFlow >> 8;
    sendRDBinaryFrame(103, payload);

    // ID 107: DutyA(×10), DutyB(×10), Fan, WGPress(×1000)
    uint16_t duA = d.dutyA * 10;
    payload[0] = duA & 0xFF; payload[1] = duA >> 8;
    payload[2] = 0; payload[3] = 0; // DutyB
    payload[4] = 0; payload[5] = 0; // Fan
    int16_t wgP = d.wgPressure * 1000;
    payload[6] = wgP & 0xFF; payload[7] = wgP >> 8;
    sendRDBinaryFrame(107, payload);

    // ID 108: DiffFuelPress (placeholder)
    memset(payload, 0, 8);
    sendRDBinaryFrame(108, payload);

    // ID 110: Speed(×10), FuelFlowLPH(×10), FuelEffKML(×10), AvgKmL(×10)
    int16_t spd = d.speed * 10;
    payload[0] = spd & 0xFF; payload[1] = spd >> 8;
    int16_t flph = d.fuelFlowLPH * 10;
    payload[2] = flph & 0xFF; payload[3] = flph >> 8;
    int16_t fkml = d.fuelEffKML * 10;
    payload[4] = fkml & 0xFF; payload[5] = fkml >> 8;
    int16_t avgk = d.avgKmL * 10;
    payload[6] = avgk & 0xFF; payload[7] = avgk >> 8;
    sendRDBinaryFrame(110, payload);

    // ================================================================
    // FRAMES LENTOS (5 Hz) — a cada 4ª chamada
    // ================================================================
    if (slowCounter == 0) {

        // ID 101: EngTemp, AirTemp, OilTemp, TransTemp (×10→°C)
        int16_t engT = d.engineTemp * 10;
        payload[0] = engT & 0xFF; payload[1] = engT >> 8;
        int16_t airT = d.airTemp * 10;
        payload[2] = airT & 0xFF; payload[3] = airT >> 8;
        int16_t oilT = d.oilTemp * 10;
        payload[4] = oilT & 0xFF; payload[5] = oilT >> 8;
        int16_t transT = d.transTemp * 10;
        payload[6] = transT & 0xFF; payload[7] = transT >> 8;
        sendRDBinaryFrame(101, payload);

        // ID 104: EGT 1-4 (°C direto)
        for (int i = 0; i < 4; i++) {
            int16_t ev = (int16_t)d.egt[i];
            payload[i * 2]     = ev & 0xFF;
            payload[i * 2 + 1] = ev >> 8;
        }
        sendRDBinaryFrame(104, payload);

        // ID 105: EGT 5-8 (°C direto)
        for (int i = 0; i < 4; i++) {
            int16_t ev = (int16_t)d.egt[i + 4];
            payload[i * 2]     = ev & 0xFF;
            payload[i * 2 + 1] = ev >> 8;
        }
        sendRDBinaryFrame(105, payload);

        // ID 106: Advance(×10), Battery(×100)
        int16_t adv = d.advance * 10;
        payload[0] = adv & 0xFF; payload[1] = adv >> 8;
        uint16_t bat = d.battery * 100;
        payload[2] = bat & 0xFF; payload[3] = bat >> 8;
        payload[4] = 0; payload[5] = 0;
        payload[6] = 0; payload[7] = 0;
        sendRDBinaryFrame(106, payload);

        // ID 109: 2-Step(0/1), Antilag/3-Step(0/1)
        uint16_t twoS = d.twoStepState;
        uint16_t threeS = (d.switchState & (1 << 2)) ? 1 : 0;
        payload[0] = twoS & 0xFF;   payload[1] = twoS >> 8;
        payload[2] = threeS & 0xFF; payload[3] = threeS >> 8;
        payload[4] = 0; payload[5] = 0;
        payload[6] = 0; payload[7] = 0;
        sendRDBinaryFrame(109, payload);
    }
}

// ============================================================
// BT Command Handler (Uplink ← RealDash)
// ============================================================
// Processa frames RealDash CAN recebidos via BT.
// Suporta:
//   0x800 → Next Screen
//   0x801 → Prev Screen
//   0x500-0x50F → Virtual Buttons ON/OFF/Toggle
//
// Botões 3 e 4 são toggles (Antilag, 2-Step).
// Demais botões são momentâneos com auto-release de 200ms.
// ============================================================
void checkBTCommands() {
    bool connected = SerialBT.hasClient();

    // Atualiza flag de conexão BT
    if (lockCarData(5)) {
        carData.btConnected = connected;

        // Auto-release de botões momentâneos (200ms)
        unsigned long now = millis();
        for (int b = 0; b < 8; b++) {
            // Exceção: botões 3 (Antilag) e 4 (2-Step) são toggles
            if (b == 2 || b == 3) continue;

            if ((carData.switchState & (1 << b)) &&
                (now - buttonReleaseTimers[b] > 200)) {
                carData.switchState &= ~(1 << b);
            }
        }
        unlockCarData();
    }

    if (!connected) return;

    // Processa frames RealDash CAN do buffer serial
    while (SerialBT.available() > 0) {
        int firstByte = SerialBT.peek();

        if (firstByte == 0x44) {
            // Possível início de frame RealDash
            if (SerialBT.available() >= 17) {
                uint8_t buf[17];
                SerialBT.readBytes(buf, 17);

                // Valida assinatura: 0x44 0x33 0x22 0x11
                if (buf[1] == 0x33 && buf[2] == 0x22 && buf[3] == 0x11) {
                    uint32_t frameId = buf[4] | ((uint32_t)buf[5] << 8) |
                                       ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
                    uint32_t cleanId = frameId & 0x1FFFFFFF;

                    // ---- Navegação de Tela ----
                    if (cleanId == 0x800) {
                        advanceScreen(1);
                    }
                    else if (cleanId == 0x801) {
                        advanceScreen(-1);
                    }
                    // ---- Botões Virtuais do SwitchPanel ----
                    else if (lockCarData(5)) {
                        // Botão 1
                        if      (cleanId == 0x501) carData.switchState |=  (1 << 0);
                        else if (cleanId == 0x500) carData.switchState &= ~(1 << 0);
                        // Botão 2
                        else if (cleanId == 0x503) carData.switchState |=  (1 << 1);
                        else if (cleanId == 0x502) carData.switchState &= ~(1 << 1);
                        // Botão 3 — Toggle (Antilag)
                        else if (cleanId == 0x505) carData.switchState ^=  (1 << 2);
                        else if (cleanId == 0x504) carData.switchState &= ~(1 << 2);
                        // Botão 4 — Toggle (2-Step)
                        else if (cleanId == 0x507) carData.switchState ^=  (1 << 3);
                        else if (cleanId == 0x506) carData.switchState &= ~(1 << 3);
                        // Botão 5
                        else if (cleanId == 0x509) carData.switchState |=  (1 << 4);
                        else if (cleanId == 0x508) carData.switchState &= ~(1 << 4);
                        // Botão 6
                        else if (cleanId == 0x50B) carData.switchState |=  (1 << 5);
                        else if (cleanId == 0x50A) carData.switchState &= ~(1 << 5);
                        // Botão 7
                        else if (cleanId == 0x50D) carData.switchState |=  (1 << 6);
                        else if (cleanId == 0x50C) carData.switchState &= ~(1 << 6);
                        // Botão 8
                        else if (cleanId == 0x50F) carData.switchState |=  (1 << 7);
                        else if (cleanId == 0x50E) carData.switchState &= ~(1 << 7);

                        // Atualiza timer de auto-release para botões momentâneos
                        for (int b = 0; b < 8; b++) {
                            if (cleanId == (uint32_t)(0x501 + (b * 2))) {
                                buttonReleaseTimers[b] = millis();
                            }
                        }

                        unlockCarData();
                    }
                }
            } else {
                // Frame incompleto — espera mais dados
                break;
            }
        }
        else {
            // Byte fora de sincronia — descarta
            SerialBT.read();
        }
    }
}
