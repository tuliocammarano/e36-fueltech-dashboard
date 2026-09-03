#ifndef BT_CONTROLLER_H
#define BT_CONTROLLER_H

#include "config.h"
#include "data_store.h"
#include <BluetoothSerial.h>
#include "display_manager.h" // Necessário para nextScreen()/prevScreen()

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

extern CarData carData;
BluetoothSerial SerialBT;
unsigned long buttonReleaseTimers[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void initBLE() {
    // Inicializa o Bluetooth Clássico com o nome E36_Dash_BT
    SerialBT.begin("E36_Dash_BT"); 
}

void sendRDBinaryFrame(uint32_t frameId, const uint8_t* data) {
    if (!SerialBT.hasClient()) return;
    
    // O RealDash exige que o bit 31 (CAN_EFF_FLAG) esteja ativado para IDs de 29-bits (Extended)
    if (frameId > 0x7FF) {
        frameId |= 0x80000000;
    }
    
    uint8_t buf[17];
    buf[0] = 0x44; buf[1] = 0x33; buf[2] = 0x22; buf[3] = 0x11;
    buf[4] = frameId & 0xFF;
    buf[5] = (frameId >> 8) & 0xFF;
    buf[6] = (frameId >> 16) & 0xFF;
    buf[7] = (frameId >> 24) & 0xFF;
    memcpy(&buf[8], data, 8);
    
    uint8_t chk = 0;
    for (int i = 0; i < 16; i++) {
        chk += buf[i];
    }
    buf[16] = chk;
    
    SerialBT.write(buf, 17);
}

void streamLogToBT() {
    carData.bleConnected = SerialBT.hasClient();
    if (!carData.bleConnected) return;
    
    static uint8_t slowCounter = 0;
    slowCounter++;
    if (slowCounter >= 4) slowCounter = 0; // Divide por 4 (20Hz -> 5Hz)

    uint8_t payload[8];
    
    // --- PACOTES RÁPIDOS (20Hz) - Evita "engasgos" no Bluetooth ---
    
    // ID 100: RPM, MAP(kPa*10 -> V/1000), TPS(x10), Gear
    payload[0] = carData.rpm & 0xFF; payload[1] = carData.rpm >> 8;
    int16_t rdMap = carData.map * 1000;
    payload[2] = rdMap & 0xFF; payload[3] = rdMap >> 8;
    int16_t rdTps = carData.tps * 10;
    payload[4] = rdTps & 0xFF; payload[5] = rdTps >> 8;
    payload[6] = carData.gear & 0xFF; payload[7] = carData.gear >> 8;
    sendRDBinaryFrame(100, payload);

    // ID 102: OilPress, FuelPress, WaterPress, BrakePress (all x1000)
    int16_t op = carData.oilPressure * 1000;
    payload[0] = op & 0xFF; payload[1] = op >> 8;
    int16_t fp = carData.fuelPressure * 1000;
    payload[2] = fp & 0xFF; payload[3] = fp >> 8;
    int16_t wp = carData.waterPressure * 1000;
    payload[4] = wp & 0xFF; payload[5] = wp >> 8;
    int16_t bp = carData.brakePressure * 1000;
    payload[6] = bp & 0xFF; payload[7] = bp >> 8;
    sendRDBinaryFrame(102, payload);
    
    // ID 103: Lambda(x100), InjA(x100), LambdaCorr(x100), FuelFlow(x10)
    int16_t lambda = carData.exhaustO2 * 100;
    payload[0] = lambda & 0xFF; payload[1] = lambda >> 8;
    int16_t injA = carData.injTimeA * 100;
    payload[2] = injA & 0xFF; payload[3] = injA >> 8;
    int16_t lamCorr = carData.lambdaCorrection * 100;
    payload[4] = lamCorr & 0xFF; payload[5] = lamCorr >> 8;
    int16_t fFlow = carData.fuelFlowTotal * 10;
    payload[6] = fFlow & 0xFF; payload[7] = fFlow >> 8;
    sendRDBinaryFrame(103, payload);

    // ID 107: DutyA(x10), DutyB(x10), Fan, WGPress(x1000)
    uint16_t dutyA = carData.dutyA * 10;
    payload[0] = dutyA & 0xFF; payload[1] = dutyA >> 8;
    uint16_t dutyB = carData.dutyB * 10;
    payload[2] = dutyB & 0xFF; payload[3] = dutyB >> 8;
    uint16_t fan = carData.fanState;
    payload[4] = fan & 0xFF; payload[5] = fan >> 8;
    int16_t wg = carData.wgPressure * 1000;
    payload[6] = wg & 0xFF; payload[7] = wg >> 8;
    sendRDBinaryFrame(107, payload);

    // ID 108: DiffFuelPress(x1000)
    int16_t diffF = carData.diffFuelPressure * 1000;
    payload[0] = diffF & 0xFF; payload[1] = diffF >> 8;
    payload[2] = 0; payload[3] = 0; payload[4] = 0; payload[5] = 0; payload[6] = 0; payload[7] = 0;
    sendRDBinaryFrame(108, payload);

    // --- PACOTES LENTOS (5Hz) - Temperaturas e Bateria ---
    if (slowCounter == 0) {
        // ID 101: EngTemp, AirTemp, OilTemp, TransTemp
        int16_t engT = carData.engineTemp * 10;
        payload[0] = engT & 0xFF; payload[1] = engT >> 8;
        int16_t airT = carData.airTemp * 10;
        payload[2] = airT & 0xFF; payload[3] = airT >> 8;
        int16_t oilT = carData.oilTemp * 10;
        payload[4] = oilT & 0xFF; payload[5] = oilT >> 8;
        int16_t transT = carData.transTemp * 10;
        payload[6] = transT & 0xFF; payload[7] = transT >> 8;
        sendRDBinaryFrame(101, payload);

        // ID 104: EGT 1-4 (C)
        for (int i = 0; i < 4; i++) {
            int16_t egtVal = carData.egt[i];
            payload[i*2] = egtVal & 0xFF; payload[i*2 + 1] = egtVal >> 8;
        }
        sendRDBinaryFrame(104, payload);

        // ID 105: EGT 5-8 (C)
        for (int i = 0; i < 4; i++) {
            int16_t egtVal = carData.egt[i+4];
            payload[i*2] = egtVal & 0xFF; payload[i*2 + 1] = egtVal >> 8;
        }
        sendRDBinaryFrame(105, payload);
        
        // ID 106: Advance, Battery Voltage (x100)
        int16_t adv = carData.advance * 10;
        payload[0] = adv & 0xFF; payload[1] = adv >> 8;
        uint16_t bat = carData.battery * 100;
        payload[2] = bat & 0xFF; payload[3] = bat >> 8;
        payload[4] = 0; payload[5] = 0; payload[6] = 0; payload[7] = 0;
        sendRDBinaryFrame(106, payload);

        // ID 109: 2-Step e Antilag
        uint16_t twoStep = carData.twoStepState;
        payload[0] = twoStep & 0xFF; payload[1] = twoStep >> 8;
        
        // O sinal nativo de antilag da ECU não é claro, então usamos a memória do botão virtual!
        uint16_t threeStep = (carData.switchState & (1 << 2)) ? 1 : 0; 
        payload[2] = threeStep & 0xFF; payload[3] = threeStep >> 8;
        
        payload[4] = 0; payload[5] = 0; payload[6] = 0; payload[7] = 0;
        sendRDBinaryFrame(109, payload);
    }
}

void checkBLE() {
    carData.bleConnected = SerialBT.hasClient();
    
    // Processa o Auto-Release dos botões (200ms)
    unsigned long currentMs = millis();
    for (int b = 0; b < 8; b++) {
        // EXCEÇÃO: Botões 3 (Antilag) e 4 (2-Step) são switches e NÃO soltam sozinhos!
        if (b == 2 || b == 3) continue; 
        
        if ((carData.switchState & (1 << b)) && (currentMs - buttonReleaseTimers[b] > 200)) {
            carData.switchState &= ~(1 << b);
        }
    }

    while (SerialBT.available() > 0) {
        int firstByte = SerialBT.peek();
        
        // Verifica se é o início de um quadro binário do RealDash (0x44)
        if (firstByte == 0x44) {
            if (SerialBT.available() >= 17) {
                uint8_t buf[17];
                SerialBT.readBytes(buf, 17);
                
                // Valida a assinatura do RealDash CAN: 0x44 0x33 0x22 0x11
                if (buf[1] == 0x33 && buf[2] == 0x22 && buf[3] == 0x11) {
                    uint32_t frameId = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
                    uint32_t cleanId = frameId & 0x1FFFFFFF;
                    
                    if (cleanId == 0x800) {
                        nextScreen();
                    }
                    else if (cleanId == 0x801) {
                        prevScreen();
                    }
                    // --- BOTÕES DO SWITCHPANEL (1 a 8) ---
                    // Botão 1
                    else if (cleanId == 0x501) carData.switchState |= (1 << 0);
                    else if (cleanId == 0x500) carData.switchState &= ~(1 << 0);
                    // Botão 2
                    else if (cleanId == 0x503) carData.switchState |= (1 << 1);
                    else if (cleanId == 0x502) carData.switchState &= ~(1 << 1);
                    // Botão 3 (Toggles state on 505 press)
                    else if (cleanId == 0x505) carData.switchState ^= (1 << 2);
                    else if (cleanId == 0x504) carData.switchState &= ~(1 << 2); // Fallback manual OFF
                    // Botão 4 (Toggles state on 507 press)
                    else if (cleanId == 0x507) carData.switchState ^= (1 << 3);
                    else if (cleanId == 0x506) carData.switchState &= ~(1 << 3); // Fallback manual OFF
                    // Botão 5
                    else if (cleanId == 0x509) carData.switchState |= (1 << 4);
                    else if (cleanId == 0x508) carData.switchState &= ~(1 << 4);
                    // Botão 6
                    else if (cleanId == 0x50B) carData.switchState |= (1 << 5);
                    else if (cleanId == 0x50A) carData.switchState &= ~(1 << 5);
                    // Botão 7
                    else if (cleanId == 0x50D) carData.switchState |= (1 << 6);
                    else if (cleanId == 0x50C) carData.switchState &= ~(1 << 6);
                    // Botão 8
                    else if (cleanId == 0x50F) carData.switchState |= (1 << 7);
                    else if (cleanId == 0x50E) carData.switchState &= ~(1 << 7);
                    
                    // Se recebemos qualquer comando ON, atualizamos o timer daquele botão para o Auto-Release
                    for (int b = 0; b < 8; b++) {
                        if (cleanId == (0x501 + (b*2))) {
                            extern unsigned long buttonReleaseTimers[8];
                            buttonReleaseTimers[b] = millis();
                        }
                    }
                }
            } else {
                // Não tem 17 bytes completos ainda, sai do loop e espera
                break; 
            }
        } 
        else {
            // Se perdeu a sincronia, descarta o byte
            SerialBT.read();
        }
    }
}

#endif // BT_CONTROLLER_H
