// ============================================================
// bt_controller.h — Interface do Bluetooth Classic (RealDash)
// ============================================================
#ifndef BT_CONTROLLER_H
#define BT_CONTROLLER_H

// Inicializa Bluetooth Classic SPP
void initBluetooth();

// Envia todos os canais de telemetria via protocolo RealDash CAN (44).
// Frames rápidos (20Hz) e lentos (5Hz). Chamado pelo HMI Task.
void streamTelemetryBT();

// Processa comandos recebidos do RealDash (troca de tela, botões virtuais).
// Chamado pelo HMI Task a 10Hz.
void checkBTCommands();

// Retorna true se há um client BT conectado
bool isBTConnected();

#endif // BT_CONTROLLER_H
