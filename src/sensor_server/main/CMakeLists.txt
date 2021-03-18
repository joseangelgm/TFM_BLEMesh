set(srcs "main.c"
         "source/si7021_i2c.c"
         "source/sensor_model_server.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS  ".")
