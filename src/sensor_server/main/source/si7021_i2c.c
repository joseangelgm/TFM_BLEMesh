#include "driver/i2c.h"

#include "si7021_i2c.h"

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define DELAY_TIME_BETWEEN_ITEMS_MS 1000 /*!< delay time between different test items */

#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUMBER(CONFIG_I2C_MASTER_PORT_NUM) /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY        /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */

#define SI7021_SENSOR_ADDR 0x40 /*!< slave address for SGP30 sensor */
#define SI7021_READ_TEMP 0xF3       /*!< READ op with no stretching  F3*/
#define SI7021_READ_TEMP_HOLD 0xE3  /*!< READ op with  stretching  E3 */
#define SENSOR_DELAY 20

#define ACK_CHECK_EN   0x1   /*!< I2C master will check ack from slave */
#define ACK_CHECK_DIS  0x0   /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0   /*!< I2C ack value */
#define NACK_VAL       0x1   /*!< I2C nack value */
#define I2C_TIMEOUT_MS 1000

#define N_SAMPLES CONFIG_NUM_SAMPLES
#define WINDOW_SIZE CONFIG_WINDOW_SIZE

#define SIZE 20

/* Circular buffer */
static int sensor_data[SIZE];
static int last_index;
/*******************/

static SemaphoreHandle_t xSem_circular_buffer = NULL;

static const char *TAG = "si7021";

/**
* @brief Initialize circular buffer, configure gpio for i2c
*/
esp_err_t si7021_init(){

    /* Circular buffer */
    last_index = 0;
    xSem_circular_buffer = xSemaphoreCreateMutex();

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    i2c_param_config(I2C_MASTER_NUM, &conf);

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief get tempereature though i2c from si7021 sensor
*/
static esp_err_t si7021_get_temperature(i2c_port_t i2c_num, uint8_t *data_h, uint8_t *data_l){

    int ret;

    //Send READ TEMP command (WRITE OPT)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SI7021_SENSOR_ADDR << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, SI7021_READ_TEMP, ACK_CHECK_EN);
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
    i2c_master_write_byte(cmd, SI7021_SENSOR_ADDR << 1 | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data_h, ACK_VAL);
    i2c_master_read_byte(cmd, data_l, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, I2C_TIMEOUT_MS / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 *  @brief insert data into the circular buffer
 */
static void insert_data(float value){
    while(xSemaphoreTake(xSem_circular_buffer, ( TickType_t ) 10 ) != pdTRUE);
    sensor_data[last_index] = value;
    last_index = (last_index + 1) % SIZE; // circular
    xSemaphoreGive(xSem_circular_buffer);
}

/**
 * @brief regularize data retreived from the sensor. Check datasheet Si7021.
 */
static float regularize_temperature(uint16_t bytes){
    return (( (175.72 * bytes) / 65536.0 ) - 46.85);
}

/**
 *  @brief Task: Get temperature data and insert into circular buffer
 */
void si7021_task_read_temperature(void* params){

    int ret;
    uint8_t sensor_data_h, sensor_data_l;
    uint16_t bytes = 0;

    while (1) {
        ret = si7021_get_temperature(I2C_MASTER_NUM, &sensor_data_h, &sensor_data_l);
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "I2C Timeout");
        } else if (ret == ESP_OK) {
            ESP_LOGD(TAG, "data_h: %02x, data_l: %02x", sensor_data_h, sensor_data_l);

            bytes = (sensor_data_h << 8 | sensor_data_l);
            float temp = regularize_temperature(bytes);
            ESP_LOGD(TAG, "temp val: %.02f [ºC]", temp);

            insert_data(temp);
        } else {
            ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
        }
        vTaskDelay(DELAY_TIME_BETWEEN_ITEMS_MS  / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

/**
 *  @brief get mean temperature data
 */
float get_mean_temperature_data(){
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
    return result / samples;
}