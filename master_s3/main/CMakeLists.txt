idf_component_register(
    SRCS
        "main.c"
        "frame_protocol.c"
        "uart_link.c"
        "audio_capture.c"
        "sr_engine.c"
        "wifi_service.c"
        "cloud_service.c"
        "conversation_log.c"
        "wake_led.c"
        "youdao_asr.c"
        "youdao_tts.c"
        "espnow_control.c"
        "alarm_service.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_event esp_netif esp_http_client nvs_flash esp_timer
             lwip mbedtls json esp-sr esp_partition esp_driver_rmt
)
