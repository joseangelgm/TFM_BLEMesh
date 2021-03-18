#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "source/si7021_i2c.h"

static const char *TAG = "TFM-Main";

void app_main(void){

    ESP_LOGI(TAG, "Stating...");
    ESP_ERROR_CHECK(si7021_init());

    ESP_LOGI(TAG, "Creating task -> Read temperature");
    xTaskCreate(&si7021_task_read_temperature, "si7021 task temp read", 1024 * 2, (void *)0, 5, NULL);

    vTaskDelete(NULL);
}