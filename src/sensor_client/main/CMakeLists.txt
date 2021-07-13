set(srcs "main.c"
        "source/sensor_model_client.c"
        "source/mqtt.c"
        "source/ble_cmd.c"
        "source/tasks_manager.c"
        "source/messages_parser.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  ".")
