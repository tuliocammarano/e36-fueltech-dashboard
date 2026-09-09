// ============================================================
// display_manager.h — Interface do gerenciador de display
// ============================================================
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

// Inicializa o display SSD1305 128x32 (Software SPI) e exibe boot logo
void initDisplay();

// Renderiza a tela atual baseado no snapshot thread-safe de CarData.
// Chamado pelo HMI Task (Core 0) a ~20 FPS.
void updateDisplay();

// Desliga o display (power save) antes do deep sleep.
void powerSaveDisplay();

#endif // DISPLAY_MANAGER_H
