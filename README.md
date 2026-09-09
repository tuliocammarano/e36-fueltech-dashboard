# E36 FuelTech Telemetry Dashboard & Gateway v2.0

Painel de telemetria para BMW E36 (ou outros veículos) com injeção FuelTech, desenhado para ler os dados da rede CAN a 1 Mbps e exibi-los em um display OLED, além de transmitir via Bluetooth para o aplicativo RealDash.

O sistema possui arquitetura avançada com **Dual-Core**, mas foi feito para ser muito **simples de montar e instalar**!

---

## 🔌 O que ligar aonde (Guia de Montagem)

Para montar o projeto, você vai precisar de:
- **1x ESP32** (Recomendado: WROOM DevKit V1 de 30 pinos)
- **1x Módulo CAN MCP2515** (com transceiver TJA1050)
- **1x Display OLED SSD1305** (128x32 SPI)
- **2x Botões simples** (Push buttons)
- **1x Módulo Regulador de Tensão Step-Down** (ex: LM2596 - converte 12V do carro para 5V)

### 1. Alimentação (Energia)
O painel foi programado para "dormir" (Deep Sleep) quando a FuelTech desliga e "acordar" sozinho quando a FuelTech liga. Por isso, ele deve ser ligado no 12V contínuo da bateria do carro.

* **Carro (12V Direto Bateria)** ➔ Entrada `IN+` do Step-Down
* **Carro (GND/Lataria)** ➔ Entrada `IN-` do Step-Down
* **Saída 5V (`OUT+`) do Step-Down** ➔ Pino `VIN` (ou `5V`) do ESP32 **E** Pino `VCC` do MCP2515
* **Saída GND (`OUT-`) do Step-Down** ➔ Pino `GND` do ESP32 **E** Pino `GND` do MCP2515

### 2. Módulo CAN (MCP2515) ➔ ESP32
Este módulo lê os dados que vêm da injeção FuelTech.

| Pino MCP2515 | Pino ESP32 | Observação / Função |
| :--- | :--- | :--- |
| **VCC** | 5V | Alimentação (vem do Step-Down) |
| **GND** | GND | Terra |
| **CS** | **GPIO 5** | Seleção do chip |
| **SO (MISO)** | **GPIO 19** | Dados |
| **SI (MOSI)** | **GPIO 23** | Dados |
| **SCK** | **GPIO 18** | Clock |
| **INT** | **GPIO 4** | Importante para acordar o painel automaticamente |

**Ligação na FuelTech:**
* **CAN H** do MCP2515 ➔ Fio **CAN HI** da FuelTech
* **CAN L** do MCP2515 ➔ Fio **CAN LO** da FuelTech

### 3. Display OLED (SSD1305) ➔ ESP32
A tela onde os dados e alertas serão exibidos.

| Pino Display | Pino ESP32 |
| :--- | :--- |
| **VCC** | 3.3V (pino 3V3 do ESP32) |
| **GND** | GND |
| **CLK / D0** | **GPIO 14** |
| **DATA / D1 / MOSI** | **GPIO 13** |
| **CS** | **GPIO 15** |
| **DC / A0** | **GPIO 27** |
| **RESET / RST** | **GPIO 33** |

### 4. Botões (Navegação de Telas)
Os botões não precisam de resistores, basta ligar um lado no ESP32 e o outro lado no GND (Terra).

| Botão | Pino ESP32 | Onde ligar a outra perna |
| :--- | :--- | :--- |
| **Próxima Tela (NEXT)** | **GPIO 26** | GND |
| **Tela Anterior (PREV)**| **GPIO 25** | GND |

---

## 🚀 Como Instalar o Software (Passo a Passo)

1. **Baixe e prepare o código**
   * Faça o download ou clone este repositório no seu computador.
   * Na pasta do projeto, encontre o arquivo `config.example.h`, faça uma cópia dele e renomeie a cópia para `config.h`.
   * Abra esse `config.h` e coloque o nome e senha do seu WiFi (isso serve apenas para fazer atualizações sem fio no futuro).

2. **Prepare a Arduino IDE**
   * Abra a Arduino IDE e certifique-se de que a placa ESP32 está instalada (em Boards Manager).
   * Vá em *Sketch -> Include Library -> Manage Libraries* e instale:
     * `U8g2` (por olikraus)
     * `mcp2515` (por autowp)

3. **Configuração da Placa**
   * Em *Tools -> Board*, selecione **ESP32 Dev Module**.
   * Em *Tools -> Partition Scheme*, escolha **Default 4MB with spiffs** ou **Min SPIFFS (1.9MB APP with OTA)**.

4. **Gravação**
   * Conecte o ESP32 via cabo USB, selecione a porta (Port) correta.
   * Clique no botão **Upload** e aguarde finalizar. Pronto!

---

## 📱 Usando com o RealDash no Celular/Tablet

Este projeto também envia os dados da FuelTech para o aplicativo RealDash via Bluetooth!

1. Copie o arquivo `realdash_e36.xml` que está nesta pasta para o seu celular/tablet.
2. Abra o RealDash, vá em **Connections** -> **Add** -> **RealDash CAN** -> **Bluetooth**.
3. Selecione o dispositivo Bluetooth chamado `E36_Dash_BT`.
4. Quando pedir o arquivo de definição (channel definition), escolha o arquivo XML que você copiou no Passo 1.

---

## 🤓 Detalhes Técnicos e Arquitetura v2.0
*(Seção para desenvolvedores e curiosos)*

### Dual-Core FreeRTOS

| Core | Task | Prioridade | Stack | Responsabilidades |
|------|------|-----------|-------|-------------------|
| **Core 1** | CAN_Task | 2 (alta) | 3 KB | Polling MCP2515, FTCAN 2.0 decoder, cálculo de consumo, SwitchPanel |
| **Core 0** | HMI_Task | 1 (menor) | 4 KB | Display OLED 20 FPS, Bluetooth SPP (RealDash), botões físicos |
| **Core 0** | loop() | 1 | default | ArduinoOTA, WiFi Track/Pit Mode |

* **Sincronização**: Uso de Mutex (FreeRTOS) para garantir leitura/escrita segura dos dados entre os núcleos.
* **Track Mode / Pit Mode**: O ESP32 alterna automaticamente entre Bluetooth (para pista) e WiFi (quando o motor para, para receber atualizações OTA), compartilhando a mesma antena.
* **Deep Sleep**: Após 30 segundos sem atividade na CAN, desliga o display e dorme, consumindo pouquíssima bateria. O MCP2515 acorda o ESP32 via pino de interrupção (INT) quando o carro liga.

### 📊 Dados Monitorados
* RPM, MAP, TPS, Marcha (Gear)
* Temperaturas (Motor, Óleo, Ar, Câmbio)
* EGT 1-8 (com alerta de circuito aberto)
* Pressões (Óleo, Combustível, Água, Freio)
* Lambda, Tempo de Injeção
* Velocidades
* **Consumo Calculado:** O código processa o Duty Cycle dos bicos (ex: 994cc/min) com filtro EMA para gerar leitura estável de L/h ou km/L no painel.

## 📄 Licença
Projeto pessoal — uso livre para fins educacionais.
