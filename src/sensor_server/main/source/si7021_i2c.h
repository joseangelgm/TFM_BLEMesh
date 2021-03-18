#ifndef SI7021_I2C_H
#define SI7021_I2C_H

#include "esp_log.h"

/**
* @brief Initialize circular buffer, configure gpio for i2c
*/
esp_err_t si7021_init();

/**
 *  @brief Task: Get temperature data and insert into circular buffer
 */
void si7021_task_read_temperature(void* params);

/**
 *  @brief get mean temperature data
 */
float get_mean_temperature_data();

#endif