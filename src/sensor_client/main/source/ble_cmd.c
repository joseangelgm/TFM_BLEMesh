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
extern void ble_mesh_send_sensor_message(uint32_t opcode, uint16_t addr, uint16_t sensor_prop_id);

/**
 * @brief Return opcode string-like into uint32_t.
 * This type is used in BLE Mesh.
 * @param opcode: opcode as a string
 * @retval uint32_t opcode
 */
static uint32_t get_opcode(const char *opcode)
{
    /* Supported opcodes by esp ble mesh at 19-07-2021 */
    if(strcmp(opcode, "GET_DESCRIPTOR") == 0)
        return ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET;

    if(strcmp(opcode, "GET_STATUS") == 0)
        return ESP_BLE_MESH_MODEL_OP_SENSOR_GET;

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
 * @brief Return if auto value is required in json
 * @param opcode: BLE Mesh message opcode
 * @retval true or false.
 */
static bool is_auto_required(uint32_t opcode)
{
    return opcode == ESP_BLE_MESH_MODEL_OP_SENSOR_GET;
}

/**
 * @brief Build a task based on a sigle element from json
 * @param ble_task: pointer to struct where a task will be store in
 * @param action: single action json received from MQTT
 * @param messages: struct to store messages for giving feedback to the user
 * @retval whether if the task could be created properly
 */
static bool build_task(action_t *ble_task, const cJSON* action, message_t *messages)
{
    bool build = true;

    const cJSON *auto_task      = cJSON_GetObjectItem(action, "auto");
    const cJSON *opcode         = cJSON_GetObjectItem(action, "opcode");
    const cJSON *delay          = cJSON_GetObjectItem(action, "delay");
    const cJSON *name           = cJSON_GetObjectItem(action, "name");
    const cJSON *addr           = cJSON_GetObjectItem(action, "addr");
    const cJSON *sensor_prop_id = cJSON_GetObjectItem(action, "sensor_prop_id");

    // Task to delete
    if(opcode == NULL && delay == NULL
        && auto_task == NULL && addr == NULL
        && name != NULL)
    {
        ble_task->opmode = REMOVE;
        ble_task->task.name = name->valuestring;
    }
    // Task to create -> one time
    else if(opcode != NULL && addr != NULL)
    {
        ble_task->opmode      = CREATE;
        ble_task->task.opcode = get_opcode(opcode->valuestring);
        ble_task->task.addr   = string_to_hex_uint16_t(addr->valuestring);

        if(sensor_prop_id != NULL)
        {
            ble_task->task.sensor_prop_id = string_to_hex_uint16_t(sensor_prop_id->valuestring);
        }
        else
        {
            ble_task->task.sensor_prop_id = 0x0000;
        }


        if(auto_task != NULL) // task to create periodically
        {
            if(cJSON_IsTrue(auto_task) && is_auto_required(ble_task->task.opcode))
            {
                ble_task->task.auto_task = true;
                ble_task->task.name   = sanitize_string(name->valuestring);
                ble_task->task.delay  = delay->valueint;
            }
            else
            {
                ble_task->task.auto_task = false;
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error processing a task!");
        add_message_text_plain(messages, "Error processing a task!");
        build = false;
    }
    return build;
}

/**
 * @brief Parse json and returns task to be launched or removed.
 *
 * @param message_t*: struct to store messages to give feedback
 * @param json: json received through mqtt.
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
                if(build_task(&acts[index], action, messages))
                    index++;
            }
            *size = index;
        }
        else{
            ESP_LOGE(TAG, "Action size is zero");
            add_message_text_plain(messages, "Action size is zero");
        }
    }
    else
    {
        add_message_text_plain(messages, "Action list required");
        ESP_LOGE(TAG, "Action list required");
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
 * @brief Function to use as a task.
 * Every json received will be converted into ble_task_t
 * and then will be launch a task with this function.
 */
static void task_ble_cmd(void *params)
{
    ble_task_t ble_task = (*(ble_task_t *) params);

    if(ble_task.auto_task)
    {
        ESP_LOGI(TAG, "[%s] auto = %d, opcode = 0x%04X, delay = %d, addr = 0x%04X, sensor_prop_id = 0x%04X", ble_task.name, (int)ble_task.auto_task, ble_task.opcode, ble_task.delay, ble_task.addr, ble_task.sensor_prop_id);
        for(;;)
        {
            ble_mesh_send_sensor_message(ble_task.opcode, ble_task.addr, ble_task.sensor_prop_id);
            vTaskDelay(ble_task.delay * 1000 / portTICK_PERIOD_MS);
        }
    }
    else
    {
        ESP_LOGI(TAG, "[One-time task] opcode = 0x%04X, addr = 0x%04X, sensor_prop_id = 0x%04X", ble_task.opcode, ble_task.addr, ble_task.sensor_prop_id);
        ble_mesh_send_sensor_message(ble_task.opcode, ble_task.addr, ble_task.sensor_prop_id);
    }

    vTaskDelete(NULL);
}

/**
 * @brief Create a new task
 * @param ble_task: ble_task_t* to create
 * @param messages: messages_t* struct to store message to send over MQTT
 */
static void create_task(ble_task_t *ble_task, message_t *messages)
{

    // If it is auto, it will be register into task_manager
    if(ble_task->auto_task)
    {

        // Check if the tasks exists
        TaskHandle_t TaskHandle = NULL;
        task_t *new_task = (task_t *)malloc(sizeof(task_t));
        new_task->name = ble_task->name;

        status_t status = add_new_task_if_not_exists(new_task);
        if(status == CREATED)
        {
            ble_task_t aux;
            memcpy(&aux, ble_task, sizeof(ble_task_t));
            xTaskCreate(&task_ble_cmd, aux.name, 2048, (void *) &aux, 5, &TaskHandle);
            new_task->task_handler = TaskHandle; // save the handler after set it when create the task. IMPORTANT!!

            ESP_LOGI(TAG, "Task %s created", ble_task->name);
            add_message_text_plain(messages, "Task %s created", ble_task->name);
        }
        else
        {
            free(new_task);
            ESP_LOGE(TAG, "Task - %s - exists!", ble_task->name);
            add_message_text_plain(messages, "Task %s exists", ble_task->name);
        }
    }
    // If it is not auto task, just create it.
    else
    {
        ble_task_t aux;
        memcpy(&aux, ble_task, sizeof(ble_task_t));

        // Create a name for one time task
        char buff[80];
        sprintf(buff, "Task 0x%04x, addr 0x%04x", aux.opcode, aux.addr);

        xTaskCreate(&task_ble_cmd, buff, 2048, (void *) &aux, 5, NULL);
        add_message_text_plain(messages, "One-time Task with opcode 0x%04x, addr 0x%04x launched", ble_task->opcode, ble_task->addr);
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