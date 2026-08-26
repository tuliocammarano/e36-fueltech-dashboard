# E36 FuelTech Dashboard & Datalogger 🚗💻

Este projeto transforma um ESP32 em um painel OLED de telemetria completa para veículos equipados com injeções programáveis **FuelTech (Linha FT450 / FT550 / FT600)**. Ele lê os dados em tempo real da porta CAN da FuelTech, exibe no painel, grava logs de corrida (Datalogger) na memória flash e descarrega automaticamente os arquivos via WiFi (FTP) para um NAS/Servidor quando você chega em casa!

## 🌟 Principais Funcionalidades

- **FTCAN 2.0 Parser Integrado:** Decodifica nativamente o protocolo da FuelTech usando um módulo MCP2515.
- **Telas Dinâmicas (OLED 128x32):** Interface gráfica otimizada dividida em 6 telas:
  - 🏎️ Principal (RPM, MAP, O2, Marcha)
  - 🌡️ Temperaturas (Motor, Óleo, Ar)
  - 🔥 EGT (Termopares de 1 a 6 cilindros)
  - 💉 Injeção (Tempo de Injeção, Correção de Sonda, Fluxo, Consumo)
  - 🛢️ Pressões (Óleo, Combustível, Água)
  - 📶 Status do Sistema
- **Navegação via Bluetooth (Clássico SPP):** Troque as páginas do painel pelos botões do volante usando um app Android (Tasker) enviando os comandos `NEXT` e `PREV`.
- **Datalogger embutido (LittleFS):** Grava um arquivo `.CSV` de todos os sensores na memória interna do ESP32 a uma taxa de 1Hz (configurável).
- **Auto-Sync via WiFi + FTP:** Ao captar o WiFi da garagem, o painel sincroniza a hora via NTP e faz o upload invisível de todos os logs para o seu servidor/NAS caseiro!

## 🔌 Requisitos de Hardware

- **1x ESP32** (Versão clássica/WROOM - Memória mínima recomendada: 4MB Flash)
- **1x Display OLED 128x32** (Compatível com controlador SSD1305 / SSD1309)
- **1x Módulo CAN MCP2515** (Transceiver TJA1050)
- Step-down 12V -> 5V para alimentar o sistema no carro

## 🛠️ Pinagem (Esquema de Ligação)

**Display OLED (Ligado via Software SPI):**
- **SDA (Data/MOSI):** GPIO 13
- **SCL (Clock):** GPIO 14
- **DC:** GPIO 27
- **RES:** GPIO 33
- **CS:** GPIO 15

**Módulo MCP2515 (Ligado via Hardware SPI):**
- **MOSI:** GPIO 23
- **MISO:** GPIO 19
- **SCK:** GPIO 18
- **CS:** GPIO 5

## 🚀 Como instalar

1. Clone o repositório.
2. Copie o arquivo `config.example.h` e renomeie para `config.h`.
3. Abra o `config.h` e edite suas credenciais (Rede WiFi e Servidor FTP).
4. Na IDE do Arduino, instale as seguintes bibliotecas:
   - `U8g2` (por oliver)
   - `mcp2515` (por autowp)
5. **CRÍTICO:** Em Ferramentas > Partition Scheme, selecione **"Huge APP (3MB No OTA / 1MB SPIFFS)"** para o código caber na placa com o Bluetooth.
6. Compile e faça o upload!

## 📱 Comandos Bluetooth (Tasker / Serial Terminal)

Conecte no dispositivo Bluetooth chamado `E36-Dash` e envie strings de texto plano:
- `NEXT` -> Vai para a próxima tela
- `PREV` -> Volta uma tela
- `SCREEN:3` -> Pula diretamente para a tela de Injeção
- `LOG:START` / `LOG:STOP` -> Força a gravação de logs manualmente
- `UPLOAD` -> Tenta forçar a conexão WiFi e envio FTP do log atual
- `STATUS` -> Retorna um JSON com as leituras atuais dos sensores (útil para dashboards no próprio celular!)

---
