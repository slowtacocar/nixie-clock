#include "esp_wifi.h"
#include "esp_sntp.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_wpa2.h"

#define WIFI_CONNECTED_BIT BIT0

static const int pins[] = {9, 8, 0, 0, 3, 38, 33, 1, 6, 5, 7, 0, 17, 14, 12, 18, 37, 35, 36, 0, 11, 43, 44, 10};

static EventGroupHandle_t s_wifi_event_group;

static void handle_wifi_start(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    printf("Connecting...\n");
    esp_wifi_connect();
}

static void handle_got_ip(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    printf("Connected\n");
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

static void handle_wifi_disconnect(void *esp_netif, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    printf("Disconnected--reconnecting...\n");
    esp_wifi_connect();
}

void app_main(void) {
    for (int i = 0; i < 24; i++) {
        if (pins[i]) {
            gpio_reset_pin(pins[i]);
            gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        }
    }

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            "eduroam",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_identity((uint8_t *) "***", 16));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_username((uint8_t *) "***", 16));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_password((uint8_t *) "***", 16));
    ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, &handle_wifi_start, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handle_got_ip, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handle_wifi_disconnect, NULL, NULL));
    printf("Starting wifi\n");
    ESP_ERROR_CHECK(esp_wifi_start());

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    printf("Starting sntp\n");
    sntp_setservername(0, "pool.ntp.org");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();

    setenv("TZ", "EDT+4", 1);
    tzset();

    while (1) {
        time_t epoch;
        time(&epoch);
        struct tm *timeinfo = localtime(&epoch);

        int digits[] = {
            timeinfo->tm_hour / 10,
            timeinfo->tm_hour % 10,
            timeinfo->tm_min / 10,
            timeinfo->tm_min % 10,
            timeinfo->tm_sec / 10,
            timeinfo->tm_sec % 10,
        };
        for (int i = 0; i < 6; i++) {
            if (pins[i * 4]) gpio_set_level(pins[i * 4], digits[i] & 0b0001);
            if (pins[i * 4 + 1]) gpio_set_level(pins[i * 4 + 1], digits[i] & 0b0010);
            if (pins[i * 4 + 2]) gpio_set_level(pins[i * 4 + 2], digits[i] & 0b0100);
            if (pins[i * 4 + 3]) gpio_set_level(pins[i * 4 + 3], digits[i] & 0b1000);
        }
    }
}
