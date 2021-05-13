#ifndef _BLE_CMD_H_
#define _BLE_CMD_H_

typedef struct mqtt_json {
    char* json;
    int size;
} mqtt_json;

typedef struct ble_task_t {
    char *name;
    int delay; //seconds
    uint32_t opcode;
} ble_task_t;

/**
 * @brief task to parse a json and launchs ble commands
 */
void task_parse_json(void *params);

#endif