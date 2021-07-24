#ifndef _TEMPERATURE_SENSOR_H_
#define _TEMPERATURE_SENSOR_H_

esp_err_t si7021_init_temp();

uint8_t get_mean_temperature();

#endif