/*
if( data->op_code == 1 ) { // Text frame only.
    cJSON *root = cJSON_Parse((char*)data->data_ptr);
    char *type = cJSON_GetObjectItem(root,"type")->valuestring;
    ESP_LOGI(TAG, "type=%s",type)

    int field = 0;
    if(strcmp(type, "state") == 0) {
        field = cJSON_GetObjectItem(root,"value")->valueint;
        ESP_LOGI(TAG, "state = %d",field);
    }
    else if(strcmp(type, "users") == 0){
        field = cJSON_GetObjectItem(root,"count")->valueint;
        ESP_LOGI(TAG, "Users = %d", field);
    }
}
*/

#include "esp_log.h"
#include "cJSON.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "source/mqtt_data.h"

static const char *TAG = "MQTT_DATA";

/**
 * @brief Translate action to opcode (BLE Mesh)
 */
static action_code_t get_opcode(int action_code, uint32_t *opcode){
    action_code_t err = A_OK;
    switch(action_code){
        case 0:
            *opcode = ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET;
            break;
        case 1:
            *opcode = ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET;
            break;
        case 2:
            *opcode = ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET;
            break;
        case 3:
            *opcode = ESP_BLE_MESH_MODEL_OP_SENSOR_GET;
            break;
        case 4:
            *opcode = ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET;
            break;
        default:
            err = A_UNKNOWN;
    }
    return err;
}


/**
 * @brief Parse json to build action_t
 */
// mosquitto_pub -t /sensors/commands -m {\"action\":4}
action_code_t build_action(cJSON* root, action_t *action){

    action_code_t err = A_OK;

    cJSON *action_value = cJSON_GetObjectItem(root, "action");
    if(!cJSON_IsNumber(action_value)){
        err = A_ERROR;
        return err;
    }

    int action_code = action_value->valueint;
    ESP_LOGW(TAG, "Action: %d", action_code);

    uint32_t opcode;
    err = get_opcode(action_code, &opcode);
    if(err != A_OK){
        return err;
    }

    ESP_LOGW(TAG, "opcode: %u", opcode);
    action->status = READY;
    action->opcode = opcode;

    return err;
}