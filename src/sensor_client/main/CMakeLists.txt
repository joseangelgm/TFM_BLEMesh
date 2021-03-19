set(srcs "main.c"
        "source/board.c"
        "source/sensor_model_client.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  ".")
