#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

//Include .h
#include "source/si7021_i2c.h"
#include "source/sensor_model_server.h"

static const char *TAG = "Main-Server";

void app_main(void){

    /* NVS */
    ESP_LOGI(TAG, "Initialising NVS");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);


    ESP_LOGI(TAG, "Initialising sensor si7021");
    ESP_ERROR_CHECK(si7021_init());

    ESP_LOGI(TAG, "Initialising Ble and Bluetooth with sensor model -> server");
    ESP_ERROR_CHECK(ble_mesh_init());

    vTaskDelete(NULL);
}