#include "esp_log.h"
#include "driver/i2c.h"

#include "source/si7021_utils.h"
#include "source/humidity_sensor.h"

#define OP_READ_HUM 0xF5 /*!< READ op for humidity*/

static const char *TAG = "si7021_hum";

/* Circular buffer */
static int sensor_data[SIZE] = {0};
static int last_index;
static SemaphoreHandle_t xSem_circular_buffer = NULL;
/*******************/

static esp_err_t get_humidity(i2c_port_t i2c_num, uint8_t *data_h, uint8_t *data_l)
{

    esp_err_t ret;

    //Send READ command (WRITE OPT)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SLAVE_ADDR << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, OP_READ_HUM, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, I2C_TIMEOUT_MS / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(SENSOR_DELAY / portTICK_RATE_MS);

    // get response (READ COMMAND)
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SLAVE_ADDR << 1 | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data_h, ACK_VAL);
    i2c_master_read_byte(cmd, data_l, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, I2C_TIMEOUT_MS / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

static void insert_data(float value)
{
    while(xSemaphoreTake(xSem_circular_buffer, ( TickType_t ) 10 ) != pdTRUE);
    sensor_data[last_index] = value;
    last_index = (last_index + 1) % SIZE; // circular
    xSemaphoreGive(xSem_circular_buffer);
}

static float regularize_humidity(uint16_t bytes)
{
    return (( (125.0 * bytes) / 65536.0 ) - 6);
}

static void task_read_humidity(void* params)
{

    int ret;
    uint8_t sensor_data_h, sensor_data_l;
    uint16_t bytes = 0;

    for(;;)
    {
        ret = get_humidity(I2C_MASTER_NUM, &sensor_data_h, &sensor_data_l);
        if (ret == ESP_ERR_TIMEOUT)
        {
            ESP_LOGE(TAG, "I2C Timeout");
        }
        else if (ret == ESP_OK)
        {
            ESP_LOGD(TAG, "data_h: %02x, data_l: %02x", sensor_data_h, sensor_data_l);

            bytes = (sensor_data_h << 8 | sensor_data_l);
            float hum = regularize_humidity(bytes);
            ESP_LOGI(TAG, "Sensor hum: %.02f %%", hum);

            insert_data(hum);
        }
        else
        {
            ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
        }
        vTaskDelay(DELAY_TIME_ITEMS * 1000 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

uint8_t get_mean_humidity()
{
    while(xSemaphoreTake(xSem_circular_buffer, ( TickType_t ) 10 ) != pdTRUE);
    int index = (last_index + SIZE - 1) % SIZE;
    int samples = 0;
    int result = 0;
    while(samples < WINDOW_SIZE){
        result += sensor_data[index];
        index = (index + SIZE - 1) % SIZE;
        samples++;
    }
    xSemaphoreGive(xSem_circular_buffer);
    return (uint8_t)(result / samples);
}

esp_err_t si7021_init_hum(){

    /* Circular buffer */
    last_index = 0;
    xSem_circular_buffer = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "Creating task -> Read humidity");
    xTaskCreate(&task_read_humidity, "si7021_task_read_hum", 1024 * 2, (void *)0, 10, NULL);

    return ESP_OK;
}