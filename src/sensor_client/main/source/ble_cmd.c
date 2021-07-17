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
#include "source/data_format.h"

static const char *TAG = "BLE_CMD";

// function in sensor_model_client.c to send a message of type 'opcode' to a addr
extern void ble_mesh_send_sensor_message(uint32_t opcode, uint16_t addr);

/**
 * @brief Return opcode string-like into uint32_t.
 * This type is used in BLE Mesh.
 * @param opcode: opcode as a string
 * @retval uint32_t opcode
 */
static uint32_t get_opcode(const char *opcode)
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

/**
 * @brief Return the same char* ending with \0.
 * For example, it is used for task name since
 * it is not \0-ended.
 * @param string: string to be sanitize
 * @retval same string sanitized
 */
static char* sanitize_string(const char* string)
{
    size_t size = strlen(string);
    char *new_string = malloc(size * sizeof(char) + 1);
    memset(new_string, '\0', size * sizeof(char) + 1);
    strncpy(new_string, string, size);
    return new_string;
}

/**
 * @brief Function to use as a task.
 * Every json received will be converted into ble_task_t
 * and then will be launch a task with this function.
 */
static void task_ble_cmd(void *params)
{
    ble_task_t *ble_task = (ble_task_t *) params;
    ESP_LOGI(TAG, "[%s] auto = %d, opcode = %d, delay = %d, addr = %X", ble_task->name, (int)ble_task->auto_task, ble_task->opcode, ble_task->delay, ble_task->addr);

    if(ble_task->auto_task)
    {
        for(;;)
        {
            ble_mesh_send_sensor_message(ble_task->opcode, ble_task->addr);
            vTaskDelay(ble_task->delay * 1000 / portTICK_PERIOD_MS);
        }
    }
    else
    {
        ble_mesh_send_sensor_message(ble_task->opcode, ble_task->addr);
    }

    vTaskDelete(NULL);
}

static bool is_auto_task(uint32_t opcode)
{
    return opcode == ESP_BLE_MESH_MODEL_OP_SENSOR_GET;
}

/**
 * @brief Parse json and returns task to be launched or removed.
 *
 * @param json: json received through mqtt.
 * @param message_t*: struct to store messages to give feedback
 * @retval size: size of action_t array.
 * @retval action_t*: array of tasks.
 */
static action_t* parse_build_task(message_t* messages, char *json, int* size)
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

                // Task to delete
                if(opcode == NULL && delay == NULL
                    && addr == NULL && name != NULL)
                {
                    // If name is a string
                    if(cJSON_IsString(name))
                    {
                        acts[index].opmode = REMOVE;
                        acts[index].task.name = name->valuestring;
                    }
                }
                // Task to create
                else if(auto_task != NULL && cJSON_IsBool(auto_task)
                        && opcode != NULL && cJSON_IsString(opcode)
                        && delay  != NULL && cJSON_IsNumber(delay)
                        && name   != NULL && cJSON_IsString(name))
                {

                    acts[index].opmode      = CREATE;
                    acts[index].task.name   = sanitize_string(name->valuestring);
                    acts[index].task.delay  = delay->valueint;
                    acts[index].task.opcode = get_opcode(opcode->valuestring);
                    acts[index].task.addr   = string_to_hex_uint16_t(addr->valuestring);

                    //Determine if task is auto or not depending of opcode an auto_task
                    bool can_be_auto = is_auto_task(acts[index].task.opcode);

                    if(cJSON_IsTrue(auto_task) && can_be_auto)
                        acts[index].task.auto_task = true;
                    else
                        acts[index].task.auto_task = false;
                }
                else
                {
                    // Error leyendo la tarea <task_name>
                    ESP_LOGE(TAG, "Error processing a task!");
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

/**
 * @brief delete a running task
 * @param ble_task: ble_task_t* to delete
 * @param messages: messages_t* struct to store message to send over MQTT
 */
static void delete_task(ble_task_t *ble_task, message_t *messages)
{
    ESP_LOGI(TAG, "Deleting task %s", ble_task->name);

    task_t found = {
        .name = ble_task->name,
    };
    task_t *task = obtain_task(&found);

    if(task != NULL)
    {
        if(task->task_handler != NULL)
        {
            vTaskDelete(task->task_handler);
            remove_task(task);

            ESP_LOGI(TAG, "Task %s removed", ble_task->name);
            add_message_text_plain(messages, "Task %s deleted", ble_task->name);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Task %s doesn't exists. Remove failed", ble_task->name);
        add_message_text_plain(messages, "Task %s doesn't exists. Remove failed", ble_task->name);
    }
}

/**
 * @brief Create a new task
 * @param ble_task: ble_task_t* to create
 * @param messages: messages_t* struct to store message to send over MQTT
 */
static void create_task(ble_task_t *ble_task, message_t *messages)
{
    ESP_LOGI(TAG, "Creating task:\n\tname %s,\n\tauto %d,\n\topcode %u,\n\tdelay %d\n\taddr %X",
        ble_task->name, (int)ble_task->auto_task,
        ble_task->opcode, ble_task->delay,
        ble_task->addr);

    // If it is auto, it will be register into task_manager
    if(ble_task->auto_task)
    {
        ESP_LOGI(TAG, "Task %s is auto", ble_task->name);

        // Check if the tasks exists
        TaskHandle_t TaskHandle = NULL;
        task_t *new_task = (task_t *)malloc(sizeof(task_t));
        new_task->name = ble_task->name;

        status_t status = add_new_task_if_not_exists(new_task);
        if(status == CREATED)
        {
            ble_task_t aux;
            memcpy(&aux, &ble_task, sizeof(ble_task_t));
            xTaskCreate(&task_ble_cmd, aux.name, 2048, (void *) &aux, 5, &TaskHandle);
            new_task->task_handler = TaskHandle; // save the handler after set it when create the task. IMPORTANT!!

            add_message_text_plain(messages, "Task %s created", ble_task->name);
        }
        else
        {
            ESP_LOGE(TAG, "Task - %s - exists!", ble_task->name);
            add_message_text_plain(messages, "Task %s exists", ble_task->name);
        }
    }
    // If it is not auto task, just create it.
    else
    {
        ESP_LOGI(TAG, "Task %s is not auto", ble_task->name);

        ble_task_t aux;
        memcpy(&aux, &ble_task, sizeof(ble_task_t));
        xTaskCreate(&task_ble_cmd, aux.name, 2048, (void *) &aux, 5, NULL);
        add_message_text_plain(messages, "Task %s launched", ble_task->name);
    }
}

/**
 * @brief task to parse mqtt_json structs to create tasks
 * @param params: pointer to QueueHandle_t queue
 */
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
            message_t* messages = create_message(PLAIN_TEXT);

            json_string = (char *) malloc(json_received.size * sizeof(char) + 1);
            memset(json_string, '\0',  json_received.size * sizeof(char) + 1);
            strncpy(json_string, json_received.json, json_received.size);
            ESP_LOGI(TAG, "Json received %s", json_string);

            // Check if json is correct
            size_actions = 0;
            actions = parse_build_task(messages, json_string, &size_actions); free(json_string);
            if(size_actions != 0 && actions != NULL)
            {
                for(int i = 0; i < size_actions; i++)
                {
                    if(actions[i].opmode == REMOVE) // remove task
                    {
                        delete_task(&actions[i].task, messages);
                    }
                    else if(actions[i].opmode == CREATE)
                    {
                        create_task(&actions[i].task, messages);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "opmode couldn't be recognize!");
                    }
                }
            }
            else
            {
                ESP_LOGE(TAG, "Json could be processed or size task equals zero!");
            }
            send_message_queue(messages);
            free(actions);
        }
        else
        {
            ESP_LOGE(TAG, "Error in queue -> task_receive_json");
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}