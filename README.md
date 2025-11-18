# Monitor IoT de Postura e Foco -- ESP32

Este projeto utiliza um **ESP32**, um **sensor PIR HC‑SR501**, **LED
RGB**, **buzzer** e um **botão** para monitorar automaticamente o tempo
sentado, enviar alertas e registrar sessões via **MQTT**.

## 👥 Participantes do Grupo

- Arthur Marcio de Barros Silva Rm5633559
-  Mayke santos Rm562680
-  Leonardo Yukio Iwagoe Ribeiro Rm562864

------------------------------------------------------------------------

## 🚀 Funcionalidades Principais

-   Detecta automaticamente quando o usuário está **sentado** ou
    **ausente**.\
-   Gera **alertas** após longos períodos sentado.\
-   Permite iniciar **pausas de 5 minutos** via botão.\
-   Publica dados em tempo real via MQTT:
    -   status atual\
    -   sessões concluídas\
    -   alertas\
    -   pausas\
    -   estatísticas de foco\
-   LEDs indicam estados (sentado, alerta, pausa, pronto).\
-   Buzzer emite sinais sonoros para início/fim de sessões, pausas e
    alertas.

------------------------------------------------------------------------

## 🧰 Hardware Necessário

-   ESP32\
-   Sensor PIR HC‑SR501\
-   LED RGB (3 pinos)\
-   Buzzer\
-   Botão (push button)\
-   Jumpers\
-   Protoboard

------------------------------------------------------------------------

## 🌐 Conexão WiFi

O ESP32 conecta automaticamente à rede:

    SSID: Wokwi-GUEST
    Senha: (vazia)

------------------------------------------------------------------------

## 📡 MQTT

O código utiliza o broker:

    Broker: test.mosquitto.org
    Porta: 1883
    ClientID: ESP32_PostureMonitor_PIR

### **Tópicos utilizados**

  Tópico                  Função
  ----------------------- --------------------------------------
  `iot/posture/status`    Envia atualizações gerais a cada 10s
  `iot/posture/session`   Publica sessões concluídas
  `iot/posture/alert`     Envia estado de alerta
  `iot/posture/pause`     Informa início e fim de pausas

------------------------------------------------------------------------

## 🕹️ Funcionamento Resumido

### ✔️ Detecção de presença

-   PIR detecta movimento por tempo contínuo → **inicia sessão**\
-   Ausência prolongada → **encerra sessão**

### ⏱️ Alertas

-   Após **90 minutos** sentado → alerta sonoro + LED vermelho\
-   Novo alerta a cada 30 min se permanecer sentado

### ☕ Pausas

-   Botão inicia pausa de **5 minutos**
-   Pode encerrar pausa antes do tempo
-   Durante pausa, sensores são ignorados
-   LED amarelo indica pausa

### 📊 Estatísticas

O ESP32 contabiliza: - Tempo total sentado no dia\
- Quantidade de sessões\
- Número de pausas\
- Quantas vezes o PIR detectou movimento

------------------------------------------------------------------------

## 🖼️ LED -- Códigos de Cor

  Cor        Estado
  ---------- ------------------------
  Azul       Sessão ativa (sentado)
  Verde      Pronto / Em pé
  Vermelho   Alerta
  Amarelo    Pausa

------------------------------------------------------------------------

## 🔊 Buzzer -- Sinalizações

-   **Sessão iniciada** → tom médio\
-   **Sessão encerrada** → tom grave\
-   **Pausa iniciada** → escala crescente\
-   **Pausa encerrada** → escala aguda\
-   **Alerta** → 5 bipes repetidos

------------------------------------------------------------------------

## 📁 Código Completo

O código completo está incluído no repositório na pasta principal.

------------------------------------------------------------------------

## 📽️ Vídeo Explicativo

Demonstração completa do sistema:\
👉 https://youtu.be/4JTZCAZo4PM


