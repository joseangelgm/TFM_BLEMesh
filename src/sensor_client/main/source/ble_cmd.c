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
#include "source/messages_parser.h"

static const char *TAG = "BLE_CMD";

extern void ble_mesh_send_sensor_message(uint32_t opcode, uint16_t addr);

static uint32_t get_opcode(char *opcode)
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
    ESP_LOGW(TAG, "[%s] %d, %d, %d, %X. Running.", ble_task->name, (int)ble_task->auto_task, ble_task->opcode, ble_task->delay, ble_task->addr);

    for(;;)
    {
        ble_mesh_send_sensor_message(ble_task->opcode, ble_task->addr);
        vTaskDelay(ble_task->delay * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static char* sanitize_string(char* string)
{
    unsigned long size = strlen(string);
    char *new_string = malloc(size * sizeof(char) + 1);
    memset(new_string, '\0', size * sizeof(char) + 1);
    strncpy(new_string, string, size);
    return new_string;
}

static uint8_t char_to_uint8_t(char c)
{
    // A = 65 -> A = 1010 -> 10 in decimal. Thats why substract 55
    if(c == 'A' || (c > 'A' && c < 'F') || c == 'F')
    {
        return (uint8_t) c - 55;
    }
    // a = 97 -> a = 1010 -> 10 in decimal. Thats why substract 87
    else if(c == 'a' || (c > 'a' && c < 'f') || c == 'f')
    {
        return (uint8_t) c - 87;
    }
    // 0 = 65 -> A = 1010 -> 10 in decimal. Thats why substract 55
    else if(c == '0' || (c > '0' && c < '9') || c == '9')
    {
        return (uint8_t) c - 48;
    }
    return 0;
}

static uint16_t string_to_hex_uint16_t(const char *string)
{
    uint16_t cast = 0x0000;
    if (strlen(string) == 4)
    {
        cast = (uint16_t) ( (char_to_uint8_t(string[0]) << 12)
                          | (char_to_uint8_t(string[1]) << 8)
                          | (char_to_uint8_t(string[2]) << 4)
                          |  char_to_uint8_t(string[3]));
    }
    return cast;
}

static action_t* parse_build_task(char *json, int* size)
{
    action_t *acts = NULL;

    cJSON *root = cJSON_Parse(json);
    const cJSON *actions = cJSON_GetObjectItem(root, "actions");
    if(actions != NULL)
    {
        const cJSON *action = NULL;
        int size_actions = cJSON_GetArraySize(actions);

        if(size_actions != 0)
        {
            int index = 0;
            acts = (action_t *) malloc(size_actions * sizeof(action_t));
            memset(acts, 0, size_actions * sizeof(action_t));

            cJSON_ArrayForEach(action, actions)
            {

                const cJSON *auto_task = cJSON_GetObjectItem(action, "auto");
                const cJSON *opcode    = cJSON_GetObjectItem(action, "opcode");
                const cJSON *delay     = cJSON_GetObjectItem(action, "delay");
                const cJSON *name      = cJSON_GetObjectItem(action, "name");
                const cJSON *addr      = cJSON_GetObjectItem(action, "addr");

                if(auto_task == NULL && opcode == NULL
                    && delay == NULL && addr == NULL
                    && name != NULL && cJSON_IsString(name)) // tarea a borrar
                {
                    acts[index].opmode = REMOVE;
                    acts[index].task.name = name->valuestring;
                }
                else
                {
                    if(auto_task != NULL && cJSON_IsBool(auto_task)
                        && opcode != NULL && cJSON_IsString(opcode)
                        && delay != NULL && cJSON_IsNumber(delay)
                        && name != NULL && cJSON_IsString(name))
                    {
                        if(cJSON_IsTrue(auto_task))
                            acts[index].task.auto_task = true;
                        else
                            acts[index].task.auto_task = false;

                        acts[index].opmode      = CREATE;
                        acts[index].task.name   = sanitize_string(name->valuestring);
                        acts[index].task.delay  = delay->valueint;
                        acts[index].task.opcode = get_opcode(opcode->valuestring);
                        acts[index].task.addr   = string_to_hex_uint16_t(addr->valuestring);
                    }
                    else
                    {
                        // Error leyendo la tarea <task_name>
                        ESP_LOGE(TAG, "Error processing a task!");
                    }
                }
                index++;
            }
            *size = index; // Get real number of tasks that will be processed
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

    char buff[MAX_LENGHT_MESSAGE];

    for(;;)
    {
        xStatus = xQueueReceive(queue, &json_received, portMAX_DELAY);
        if(xStatus == pdTRUE)
        {
            message_t* message = create_message(PLAIN_TEXT);

            json_string = (char *) malloc(json_received.size * sizeof(char) + 1);
            memset(json_string, '\0',  json_received.size * sizeof(char) + 1);
            strncpy(json_string, json_received.json, json_received.size);
            ESP_LOGI(TAG, "Json received %s", json_string);

            // Check if json is correct
            size_actions = 0;
            actions = parse_build_task(json_string, &size_actions);
            if(size_actions != 0)
            {
                if(actions != NULL)
                {
                    for(int i = 0; i < size_actions; i++)
                    {
                        if(actions[i].opmode == REMOVE) // remove task
                        {
                            ESP_LOGI(TAG, "Removing task %s", actions[i].task.name);

                            task_t found = {
                                .name = actions[i].task.name,
                            };
                            task_t *task = obtain_task(&found);
                            if(task != NULL)
                            {
                                if(task->task_handler != NULL)
                                {
                                    vTaskDelete(task->task_handler);
                                    remove_task(task);

                                    ESP_LOGI(TAG, "Task %s removed", actions[i].task.name);

                                    memset(buff,'\0',MAX_LENGHT_MESSAGE);
                                    sprintf(buff, "Task %s deleted", actions[i].task.name);
                                    add_message_text_plain(&message->m_content.text_plain, buff);
                                }
                                else
                                {
                                    //message: there was a problem with the handler of task %s.
                                }
                            }
                            else
                            {
                                ESP_LOGE(TAG, "Task %s doesn't exists. Remove failed", actions[i].task.name);
                                memset(buff,'\0',MAX_LENGHT_MESSAGE);
                                sprintf(buff, "Task %s doesn't exists", actions[i].task.name);
                                add_message_text_plain(&message->m_content.text_plain, buff);
                            }
                        }
                        else if(actions[i].opmode == CREATE)
                        {
                            ESP_LOGI(TAG, "Creating task:\n\tname %s,\n\tauto %d,\n\topcode %u,\n\tdelay %d\n\taddr %X",
                                 actions[i].task.name, (int)actions[i].task.auto_task,
                                 actions[i].task.opcode, actions[i].task.delay,
                                 actions[i].task.addr);

                            // Check if the tasks exists
                            TaskHandle_t TaskHandle = NULL;
                            task_t *new_task = (task_t *)malloc(sizeof(task_t));
                            new_task->name = actions[i].task.name;
                            //new_task->task_handler = TaskHandle;

                            status_t status = add_new_task_if_not_exists(new_task);
                            if(status == CREATED)
                            {
                                ble_task_t aux;
                                memcpy(&aux, &actions[i].task, sizeof(ble_task_t));
                                xTaskCreate(&task_ble_cmd, aux.name, 2048, (void *) &aux, 5, &TaskHandle);
                                new_task->task_handler = TaskHandle; // save the handler after set it when create the task. IMPORTANT!!
                                //xTaskCreatePinnedToCore(&task_ble_cmd, aux.name, 2046, (void *) &aux, 5, &TaskHandle, 1);

                                memset(buff,'\0',MAX_LENGHT_MESSAGE);
                                sprintf(buff, "Task %s created", actions[i].task.name);
                                add_message_text_plain(&message->m_content.text_plain, buff);
                            }
                            else
                            {
                                // Send for mqtt message -> The task "name" exists!
                                ESP_LOGE(TAG, "Task - %s - exists!", actions[i].task.name);

                                memset(buff,'\0',MAX_LENGHT_MESSAGE);
                                sprintf(buff, "Task %s exists", actions[i].task.name);
                                add_message_text_plain(&message->m_content.text_plain, buff);
                            }
                            //free(new_task);
                        }
                        else
                        {
                            ESP_LOGE(TAG, "opmode couldn't be recognize!");
                        }
                    }
                }
            }
            else
            {
                ESP_LOGE(TAG, "Json could be processed or size task equals zero!");
            }

            send_message_queue(message);

            free(actions);
            free(json_string);
            free(message);
        }
        else
        {
            ESP_LOGE(TAG, "Error in queue -> task_receive_json");
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}