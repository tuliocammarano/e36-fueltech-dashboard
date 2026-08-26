// ============================================================
// config.h — Configurações centralizadas do E36 Dashboard
// ============================================================
#ifndef CONFIG_H
#define CONFIG_H

// ---- Pinos do Hardware ----
#define MCP2515_CS_PIN  5       // Chip Select do MCP2515 (CAN)
#define OLED_MOSI       13
#define OLED_CLK        14
#define OLED_DC         27
#define OLED_CS         15
#define OLED_RESET      33

// Display
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   32

// CAN
#define CAN_BITRATE     CAN_1000KBPS
#define CAN_OSC         MCP_8MHZ

// ==== WiFi Settings ====
#define WIFI_SSID       "SEU_WIFI_AQUI"
#define WIFI_PASSWORD   "SUA_SENHA_AQUI"

// ==== NAS FTP Settings ====
// Configuração para envio dos logs de telemetria
#define FTP_HOST               "192.168.X.X"
#define FTP_PORT               21
#define FTP_USER               "seu_usuario_ftp"
#define FTP_PASSWORD           "sua_senha_ftp"
#define FTP_UPLOAD_PATH        "/"

// ---- BLE ----
#define BLE_DEVICE_NAME "E36-Dash"
// Nordic UART Service UUIDs (compatível com Tasker BLE Serial plugin)
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ---- Timing (ms) ----
#define DISPLAY_UPDATE_MS   50      // 20 FPS
#define LOG_INTERVAL_MS     1000    // 1 Hz
#define WIFI_CHECK_MS       30000   // 30 segundos
#define BLE_CHECK_MS        100     // 100ms
#define CAN_TIMEOUT_MS      2000   // Timeout p/ considerar CAN offline

// ---- Limiares de Alerta ----
#define EGT_WARNING         850.0f  // °C
#define EGT_CRITICAL        950.0f  // °C
#define EGT_ERROR_VALUE     1050.0f // °C (termopar desconectado)
#define OIL_PRESS_MIN       1.0f    // bar
#define ENGINE_TEMP_WARNING 100.0f  // °C
#define ENGINE_TEMP_CRITICAL 110.0f // °C
#define OIL_TEMP_WARNING    130.0f  // °C

// ---- Data Logger ----
#define MAX_LOG_FILES       10      // Máximo de arquivos antes de sobrescrever
#define LOG_FLUSH_BYTES     512     // Buffer antes de flush
#define MAX_FS_USAGE_PCT    90      // % máximo de uso do LittleFS

// ---- Display ----
#define NUM_SCREENS         6       // Número de telas disponíveis

// ---- CAN IDs (Simplified Packets — FT600/550/450) ----
#define FTCAN_ID_0x600  0x14080600  // TPS, MAP, Air Temp, Engine Temp
#define FTCAN_ID_0x601  0x14080601  // Oil Press, Fuel Press, Water Press, Gear
#define FTCAN_ID_0x602  0x14080602  // Exhaust O2, RPM, Oil Temp, Pit Limit
#define FTCAN_ID_0x603  0x14080603  // Wheel Speeds (FR, FL, RR, RL)
#define FTCAN_ID_0x604  0x14080604  // Traction Control
#define FTCAN_ID_0x605  0x14080605  // Shock Sensors
#define FTCAN_ID_0x606  0x14080606  // G-force, Yaw-rate
#define FTCAN_ID_0x607  0x14080607  // Lambda Corr, Fuel Flow, Inj Time A/B
#define FTCAN_ID_0x608  0x14080608  // Oil Temp, Trans Temp, Fuel Cons, Brake Press

// CAN IDs EGT-4 (Arduino emulando protocolo FT)
#define EGT4_MODEL_A    0x02400000  // Canais 1-4
#define EGT4_MODEL_B    0x02480000  // Canais 5-8

#endif // CONFIG_H
