set(srcs "main.c"
         "source/si7021_i2c.c"
         "source/sensor_model_server.c"
         "source/temperature_sensor.c"
         "source/humidity_sensor.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS  ".")
