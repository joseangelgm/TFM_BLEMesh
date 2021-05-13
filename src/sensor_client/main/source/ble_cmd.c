#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include "esp_ble_mesh_sensor_model_api.h"

#include "source/ble_cmd.h"

static const char *TAG = "BLE_CMD";

extern void ble_mesh_send_sensor_message(uint32_t opcode);

uint32_t get_opcode(char *opcode)
{
    if(strcmp(opcode, "GET_STATUS") == 0){
        return ESP_BLE_MESH_MODEL_OP_SENSOR_GET;
    }
    else if(strcmp(opcode, "GET_CADENCE") == 0){
        return ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET;
    }
    else if(strcmp(opcode, "GET_DESCRIPTOR") == 0){
        return ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET;
    }
    else if(strcmp(opcode, "GET_SETTINGS") == 0){
        return ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET;
    }
    else if(strcmp(opcode, "GET_SERIES") == 0){
        return ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET;
    }
    return 0;
}

#if 0
static void task_ble_cmd(void *params)
{
    ble_task_t *ble_task = (ble_task_t *) params;
    ESP_LOGW(TAG, "task_ble_cmd2: %d, %d", ble_task->opcode, ble_task->delay);

    //for(;;)
    //{
    //    ble_mesh_send_sensor_message(ble_task->opcode);
    //    vTaskDelay(ble_task->delay * 1000 / portTICK_PERIOD_MS);
    //}
    vTaskDelete(NULL);
}
#endif

static void task_ble_cmd(void *params)
{
    ble_task_t *ble_task = (ble_task_t *) params;
    ESP_LOGW(TAG, "task_ble_cmd2: %d, %d", ble_task->opcode, ble_task->delay);

    for(;;)
    {
        ble_mesh_send_sensor_message(ble_task->opcode);
        vTaskDelay(ble_task->delay * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


// mosquitto_pub -t /sensors/commands -m "{\"auto\":true,\"tasks\":[{\"opcode\":\"GET_STATUS\",\"delay\":7}]}"
/**
 * @brief return the size of ble_task_t created
*/
static ble_task_t* parse_build_task(char *json, int* size)
{

    int size_tasks = 0;
    ble_task_t *ble_tasks = NULL;

    cJSON *root = cJSON_Parse(json);
    const cJSON *automatic = cJSON_GetObjectItem(root, "auto");
    if(automatic != NULL || cJSON_IsBool(automatic)){
        if(cJSON_IsTrue(automatic)){
            const cJSON *tasks = cJSON_GetObjectItem(root, "tasks");
            if(tasks != NULL){
                const cJSON *task = NULL;
                int size = cJSON_GetArraySize(tasks);

                if(size != 0){

                    ble_tasks = (ble_task_t *) malloc(size * sizeof(ble_task_t));
                    memset(ble_tasks, 0, size * sizeof(ble_task_t));

                    cJSON_ArrayForEach(task, tasks){

                        const cJSON *opcode = cJSON_GetObjectItem(task, "opcode");
                        const cJSON *delay  = cJSON_GetObjectItem(task, "delay");

                        if(opcode != NULL && cJSON_IsString(opcode)
                            && delay != NULL && cJSON_IsNumber(delay)){

                            ble_tasks[size_tasks].opcode = get_opcode(opcode->valuestring);
                            ble_tasks[size_tasks].delay  = delay->valueint;
                            size_tasks++;

                        }
                    }
                }
            }
        }
        else{ // No es una tarea automatica

        }
    }
    cJSON_Delete(root);
    if(ble_tasks == NULL){
        ESP_LOGE(TAG, "Null BUILD");
    }
    *size = size_tasks;
    return ble_tasks;
}

#if 0
static int parse_build_task(char *json, ble_task_t **ble_tasks)
{

    // char *topic = malloc(event->topic_len * sizeof(char) + 1);
    // memset(topic, '\0', event->topic_len * sizeof(char) + 1);

    int size_tasks = 0;

    cJSON *root = cJSON_Parse(json);
    const cJSON *automatic = cJSON_GetObjectItem(root, "auto");
    if(automatic != NULL || cJSON_IsBool(automatic)){
        /*
            Check if auto is true or false
        */
        if(cJSON_IsTrue(automatic)){
            const cJSON *tasks = cJSON_GetObjectItem(root, "tasks");
            if(tasks != NULL){
                const cJSON *task = NULL;
                int size = cJSON_GetArraySize(tasks);

                if(size != 0){

                    *ble_tasks = (ble_task_t *) malloc(size * sizeof(ble_task_t));
                    memset(*ble_tasks, 0, size * sizeof(ble_task_t));

                    cJSON_ArrayForEach(task, tasks){

                        const cJSON *opcode = cJSON_GetObjectItem(task, "opcode");
                        const cJSON *delay  = cJSON_GetObjectItem(task, "delay");

                        if(opcode != NULL && cJSON_IsString(opcode)
                            && delay != NULL && cJSON_IsNumber(delay)){

                            (*ble_tasks)[size_tasks].opcode = get_opcode(opcode->valuestring);
                            (*ble_tasks)[size_tasks].delay  = delay->valueint;
                            size_tasks++;

                        }
                    }
                }
            }
        }
        else{ // No es una tarea automatica

        }
    }
    cJSON_Delete(root);
    if(ble_tasks == NULL){
        ESP_LOGE(TAG, "Null BUILD");
    }
    return size_tasks;
}
#endif

void task_parse_json(void *params)
{
    QueueHandle_t queue = (*(QueueHandle_t *) params);
    BaseType_t xStatus;
    mqtt_json json_received;

    char* json_string;
    ble_task_t *ble_tasks = NULL;
    int size_tasks = 0;

    for(;;)
    {
        xStatus = xQueueReceive(queue, &json_received, portMAX_DELAY);
        if(xStatus == pdTRUE)
        {
            json_string = (char *) malloc(json_received.size * sizeof(char) + 1);
            memset(json_string, '\0',  json_received.size * sizeof(char) + 1);
            strncpy(json_string, json_received.json, json_received.size);
            ESP_LOGI(TAG, "Json received %s", json_string);

            // Check if json is correct
            ble_tasks = parse_build_task(json_string, &size_tasks);
            if(size_tasks != 0)
            {
                if(ble_tasks == NULL)
                {
                    ESP_LOGE(TAG, "Null OUT");
                }
                else
                {
                    for(int i = 0; i < size_tasks; i++)
                    {
                        ESP_LOGI(TAG, "Task: opcode real %u, delay %d", ble_tasks[i].opcode, ble_tasks[i].delay);
                        ESP_LOGW(TAG, "Before Task: %lu, %d, %lu", (unsigned long) ble_tasks[i].opcode, ble_tasks[i].delay, (unsigned long) ESP_BLE_MESH_MODEL_OP_SENSOR_GET);
                        ble_task_t aux;
                        memcpy(&aux, &ble_tasks[i], sizeof(ble_task_t));
                        xTaskCreate(&task_ble_cmd, "task_ble_cmd", 2046, (void *) &aux, 4, NULL);
                    }
                }
            }
            free(ble_tasks);
            free(json_string);
        }
        else
        {
            ESP_LOGE(TAG, "Error in queue -> task_receive_json");
        }
        vTaskDelay(400 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}