#ifndef _BLE_CMD_H_
#define _BLE_CMD_H_

typedef struct mqtt_json {
    char* json;
    int size;
} mqtt_json;

typedef enum {
    CREATE,
    REMOVE
} opmode_t;

typedef struct ble_task_t {
    char *name; // name of the task.
    bool auto_task; // whether it is a auto task or just one execution
    int delay; // seconds
    uint32_t opcode; // BLE opcode message
} ble_task_t;

typedef struct action_t {
    opmode_t opmode;
    ble_task_t task; // task to be created
} action_t;

/**
 * @brief task to parse a json and launchs ble commands
 */
void task_parse_json(void *params);

#endif