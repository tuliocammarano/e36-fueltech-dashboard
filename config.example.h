// ============================================================
// config.example.h — Template de configuração
// ============================================================
// Copie este arquivo para config.h e edite suas credenciais.
// O config.h está no .gitignore e não será commitado.
// ============================================================
#ifndef CONFIG_H
#define CONFIG_H

// ---- Pinos do Hardware ----
// MCP2515 (Hardware SPI: VSPI — GPIO 18/19/23)
#define MCP2515_CS_PIN  5

// OLED SSD1305 128x32 (Software SPI — GPIO 13/14, sem conflito com VSPI)
#define OLED_MOSI       13
#define OLED_CLK        14
#define OLED_DC         27
#define OLED_CS         15
#define OLED_RESET      33

// ---- Botões Físicos ----
#define BTN_NEXT_PIN    26
#define BTN_PREV_PIN    25

// ---- Display ----
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   32
#define NUM_SCREENS     6

// ---- CAN ----
#define CAN_BITRATE     CAN_1000KBPS
#define CAN_OSC         MCP_8MHZ

// ---- WiFi (Apenas para OTA) ----
// >>> ALTERE PARA SUA REDE <<<
#define WIFI_SSID       "SEU_WIFI_AQUI"
#define WIFI_PASSWORD   "SUA_SENHA_AQUI"
#define OTA_PASSWORD    "e36ota"

// ---- Bluetooth Clássico (RealDash SPP) ----
#define BT_DEVICE_NAME  "E36_Dash_BT"

// ---- Temporização ----
#define DISPLAY_UPDATE_MS   50
#define BLE_CHECK_MS        100
#define BT_LOG_INTERVAL_MS  50
#define WIFI_CHECK_MS       30000
#define CAN_TIMEOUT_MS      3000
#define SEGMENT_TIMEOUT_MS  100
#define DEBOUNCE_MS         300

// ---- FreeRTOS Task Config ----
#define CAN_TASK_STACK      8192
#define CAN_TASK_PRIORITY   2
#define HMI_TASK_STACK      8192
#define HMI_TASK_PRIORITY   1

// ---- Cálculo de Consumo (M50 6 Cilindros) ----
#define NUM_CYLINDERS           6
#define INJECTOR_FLOW_CC_MIN    994.0f
#define FUEL_DENSITY_KG_L       0.755f
#define EMA_ALPHA               0.2f

// ---- Limiares de Alerta ----
#define EGT_WARNING             850.0f
#define EGT_CRITICAL            950.0f
#define EGT_ERROR_VALUE         1050.0f
#define EGT_OPEN_CIRCUIT_RAW    0x20D0
#define OIL_PRESS_MIN           1.0f
#define ENGINE_TEMP_WARNING     100.0f
#define ENGINE_TEMP_CRITICAL    110.0f
#define OIL_TEMP_WARNING        130.0f

// ---- CAN IDs (Simplified Packets — FT600/550/450) ----
#define FTCAN_ID_0x600  0x14080600
#define FTCAN_ID_0x601  0x14080601
#define FTCAN_ID_0x602  0x14080602
#define FTCAN_ID_0x603  0x14080603
#define FTCAN_ID_0x604  0x14080604
#define FTCAN_ID_0x605  0x14080605
#define FTCAN_ID_0x606  0x14080606
#define FTCAN_ID_0x607  0x14080607
#define FTCAN_ID_0x608  0x14080608

// ---- CAN IDs SwitchPanel & EGT-4 ----
#define SWITCHPANEL_TX_ID   0x12200320
#define EGT4_MODEL_A        0x02400000
#define EGT4_MODEL_B        0x02480000

#endif // CONFIG_H
