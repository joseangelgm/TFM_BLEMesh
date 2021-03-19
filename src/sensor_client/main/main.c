#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "source/sensor_model_client.h"

static const char *TAG = "Main-Client";

void app_main(void){

    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "Stating...");

    /*********** Init ***********/
    ESP_LOGI(TAG, "Initialising NVS");
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Initialising Ble and Bluetooth with sensor model -> client");
    ESP_ERROR_CHECK(ble_mesh_init());
    /*****************************/

    vTaskDelete(NULL);
}