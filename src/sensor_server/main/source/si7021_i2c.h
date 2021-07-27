#ifndef SI7021_I2C_H
#define SI7021_I2C_H

#define DELAY_TIME_BETWEEN_ITEMS_MS 1

/**
* @brief Initialize circular buffer, configure gpio for i2c
*/
esp_err_t si7021_init();

/**
 *  @brief get mean temperature data
 */
uint8_t si7021_get_mean_temperature_data();

uint8_t si7021_get_mean_humidity_data();

#endif