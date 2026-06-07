idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_http_server
        nvs_flash
        esp_driver_gpio
        esp_timer
)