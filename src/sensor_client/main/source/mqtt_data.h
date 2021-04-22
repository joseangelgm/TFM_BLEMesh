#ifndef _MQTT_DATA_
#define _MQTT_DATA_

#include <stdio.h>
#include "cJSON.h"

typedef enum {
    A_ERROR,
    A_UNKNOWN,
    A_OK
} action_code_t;

/*
    READY -> RUNNING <-> STOPPED
                      -> DONE
*/
typedef enum {
    READY,
    RUNNING,
    STOPPED,
    DONE
} status_t;

typedef struct action_t {
    status_t status;
    uint32_t opcode;
    char* result;
} action_t;

action_code_t build_action(cJSON* root, action_t *action);

#endif