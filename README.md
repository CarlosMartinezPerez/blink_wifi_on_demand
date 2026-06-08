# Blink com WiFi On Demand para ESP32

## Descrição

O **WiFi On Demand** é um projeto para ESP32 desenvolvido com ESP-IDF que demonstra uma estratégia de ativação seletiva da conectividade sem fio.

Diferentemente de aplicações convencionais, onde o Wi-Fi permanece ligado continuamente, este projeto mantém a rede desligada durante a maior parte do tempo. A interface Wi-Fi é ativada apenas quando o usuário pressiona um botão físico, permitindo acesso temporário a uma página web de configuração.

Através dessa interface web é possível:

* Visualizar o endereço IP atual do dispositivo.
* Monitorar a intensidade do sinal Wi-Fi (RSSI).
* Consultar o tempo de funcionamento (uptime).
* Alterar remotamente o período de piscada de um LED.
* Salvar a configuração em memória não volátil (NVS).
* Desligar novamente o Wi-Fi sem reiniciar o dispositivo.

O projeto demonstra conceitos de sistemas embarcados conectados, gerenciamento de energia, persistência de parâmetros e configuração remota via navegador.

---

## Principais Características

* ESP32 utilizando ESP-IDF.
* Wi-Fi ativado apenas sob demanda.
* Interface Web embarcada.
* Controle remoto do período de piscada de um LED.
* Armazenamento persistente em NVS.
* Monitoramento de RSSI.
* Exibição de uptime do sistema.
* Controle por botão físico.
* Implementação multitarefa com FreeRTOS.

---

## Funcionamento

Após a energização:

1. O ESP32 inicializa GPIOs, NVS e tarefas FreeRTOS.
2. O LED de aplicação inicia sua rotina de piscada.
3. O Wi-Fi permanece desligado.
4. O botão físico permite alternar entre:

   * Wi-Fi desligado
   * Wi-Fi ligado

Quando o Wi-Fi é habilitado:

1. O ESP32 conecta-se à rede configurada.
2. Obtém endereço IP via DHCP.
3. Inicia um servidor HTTP interno.
4. Disponibiliza uma página de configuração acessível pelo navegador.

Quando o Wi-Fi é desabilitado:

1. O servidor HTTP é encerrado.
2. A interface Wi-Fi é desligada.
3. O dispositivo continua executando normalmente suas funções locais.

---

## Interface Web

A página principal apresenta informações de diagnóstico em tempo real:

### Status do Sistema

* Endereço IP
* Intensidade do sinal (RSSI)
* Período atual de piscada
* Tempo de funcionamento (uptime)

### Controle de Frequência

Permite alterar o período de piscada do LED através de um formulário HTML.

Faixa permitida:

* Mínimo: 10 ms
* Máximo: 60000 ms

O valor configurado é armazenado na NVS para ser restaurado após reinicializações.

### Controle do Wi-Fi

A interface também disponibiliza um botão para desligamento remoto da conectividade.

---

## Arquitetura de Software

### Task de Blink

Responsável por:

* Alternar o estado do LED de aplicação.
* Respeitar o período configurado.
* Registrar eventos via log serial.

### Task de Botão

Responsável por:

* Monitorar o botão físico.
* Detectar transições de estado.
* Habilitar ou desabilitar o Wi-Fi.

### Manipulador de Eventos Wi-Fi

Processa eventos do sistema:

* Inicialização da estação Wi-Fi.
* Conexão e desconexão.
* Obtenção de endereço IP.

### Servidor HTTP

Disponibiliza três endpoints:

| Endpoint      | Função                      |
| ------------- | --------------------------- |
| `/`           | Página principal            |
| `/set_period` | Atualiza período de piscada |
| `/wifi_off`   | Desliga Wi-Fi               |

---

## Persistência de Configuração

O período de piscada é armazenado utilizando a biblioteca NVS (Non-Volatile Storage).

Chave utilizada:

```text
Namespace: config
Key: period
```

Dessa forma, a configuração permanece disponível mesmo após desligamento ou reinicialização do ESP32.

---

## Hardware Utilizado

### Entradas

| Dispositivo | GPIO  |
| ----------- | ----- |
| Botão Wi-Fi | GPIO4 |

### Saídas

| Dispositivo      | GPIO   |
| ---------------- | ------ |
| LED Status Wi-Fi | GPIO2  |
| LED Blink        | GPIO18 |

---

## Conceitos Demonstrados

Este projeto serve como exemplo prático dos seguintes conceitos:

* ESP-IDF
* FreeRTOS
* Servidor HTTP embarcado
* Wi-Fi Station Mode
* Event Loop do ESP-IDF
* Persistência via NVS
* Programação multitarefa
* Configuração remota por navegador
* Estratégias de economia de energia
* Projeto de dispositivos IoT conectados

---

## Possíveis Evoluções

* Configuração de SSID e senha via interface web.
* Modo Access Point para provisionamento.
* Autenticação de usuários.
* HTTPS/TLS.
* Atualização OTA.
* Controle de múltiplos parâmetros.
* Dashboard com atualização em tempo real.
* Integração MQTT.

---

## Licença

Projeto desenvolvido para fins educacionais e experimentação com ESP32 e ESP-IDF.
