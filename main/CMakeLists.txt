idf_component_register(SRCS "test1.c"
                       INCLUDE_DIRS "." "../components/mpu6050"
                       INCLUDE_DIRS "." "../components/mirf"
                       REQUIRES mpu6050
                       REQUIRES mirf
                       REQUIRES nvs_flash
                       REQUIRES esp_netif
                       REQUIRES esp_wifi
                       REQUIRES esp_event
                       REQUIRES esp_timer)
