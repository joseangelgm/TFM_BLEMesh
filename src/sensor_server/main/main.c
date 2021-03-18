#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"


//#include "esp_ble_mesh_defs.h"
//#include "esp_ble_mesh_common_api.h"
//#include "esp_ble_mesh_networking_api.h"
//#include "esp_ble_mesh_provisioning_api.h"
//#include "esp_ble_mesh_config_model_api.h"
//#include "esp_ble_mesh_sensor_model_api.h"
#include "ble_mesh_example_init.h"

//Include .h
#include "source/si7021_i2c.h"
#include "source/sensor_model_server.h"

static const char *TAG = "TFM-Main";

esp_err_t init_bluetooth(void);

void app_main(void){

    ESP_LOGI(TAG, "Stating...");

    /*********** Init ***********/
    ESP_LOGI(TAG, "Initialising NVS");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Initialising sensor si7021");
    ESP_ERROR_CHECK(si7021_init());

    ESP_LOGI(TAG, "Initialising bluetooth");
    ESP_ERROR_CHECK(init_bluetooth());

    ESP_LOGI(TAG, "Initialising Ble with sensor model -> server");
    ESP_ERROR_CHECK(ble_mesh_init());
    /****************************/

    /*********** Tasks ***********/
    ESP_LOGI(TAG, "Creating task -> Read temperature");
    xTaskCreate(&si7021_task_read_temperature, "si7021 task temp read", 1024 * 2, (void *)0, 5, NULL);
    /*****************************/

    vTaskDelete(NULL);
}

esp_err_t init_bluetooth(void){

    esp_err_t err = ESP_OK;
    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
    }
    return err;
}