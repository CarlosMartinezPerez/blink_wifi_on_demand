// blink definido via HTTP, com wifi ativado apenas se acionado botão

// ======================================================
// Bibliotecas padrão
// ======================================================

#include <stdio.h>
#include <stdbool.h>
#include "esp_timer.h"

// ======================================================
// FreeRTOS
// ======================================================

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ======================================================
// GPIO
// ======================================================

#include "driver/gpio.h"

// ======================================================
// WiFi
// ======================================================

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

// ======================================================
// NVS
// ======================================================

#include "nvs_flash.h"

// ======================================================
// HTTP Server
// ======================================================

#include "esp_http_server.h"

// ======================================================
// GPIOs
// ======================================================

#define LED_STATUS_GPIO    GPIO_NUM_2
#define LED_BLINK_GPIO     GPIO_NUM_18

#define BUTTON_WIFI_GPIO   GPIO_NUM_4

// ======================================================
// WiFi
// ======================================================

#define WIFI_SSID      "NOME DA REDE WIFI"
#define WIFI_PASSWORD  "SENHA DA REDE WIFI"

// ======================================================
// Variáveis globais
// ======================================================

static const char *TAG = "WIFI_ON_DEMAND";

static bool wifi_enabled = false;

static httpd_handle_t server = NULL;

static uint32_t blink_period_ms = 500;

// ======================================================
// Protótipos
// ======================================================

static void wifi_start(void);

static void wifi_stop(void);

static void start_webserver(void);

static void stop_webserver(void);

static void blink_task(void *pvParameters);

static void button_task(void *pvParameters);

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data);

static void get_ip_string(
    char *buffer,
    size_t len);

static int8_t get_wifi_rssi(void);

static void save_blink_period(void);

static void load_blink_period(void);

static bool wifi_is_enabled(void);

// ======================================================
// Inicialização dos GPIOs
// ======================================================

static void gpio_init_all(void)
{
    // ------------------------------------------
    // LEDS
    // ------------------------------------------
        
    gpio_reset_pin(LED_STATUS_GPIO);

    gpio_set_direction(
        LED_STATUS_GPIO,
        GPIO_MODE_OUTPUT);

    gpio_set_level(
        LED_STATUS_GPIO,
        0);

    gpio_reset_pin(LED_BLINK_GPIO);

    gpio_set_direction(
        LED_BLINK_GPIO,
        GPIO_MODE_OUTPUT);

    gpio_set_level(
        LED_BLINK_GPIO,
        0);

    // ------------------------------------------
    // Botão
    // ------------------------------------------

    gpio_reset_pin(BUTTON_WIFI_GPIO);

    gpio_set_direction(
        BUTTON_WIFI_GPIO,
        GPIO_MODE_INPUT);

    gpio_set_pull_mode(
        BUTTON_WIFI_GPIO,
        GPIO_PULLUP_ONLY);
}

// ======================================================
// Task de piscada
// ======================================================

static void blink_task(void *pvParameters)
{
    bool state = false;

    while (1)
    {
        state = !state;

        gpio_set_level(
            LED_BLINK_GPIO,
            state);

        ESP_LOGI(
            TAG,
            "GPIO18 = %d | Periodo = %lu ms",
            state,
            (unsigned long)blink_period_ms);

        vTaskDelay(
            pdMS_TO_TICKS(
                blink_period_ms));
    }
}

// ======================================================
// Task do botão
// ======================================================

static void button_task(void *pvParameters)
{
    bool last_state = true;

    while (1)
    {
        bool current_state =
            gpio_get_level(
                BUTTON_WIFI_GPIO);

        // Transição HIGH -> LOW
        if (!current_state &&
            last_state)
        {
            wifi_enabled =
                !wifi_enabled;
            
            if (wifi_enabled)
            {
                wifi_start();
            }
            else
            {
                //stop_webserver();

                wifi_stop();
            }

            gpio_set_level(
                LED_STATUS_GPIO,
                wifi_enabled);

            ESP_LOGI(
                TAG,
                "wifi_enabled = %s",
                wifi_enabled ?
                "TRUE" :
                "FALSE");

            vTaskDelay(
                pdMS_TO_TICKS(300));
        }

        last_state =
            current_state;

        vTaskDelay(
            pdMS_TO_TICKS(20));
    }
}

// ======================================================
// Obtém IP atual
// ======================================================

static void get_ip_string(
    char *buffer,
    size_t len)
{
    esp_netif_ip_info_t ip_info;

    esp_netif_t *netif =
        esp_netif_get_handle_from_ifkey(
            "WIFI_STA_DEF");

    if (netif == NULL)
    {
        snprintf(
            buffer,
            len,
            "Sem interface");

        return;
    }

    if (esp_netif_get_ip_info(
            netif,
            &ip_info) != ESP_OK)
    {
        snprintf(
            buffer,
            len,
            "Sem IP");

        return;
    }

    snprintf(
        buffer,
        len,
        IPSTR,
        IP2STR(&ip_info.ip));
}

// ======================================================
// Obtém RSSI
// ======================================================

static int8_t get_wifi_rssi(void)
{
    wifi_ap_record_t ap_info;

    if (!wifi_is_enabled())
    {
        return 0;
    }

    if (esp_wifi_sta_get_ap_info(
            &ap_info) == ESP_OK)
    {
        return ap_info.rssi;
    }

    return 0;
}

// ======================================================
// Página principal
// ======================================================

static esp_err_t root_handler(
    httpd_req_t *req)
{
    char html[2048];

    char ip_str[16] = "Nao conectado";

    int8_t wifi_rssi = 0;

    uint64_t uptime_seconds =
        esp_timer_get_time() / 1000000ULL;

    get_ip_string(
        ip_str,
        sizeof(ip_str));

    wifi_rssi =
        get_wifi_rssi();

    snprintf(
        html,
        sizeof(html),

        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'>"
        "<meta http-equiv='refresh' content='10'>"
        "<title>WiFi On Demand</title>"
        "</head>"

        "<body>"

        "<h1>Controle Blink ESP32</h1>"

        "<hr>"

        "<h2>Status do Sistema</h2>"

        "<p><b>IP:</b> %s</p>"

        "<p><b>RSSI:</b> %d dBm</p>"

        "<p><b>Periodo:</b> %lu ms</p>"

        "<p><b>Uptime:</b> %lu s</p>"

        "<hr>"

        "<h2>Controle de Frequencia</h2>"

        "<form action='/set_period'>"

        "<input "
        "type='number' "
        "name='value' "
        "min='10' "
        "max='60000' "
        "value='%lu'>"

        "<button type='submit'>Aplicar</button>"

        "</form>"

        "<hr>"

        "<h2>WiFi</h2>"

        "<form action='/wifi_off'>"
            "<button type='submit'>"
                "Desligar WiFi"
            "</button>"
        "</form>"

        "</body>"
        "</html>",

        ip_str,
        wifi_rssi,
        (unsigned long)blink_period_ms,
        (unsigned long)uptime_seconds,
        (unsigned long)blink_period_ms);

    httpd_resp_send(
        req,
        html,
        HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// ======================================================
// Handler de período
// ======================================================

static esp_err_t set_period_handler(
    httpd_req_t *req)
{
    char query[64];

    if (httpd_req_get_url_query_str(
            req,
            query,
            sizeof(query)) == ESP_OK)
    {
        char value_str[16];

        if (httpd_query_key_value(
                query,
                "value",
                value_str,
                sizeof(value_str)) == ESP_OK)
        {
            uint32_t value =
                (uint32_t)atoi(value_str);

            if (value >= 10 &&
                value <= 60000)
            {
                blink_period_ms = value;

                save_blink_period();

                ESP_LOGI(
                    TAG,
                    "Periodo alterado para %lu ms",
                    (unsigned long)blink_period_ms);
            }
        }
    }

    return root_handler(req);
}

// ======================================================
// Handler desliga wifi
// ======================================================

static esp_err_t wifi_off_handler(
    httpd_req_t *req)
{
    gpio_set_level(
        LED_STATUS_GPIO,
        0);

    wifi_enabled = false;

    httpd_resp_sendstr(
        req,
        "WiFi desligado. Feche esta pagina.");

    esp_wifi_stop();

    return ESP_OK;
}

// ======================================================
// Eventos WiFi
// ======================================================

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(
            TAG,
            "Conectando ao WiFi...");

        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(
            TAG,
            "Wifi desconectado");

        stop_webserver();
    }

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(
            TAG,
            "IP obtido: " IPSTR,
            IP2STR(&event->ip_info.ip));

        start_webserver();
    }
}

// ======================================================
// Liga WiFi
// ======================================================

static void wifi_start(void)
{
    static bool initialized = false;

    if (!initialized)
    {
        ESP_ERROR_CHECK(
            esp_netif_init());

        ESP_ERROR_CHECK(
            esp_event_loop_create_default());

        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg =
            WIFI_INIT_CONFIG_DEFAULT();

        ESP_ERROR_CHECK(
            esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(
            esp_event_handler_register(
                WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &wifi_event_handler,
                NULL));

        ESP_ERROR_CHECK(
            esp_event_handler_register(
                IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &wifi_event_handler,
                NULL));

        initialized = true;
    }

    wifi_config_t wifi_config =
    {
        .sta =
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        }
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(
            WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config));

    ESP_ERROR_CHECK(
        esp_wifi_start());

    ESP_LOGI(
        TAG,
        "WiFi iniciado");
}

// ======================================================
// Desliga WiFi
// ======================================================

static void wifi_stop(void)
{
    ESP_LOGI(
        TAG,
        "Parando WiFi");

    esp_wifi_stop();
}

// ======================================================
// Verifica estado do wifi
// ======================================================

static bool wifi_is_enabled(void)
{
    wifi_mode_t mode;

    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        return false;
    }

    return true;
}

// ======================================================
// Salva período na NVS
// ======================================================

static void save_blink_period(void)
{
    nvs_handle_t nvs_handle;

    if (nvs_open(
            "config",
            NVS_READWRITE,
            &nvs_handle) == ESP_OK)
    {
        nvs_set_u32(
            nvs_handle,
            "period",
            blink_period_ms);

        nvs_commit(
            nvs_handle);

        nvs_close(
            nvs_handle);

        ESP_LOGI(
            TAG,
            "Periodo salvo: %lu ms",
            (unsigned long)blink_period_ms);
    }
}

// ======================================================
// Carrega período da NVS
// ======================================================

static void load_blink_period(void)
{
    nvs_handle_t nvs_handle;

    uint32_t value = 5000;

    if (nvs_open(
            "config",
            NVS_READONLY,
            &nvs_handle) == ESP_OK)
    {
        if (nvs_get_u32(
                nvs_handle,
                "period",
                &value) == ESP_OK)
        {
            blink_period_ms = value;

            ESP_LOGI(
                TAG,
                "Periodo carregado: %lu ms",
                (unsigned long)blink_period_ms);
        }

        nvs_close(
            nvs_handle);
    }
}

// ======================================================
// Inicia servidor HTTP
// ======================================================

static void start_webserver(void)
{
    if (server != NULL)
    {
        ESP_LOGI(
            TAG,
            "Servidor já está rodando");

        return;
    }

    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    if (httpd_start(
            &server,
            &config) == ESP_OK)
    {
        httpd_uri_t root_uri =
        {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };

        httpd_uri_t set_period_uri =
        {
            .uri = "/set_period",
            .method = HTTP_GET,
            .handler = set_period_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(
            server,
            &root_uri);

        httpd_register_uri_handler(
            server,
            &set_period_uri);

        httpd_uri_t wifi_off_uri =
        {
            .uri = "/wifi_off",
            .method = HTTP_GET,
            .handler = wifi_off_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(
            server,
            &wifi_off_uri);

        ESP_LOGI(
            TAG,
            "Servidor HTTP iniciado");
    }
}

// ======================================================
// Para servidor HTTP
// ======================================================

static void stop_webserver(void)
{
    if (server != NULL)
    {
        httpd_stop(server);

        server = NULL;

        ESP_LOGI(
            TAG,
            "Servidor HTTP parado");
    }
}

// ======================================================
// app_main
// ======================================================

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "Inicializando sistema");

    ESP_ERROR_CHECK(
        nvs_flash_init());

    gpio_init_all();

    // ------------------------------------------
    // Frequência hardcoded do blink
    // ------------------------------------------

    blink_period_ms = 5000;

    load_blink_period();

    ESP_LOGI(
        TAG,
        "Periodo configurado = %lu ms",
        (unsigned long)blink_period_ms);

    // ------------------------------------------
    // Cria task de blink
    // ------------------------------------------

    xTaskCreate(
        blink_task,
        "blink_task",
        4096,
        NULL,
        5,
        NULL);
    
    // ------------------------------------------
    // Cria task de botão
    // ------------------------------------------
    
    xTaskCreate(
        button_task,
        "button_task",
        4096,
        NULL,
        5,
        NULL);

    while (1)
    {
        vTaskDelay(
            pdMS_TO_TICKS(1000));
    }
}

