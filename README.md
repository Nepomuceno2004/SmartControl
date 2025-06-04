# SmartControl

Este projeto utiliza o microcontrolador **Raspberry Pi Pico W** para controle de iluminação em diferentes ambientes simulados por uma matriz de LEDs. A comunicação é feita via **MQTT**, permitindo o controle remoto por meio de aplicativos ou dashboards conectados à rede Wi-Fi, com troca de mensagens entre o dispositivo e um **broker MQTT**.

## Funcionalidades

- Conexão à rede Wi-Fi.
- Comunicação com broker MQTT para controle remoto das luzes.
- Publicação e assinatura de tópicos MQTT:
  - `/garagem`
  - `/sala`
  - `/quarto`
  - `/escritorio`
  - `/aquario/temperatura`
  - `/aquario/ph`
- Simulação da iluminação de:
  - Garagem
  - Sala
  - Quarto
  - Escritório
- Visualização das luzes na matriz de LEDs endereçáveis (WS2812).
- Suporte ao modo BOOTSEL via botão físico (GPIO 6).

## Como Usar

1. **Configure o Wi-Fi e Broker MQTT:**
   - No código, substitua `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_SERVER`, `MQTT_USERNAME` e `MQTT_PASSWORD` com suas credenciais e endereço do broker.

2. **Compile o Projeto:**
   - Use o SDK do Raspberry Pi Pico e o `cmake` para compilar o projeto com as bibliotecas necessárias (`lwIP`, `cyw43`, `mqtt`, etc.).

3. **Carregue o Programa:**
   - Envie o arquivo `.uf2` gerado para o Raspberry Pi Pico W em modo BOOTSEL.

4. **Conecte-se à Rede:**
   - Verifique o terminal serial para confirmar a conexão Wi-Fi e comunicação com o broker.

5. **Controle via App ou Dashboard:**
   - Publique mensagens nos tópicos MQTT para ligar/desligar as luzes e visualizar os dados simulados do aquário.

## Componentes Utilizados

- Raspberry Pi Pico W  
- Módulo Wi-Fi CYW43439 (integrado)  
- Matriz de LEDs WS2812 (5x5)  
- Botão físico (GPIO 6)  
- Biblioteca lwIP para TCP/IP  
- Biblioteca MQTT (paho.mqtt ou similar)  
- Biblioteca de controle de LEDs `matrizLed.h`

## Autor  
**Matheus Nepomuceno Souza**

