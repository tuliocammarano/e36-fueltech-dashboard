// ============================================================
// can_reader.h — Interface do leitor CAN e decoder FTCAN 2.0
// ============================================================
#ifndef CAN_READER_H
#define CAN_READER_H

#include "config.h"

// Inicializa MCP2515 via Hardware SPI (1 Mbps, cristal 8 MHz)
void initCAN();

// Drena o buffer RX do MCP2515, decodifica FTCAN 2.0 e
// atualiza carData (mutex-protected). Chamado pelo CAN Task (Core 1).
void processCANMessages();

// Transmite o bitmask dos botões virtuais para a FuelTech via CAN.
// Chamado pelo CAN Task a 10 Hz.
void sendVirtualButtons();

// Reseta MCP2515 para limpar flags de INT antes do deep sleep.
void prepareCANForSleep();

// Timestamp da última atividade CAN com RPM > 0 (para deep sleep timer)
extern volatile unsigned long lastCanActivityMs;

#endif // CAN_READER_H
