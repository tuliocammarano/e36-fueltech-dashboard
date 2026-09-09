# E36 FuelTech Telemetry Dashboard & Gateway v2.0

Painel de telemetria para BMW E36 com injeção FuelTech, redesenhado com arquitetura **Dual-Core FreeRTOS** para máxima confiabilidade no barramento CAN a 1 Mbps.

## 🏗️ Arquitetura v2.0

### Dual-Core FreeRTOS

| Core | Task | Prioridade | Stack | Responsabilidades |
|------|------|-----------|-------|-------------------|
| **Core 1** | CAN_Task | 2 (alta) | 8 KB | Polling MCP2515, FTCAN 2.0 decoder, cálculo de consumo, SwitchPanel |
| **Core 0** | HMI_Task | 1 (menor) | 8 KB | Display OLED 20 FPS, Bluetooth SPP (RealDash), botões físicos |
| **Core 0** | loop() | 1 | default | ArduinoOTA, WiFi Track/Pit Mode |

### Sincronização
- **CarData** (Single Source of Truth) protegida por **FreeRTOS Mutex**
- CAN Task **escreve** → HMI Task **lê snapshots** thread-safe
- Timeout do mutex: 5-10ms (não-blocante, previne deadlocks)

### Track Mode / Pit Mode (Coexistência WiFi + BT)
O ESP32 compartilha **um único rádio** entre WiFi e Bluetooth via TDM.

- **Track Mode** (default): WiFi **OFF** → Bluetooth com bandwidth total
- **Pit Mode** (auto): WiFi **ON** quando RPM = 0 por >5 segundos → OTA disponível
- Transição automática com **histerese** para evitar cycling durante cranking

## 📊 Dados Monitorados

| Canal | Fonte | Atualização |
|-------|-------|-------------|
| RPM, MAP, TPS, Gear | Simplified 0x600-0x603 | ~100 Hz |
| Temperaturas (Motor, Óleo, Ar, Câmbio) | Simplified 0x600, 0x602, 0x608 | ~100 Hz |
| EGT 1-8 (com alerta circuito aberto 0x20D0) | EGT-4 Protocol | ~10 Hz |
| Pressões (Óleo, Combustível, Água, Freio) | Simplified 0x601, 0x608 | ~100 Hz |
| Lambda, Inj Time, Lambda Correction | Simplified 0x602, 0x607 | ~100 Hz |
| Duty Cycle, Advance, Battery, 2-Step | Segmented FTCAN 2.0 | ~50 Hz |
| Consumo (L/h e km/L com EMA 80/20) | **Calculado** (994 cc/min × 6 cil) | ~100 Hz |
| Velocidade (Roda RR) | Simplified 0x603 | ~100 Hz |

## ⛽ Cálculo de Consumo

- **Motor**: BMW M50 — 6 cilindros
- **Bicos**: 994 cc/min
- **Fórmula**: `Duty = PW_ms × RPM / 120000` → `L/h = 6 × Duty × 994 × 0.06`
- **Filtro**: EMA (Exponential Moving Average) α=0.2 → suaviza transições Decel/DFCO
- **Display dinâmico**: L/h (velocidade < 1 km/h) → km/L (velocidade ≥ 1 km/h)

## 🔌 Hardware

- **MCU**: ESP32 DevKit V1 (WROOM, 4MB Flash)
- **CAN**: MCP2515 + TJA1050 (Hardware SPI: VSPI)
- **Display**: OLED SSD1305 128x32 (Software SPI — **sem conflito** com CAN)
- **Alimentação**: Step-down 12V → 5V

### Pinagem

**MCP2515 (Hardware SPI — VSPI):**
| Sinal | GPIO |
|-------|------|
| SCK | 18 |
| MISO | 19 |
| MOSI | 23 |
| CS | 5 |

**OLED SSD1305 (Software SPI):**
| Sinal | GPIO |
|-------|------|
| CLK | 14 |
| DATA/MOSI | 13 |
| CS | 15 |
| DC | 27 |
| RESET | 33 |

**Botões:**
| Sinal | GPIO |
|-------|------|
| NEXT | 26 |
| PREV | 25 |

## 🚀 Como Instalar

1. Clone o repositório
2. Copie `config.example.h` → `config.h`
3. Edite `config.h` com suas credenciais WiFi
4. Na Arduino IDE, instale as bibliotecas:
   - `U8g2` (por olikraus)
   - `mcp2515` (por autowp)
5. **Board**: ESP32 Dev Module
6. **Partition Scheme**: `Default 4MB with spiffs` ou `Min SPIFFS (1.9MB APP with OTA)`
7. Compile e faça upload!

## 📱 RealDash

1. Copie `realdash_e36.xml` para o celular
2. No RealDash: Connections → Add → RealDash CAN → Bluetooth → `E36_Dash_BT`
3. Selecione o arquivo XML como channel definition

## 📦 Dependências

| Biblioteca | Autor | Uso |
|-----------|-------|-----|
| U8g2 | olikraus | Display SSD1305 128x32 |
| mcp2515 | autowp | Controlador CAN MCP2515 |
| BluetoothSerial | Espressif (built-in) | BT Classic SPP |
| WiFi | Espressif (built-in) | OTA |
| ArduinoOTA | Arduino (built-in) | Updates |
| SPI | Arduino (built-in) | Hardware SPI |

## 🔄 Diferenças da v1.0

| Aspecto | v1.0 | v2.0 |
|---------|------|------|
| Arquitetura | Single-core cooperativo | **Dual-core FreeRTOS** |
| Thread safety | Nenhum | **Mutex-protected CarData** |
| Código | Tudo em `.h` (ODR violation) | **`.h` + `.cpp` separados** |
| WiFi em pista | Sempre ativo | **Track Mode (OFF)** |
| Consumo | Magic number no display | **Fórmula explícita + EMA central** |
| Segmented timeout | Nenhum | **100ms timeout** |
| Stack size | N/A (single-core) | **8 KB por task** |
| Boot | Blocking WiFi (até 10s) | **CAN inicia antes do display** |

## 📄 Licença

Projeto pessoal — uso livre para fins educacionais.
