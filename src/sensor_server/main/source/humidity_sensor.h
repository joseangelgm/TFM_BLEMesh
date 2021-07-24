#ifndef _HUMIDITY_SENSOR_H_
#define _HUMIDITY_SENSOR_H_

esp_err_t si7021_init_hum();

uint8_t get_mean_humidity();

#endif