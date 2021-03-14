
/* i2c - Example
   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples
   See README.md file to get detailed usage of this example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"

static const char *TAG = "i2c-example";

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)


#define DELAY_TIME_BETWEEN_ITEMS_MS 1000 /*!< delay time between different test items */


#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL               /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA               /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUMBER(CONFIG_I2C_MASTER_PORT_NUM) /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_MASTER_FREQUENCY        /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */

#define SI7021_SENSOR_ADDR 0x40   /*!< slave address for SI7021 sensor */
#define SI7021_READ_TEMP 0xF3    /*!< READ op with no stretching  F3*/
#define SI7021_READ_TEMP_HOLD 0xE3    /*!< READ op with  stretching  E3 */
#define SENSOR_DELAY 20


#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */
#define I2C_TIMEOUT_MS 1000


/**
 * @brief test code to operate on SI7021 sensor
 *
 * 1. Write READ_TEMP command
 * ___________________________________________________________________
 * | start | slave_addr + wr_bit + ack |  write READ_TEMP+ ack | stop |
 * --------|---------------------------|-----------------------|------|
 * 2. wait more than 100 ms
 * 3. read data
 * ______________________________________________________________________________________
 * | start | slave_addr + rd_bit + ack | read 1 byte + ack  | read 1 byte + nack | stop |
 * --------|---------------------------|--------------------|--------------------|------|
 */
static esp_err_t i2c_master_sensor_test(i2c_port_t i2c_num, uint8_t *data_h, uint8_t *data_l)
{
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
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}



static void i2c_test_task(void *arg)
{
    int ret;
    uint8_t sensor_data_h, sensor_data_l;
    while (1) {
        ret = i2c_master_sensor_test(I2C_MASTER_NUM, &sensor_data_h, &sensor_data_l);
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "I2C Timeout");
        } else if (ret == ESP_OK) {
            uint16_t bytes;
            printf("data_h: %02x\n", sensor_data_h);
            printf("data_l: %02x\n", sensor_data_l);
            bytes = (sensor_data_h << 8 | sensor_data_l);
            printf("temp val: %.02f [ºC]\n", ( (175.72 * bytes) / 65536.0 ) - 46.85);
        } else {
            ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
        }
        vTaskDelay(DELAY_TIME_BETWEEN_ITEMS_MS  / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    printf("STARTING I2C TEST\n");
    ESP_ERROR_CHECK(i2c_master_init());
    printf("Create Task...\n");
    xTaskCreate(i2c_test_task, "i2c_test_task_0", 1024 * 2, (void *)0, 10, NULL);
    vTaskDelete(NULL);
}