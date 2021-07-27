#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

#include "source/sensor_model_client.h"
#include "source/mqtt.h"

static const char *TAG = "Main-Client";

#define SIZE_MAIN_QUEUE 20

/* Wifi */
#define AP_RECONN_ATTEMPTS CONFIG_MAXIMUM_RETRY

static bool connected = false;
static SemaphoreHandle_t xSem_connected = NULL;

static bool get_connected();
static void set_connected(bool con);
static void wifi_init_sta(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int event_id, void* event_data);

void app_main(void)
{
    /* NVS */
    ESP_LOGI(TAG, "Initialising NVS");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Wifi */
    xSem_connected = xSemaphoreCreateMutex();
    wifi_init_sta();
    ESP_LOGI(TAG, "Waiting for wifi connection...");
    while(!get_connected());

    // BLE
    ESP_LOGI(TAG, "Initialising Ble and Bluetooth with sensor model -> client");
    ESP_ERROR_CHECK(ble_mesh_init());

    // MQTT
    ESP_ERROR_CHECK(init_mqtt());
    /*****************************/

    vTaskDelete(NULL);
}

static bool get_connected()
{
    bool con;
    while(xSemaphoreTake( xSem_connected, ( TickType_t ) 10 ) != pdTRUE);
    con = connected;
    xSemaphoreGive(xSem_connected);
    return con;
}

static void set_connected(bool con)
{
    while(xSemaphoreTake( xSem_connected, ( TickType_t ) 10 ) != pdTRUE);
    connected = con;
    xSemaphoreGive(xSem_connected);
}

static void wifi_init_sta(void)
{

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Set our event handling */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int event_id, void* event_data)
{
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < AP_RECONN_ATTEMPTS) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        set_connected(true);
    }
}