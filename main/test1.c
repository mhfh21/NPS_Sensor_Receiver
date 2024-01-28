#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mirf.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "errno.h"
#include "esp_timer.h"

#define WIFI_SSID "mqtt1"
#define WIFI_PASSWORD "bo21122112"
#define SERVER_IP "85.198.15.205"
#define SERVER_PORT 3333
#define TAG "test1"
#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;

void send_data_to_server(uint8_t *data);

static void event_handler(void* arg, esp_event_base_t event_base, 
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGI(TAG, "Disconnect reason : %d", event->reason);
        ESP_LOGI(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: %s", ip4addr_ntoa(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void AdvancedSettings(NRF24_t * dev)
{
    ESP_LOGW(pcTaskGetName(0), "Set RF Data Ratio to 1MBps");
    Nrf24_SetSpeedDataRates(dev, 0);
}

// Add this function to get the current time in milliseconds
int64_t get_time_in_ms() {
    return esp_timer_get_time() / 1000;
}

void receiver(void *pvParameters)
{
    ESP_LOGI(pcTaskGetName(0), "Start");
    NRF24_t dev;
    Nrf24_init(&dev);
    uint8_t payload = 32;
    uint8_t channel = 90;
    Nrf24_config(&dev, channel, payload);

    esp_err_t ret = Nrf24_setRADDR(&dev, (uint8_t *)"FGHIJ");
    if (ret != ESP_OK) {
        ESP_LOGE(pcTaskGetName(0), "nrf24l01 not installed");
        while(1) { vTaskDelay(1); }
    }

    AdvancedSettings(&dev);

    Nrf24_printDetails(&dev);
    ESP_LOGI(pcTaskGetName(0), "Listening...");

    uint8_t buf[32];

    while(1) {
        if (Nrf24_dataReady(&dev) == false) break;
        Nrf24_getData(&dev, buf);
    }

    while(1) {
        if (Nrf24_dataReady(&dev)) {
            Nrf24_getData(&dev, buf);
            ESP_LOGI(pcTaskGetName(0), "Got data:%s", buf);
            send_data_to_server(buf);
        }
        vTaskDelay(1);
    }
}

void wifi_init() {
    ESP_LOGI(TAG, "Initializing TCP/IP adapter...");
    esp_netif_init();
    ESP_LOGI(TAG, "Creating default event loop...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Initializing WiFi...");

    // Wi-Fi Configuration Phase
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    // Event Handlers Registration
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Wi-Fi Start Phase
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Wi-Fi Connect Phase
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi started, connecting...");
}

void send_data_to_server(uint8_t *data) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        close(sock);
        return;
    }

    int64_t time_in_ms = get_time_in_ms();
    char* data_with_time = (char*) malloc(strlen((char*)data) + 20); // 20 for time and null terminator
    sprintf(data_with_time, "%lld %s", time_in_ms, data);

    err = send(sock, data_with_time, strlen(data_with_time), 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    }

    close(sock);
    free(data_with_time);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_event_group = xEventGroupCreate();
    wifi_init();

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    xTaskCreate(&receiver, "RECEIVER", 1024*3, NULL, 2, NULL);
}