#include "driver/i2c.h"

#include "si7021_i2c.h"
#include "si7021_utils.h"
#include "source/temperature_sensor.h"
#include "source/humidity_sensor.h"

#define I2C_MASTER_SCL_IO         CONFIG_I2C_MASTER_SCL       /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO         CONFIG_I2C_MASTER_SDA       /*!< gpio number for I2C master data  */
#define I2C_MASTER_FREQ_HZ        CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */

static esp_err_t initialize_i2c()
{
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
* @brief Initialize i2c
*/
esp_err_t si7021_init()
{

    ESP_ERROR_CHECK(initialize_i2c());

    si7021_init_temp(); // initialize temperature measure
    si7021_init_hum();  // initialize humidity measure

    return ESP_OK;
}

uint8_t si7021_get_mean_temperature_data()
{
    return get_mean_temperature();
}

uint8_t si7021_get_mean_humidity_data()
{
    return get_mean_humidity();
}