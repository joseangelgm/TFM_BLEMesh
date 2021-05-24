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
#include "source/tasks_manager.h"

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

static void task_ble_cmd(void *params)
{
    ble_task_t *ble_task = (ble_task_t *) params;
    ESP_LOGW(TAG, "[%s] %d, %d, %d. Running.", ble_task->name, (int)ble_task->auto_task, ble_task->opcode, ble_task->delay);

    for(;;)
    {
        ble_mesh_send_sensor_message(ble_task->opcode);
        vTaskDelay(ble_task->delay * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

char* sanitize_string(char* string)
{
    unsigned long size = strlen(string);
    char *new_string = malloc(size * sizeof(char) + 1);
    memset(new_string, '\0', size * sizeof(char) + 1);
    strncpy(new_string, string, size);
    return new_string;
}


// mosquitto_pub -t /sensors/commands -m "{\"auto\":true,\"tasks\":[{\"opcode\":\"GET_STATUS\",\"delay\":2,\"name\":\"new_task\"}]}"
#if 0
/**
 * @brief
 *  - json = Json string-like to be procesed
 *  - size = size of the struct
 *  Return action_t to be proceses.
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
                        const cJSON *name   = cJSON_GetObjectItem(task, "name");

                        if(opcode != NULL && cJSON_IsString(opcode)
                            && delay != NULL && cJSON_IsNumber(delay)
                            && name != NULL && cJSON_IsString(name)){

                            ble_tasks[size_tasks].opcode = get_opcode(opcode->valuestring);
                            ble_tasks[size_tasks].delay  = delay->valueint;
                            ble_tasks[size_tasks].name = sanitize_string(name->valuestring);
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
    *size = size_tasks;
    return ble_tasks;
}
#endif

static action_t* parse_build_task(char *json, int* size)
{
    action_t *acts = NULL;
    int size_actions = 0;

    cJSON *root = cJSON_Parse(json);
    const cJSON *actions = cJSON_GetObjectItem(root, "actions");
    if(actions != NULL)
    {
        const cJSON *action = NULL;
        size_actions = cJSON_GetArraySize(actions);

        if(size_actions != 0)
        {
            int index = 0;
            acts = (action_t *) malloc(size_actions * sizeof(action_t));
            memset(acts, 0, size_actions * sizeof(action_t));

            cJSON_ArrayForEach(action, actions)
            {
                const cJSON *elem = NULL;

                elem = cJSON_GetObjectItem(action, "task_name");
                if(elem != NULL) // tarea a borrar
                {
                    acts[index].opmode = REMOVE;
                    acts[index].task.name = elem->valuestring;
                }
                else
                {
                    elem = cJSON_GetObjectItem(action, "task");
                    if(elem != NULL) // creamos tarea
                    {
                        const cJSON *auto_task = cJSON_GetObjectItem(elem, "auto");
                        const cJSON *opcode    = cJSON_GetObjectItem(elem, "opcode");
                        const cJSON *delay     = cJSON_GetObjectItem(elem, "delay");
                        const cJSON *name      = cJSON_GetObjectItem(elem, "name");

                        if(auto_task != NULL && cJSON_IsBool(auto_task)
                            && opcode != NULL && cJSON_IsString(opcode)
                            && delay != NULL && cJSON_IsNumber(delay)
                            && name != NULL && cJSON_IsString(name))
                        {
                            if(cJSON_IsTrue(auto_task))
                                acts[index].task.auto_task = true;
                            else
                                acts[index].task.auto_task = true;

                            acts[index].task.name   = sanitize_string(name->valuestring);
                            acts[index].task.delay  = delay->valueint;
                            acts[index].task.opcode = get_opcode(opcode->valuestring);
                            acts[index].opmode = CREATE;
                        }
                        else
                        {
                            // Error leyendo la tarea <task_name>
                            ESP_LOGE(TAG, "Error processing a task!");
                        }
                    }
                }
            }
        }
        else{
            // Error obteniendo el numero de tareas
            ESP_LOGE(TAG, "Error getting number of tasks!");
        }
    }
    else
    {
        // Devolver error de que no esta bien formado el json
        ESP_LOGE(TAG, "Json bad formed!");
    }
    *size = size_actions;
    return acts;
}


void task_parse_json(void *params)
{
    QueueHandle_t queue = (*(QueueHandle_t *) params);
    BaseType_t xStatus;
    mqtt_json json_received;

    char* json_string;
    action_t *actions = NULL;
    int size_actions;

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
            size_actions = 0;
            actions = parse_build_task(json_string, &size_actions);
            ESP_LOGW(TAG, "SIZE: %d", size_actions);
            if(size_actions != 0)
            {
                if(actions != NULL)
                {
                    for(int i = 0; i < size_actions; i++)
                    {
                        ESP_LOGI(TAG, "Action:\n\tname %s,\n\tauto %d,\n\topcode %u,\n\tdelay %d",
                                 actions[i].task.name, (int)actions[i].task.auto_task, actions[i].task.opcode, actions[i].task.delay);
                        if(actions[i].opmode == REMOVE) // remove task
                        {
                            ESP_LOGW(TAG, "Removing task %s", actions[i].task.name);

                            task_t *task = obtain_task(actions[i].task.name);
                            if(task != NULL)
                            {
                                vTaskDelete(task->task_handler);
                                remove_task(actions[i].task.name);
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Task %s doesn't exists. Remove failed", actions[i].task.name);
                            }
                        }
                        else if(actions[i].opmode == CREATE)
                        {
                            // Check if the tasks exists
                            TaskHandle_t TaskHandle;
                            task_t *new_task = (task_t *)malloc(sizeof(task_t));
                            new_task->name = actions[i].task.name;
                            new_task->task_handler = TaskHandle;

                            status_t status = add_new_task_if_not_exists(new_task);
                            if(status == CREATED){
                                ble_task_t aux;
                                memcpy(&aux, &actions[i].task, sizeof(ble_task_t));
                                xTaskCreate(&task_ble_cmd, aux.name, 2046, (void *) &aux, 4, &TaskHandle);
                            }
                            else {
                                // Send for mqtt message -> The task "name" exists!
                                ESP_LOGE(TAG, "Task - %s - exists!", actions[i].task.name);
                                free(new_task);
                            }
                        }
                        else
                        {
                            ESP_LOGE(TAG, "opmode couldn't be recognize!");
                        }
                    }
                }
            }
            else{
                ESP_LOGE(TAG, "Json could be processed or size task equals zero!");
            }
            free(actions);
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