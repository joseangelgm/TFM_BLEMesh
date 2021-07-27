#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdarg.h>

#include "source/messages_parser.h"
#include "source/data_format.h"

static const char *TAG = "MSG_PARSER";

static QueueHandle_t queue_message;

/**
 * @brief Initialize the queue. This queue is used in mqtt.c.
 * When mqtt.c receive a message_t struct, call message_parser.c
 * to obtain a json.
 * @param queue: queue used to push elements
 */
void initialize_messages_parser_queue(QueueHandle_t queue)
{
    queue_message = queue;
}

/**
 * @brief queue a message_t*
 * @param m: message_t to queue
 */
void send_message_queue(message_t *m)
{
    xQueueSendToBack(queue_message, (void *) &m, 0);
}

/****** FUNCTIONS TO PARSE message_type_t ******/

/**
 * @brief obtain a json from a TEXT_PLAIN or TASKS message type
 * @param t: text_t struct
 * @param key: key to use in json -> {key: text_t as string}
 * @retval json
 */
static char* text_plain_to_json(text_t *t, char* key)
{
    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;


    cJSON *error_message = cJSON_CreateBool(t->error_message);
    if(error_message == NULL)
        goto error;

    cJSON_AddItemToObject(root, "error_message", error_message);

    cJSON *messages = cJSON_CreateArray();
    if (messages == NULL)
        goto error;

    cJSON_AddItemToObject(root, key, messages);
    cJSON *message = NULL;
    char buff[MAX_LENGHT_MESSAGE];

    for(int i = 0; i < t->num_messages; i++)
    {
        memset(buff, '\0',  MAX_LENGHT_MESSAGE);
        strncpy(buff, t->messages[i], strlen(t->messages[i]));

        message = cJSON_CreateString(buff);
        if(message == NULL)
            goto error;

        cJSON_AddItemToArray(messages, message);
    }

    json = cJSON_Print(root);

error:
    cJSON_Delete(root);
    return json;
}

/**
 * @brief obtain a json from MEASURE type
 * @param m: measure_t struct
 * @retval json
 */
static char* get_status_to_json(measure_t *m)
{

    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

    char* addr_str = uint16_to_string(m->addr);

    cJSON *addr = cJSON_CreateString(addr_str); free(addr_str);
    if(addr == NULL)
        goto error;

    char* sensor_prop_id_str = uint16_to_string(m->sensor_prop_id);

    cJSON *sensor_prop_id_json = cJSON_CreateString(sensor_prop_id_str); free(sensor_prop_id_str);
    if(sensor_prop_id_json == NULL)
        goto error;

    cJSON *measure = cJSON_CreateNumber(m->value);
    if(measure == NULL)
        goto error;

    cJSON_AddItemToObject(root, "sensor_prop_id", sensor_prop_id_json);
    cJSON_AddItemToObject(root, "addr", addr);
    cJSON_AddItemToObject(root, "measure", measure);

    json = cJSON_Print(root);

error:

    cJSON_Delete(root);
    return json;
}

/**
 * @brief obtain a json from a HEX_BUFFER type
 * @param hex: hex_buffer_t struct
 * @param key: key in json -> {key: hex_buffer_t as string}
 * @retval json
 */
static char* get_hex_buffer_to_json(hex_buffer_t *hex, const char* key)
{
    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

    char *buff = uint8_array_to_string(hex->data, hex->len);

    cJSON *data = cJSON_CreateString(buff);
    if(data == NULL)
        goto error;

    cJSON_AddItemToObject(root, key, data);

    json = cJSON_Print(root);

error:
    cJSON_Delete(root);
    return json;
}

/**
 * @brief obtain a json from a GET_DESCRIPTOR type
 * @param hex: hex_buffer_t struct
 * @retval json
 */
static char* get_descriptor_to_json(hex_buffer_t *hex)
{
    /*
    struct sensor_descriptor {
        uint16_t sensor_prop_id;
        uint32_t pos_tolerance:12,
                 neg_tolerance:12,
                 sample_func:8;
        uint8_t  measure_period;
        uint8_t  update_interval;
    }__attribute__((packed));
    */

    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

    cJSON *type = cJSON_CreateString("GET_DESCRIPTOR");
    if(type == NULL)
        goto error;

    cJSON_AddItemToObject(root, "type", type);

    cJSON *descriptors = cJSON_AddArrayToObject(root, "descriptors");
    if(descriptors == NULL)
        goto error;

    for(int i = 0; i < hex->len; i += 8)
    {
        cJSON *descriptor = cJSON_CreateObject();
        cJSON_AddItemToArray(descriptors, descriptor);

        // sensor_prop_id
        uint8_t sensor_prop_id[] = {
            hex->data[i],
            hex->data[i + 1],
        };

        char *sensor_prop_id_str = uint8_array_to_string(sensor_prop_id, 2);

        cJSON *sensor_prop_id_json = cJSON_CreateString(sensor_prop_id_str); free(sensor_prop_id_str);
        if(sensor_prop_id_json == NULL)
            goto error;

        // positive tolerance
        uint8_t pos_tolerance[] = {
            hex->data[i + 2],
            hex->data[i + 3] >> 4,
        };

        char *pos_tolerance_str = uint8_array_to_string(pos_tolerance, 2);

        cJSON *pos_tolerance_json = cJSON_CreateString(pos_tolerance_str); free(pos_tolerance_str);
        if(pos_tolerance_json == NULL)
            goto error;

        // negative tolerance
        uint8_t neg_tolerance[] = {
            hex->data[i + 3] & 0xF,
            hex->data[i + 4],
        };

        char *neg_tolerance_str = uint8_array_to_string(neg_tolerance, 2);

        cJSON *neg_tolerance_json = cJSON_CreateString(neg_tolerance_str); free(neg_tolerance_str);
        if(neg_tolerance_json == NULL)
            goto error;

        // sample_func
        uint8_t sample_func[] = {
            hex->data[i + 5]
        };

        char *sample_func_str = uint8_array_to_string(sample_func, 1);

        cJSON *sample_func_json = cJSON_CreateString(sample_func_str); free(sample_func_str);
        if(sample_func_json == NULL)
            goto error;

        // measure_period
        uint8_t measure_period[] = {
            hex->data[i + 6]
        };

        char *measure_period_str = uint8_array_to_string(measure_period, 1);

        cJSON *measure_period_json = cJSON_CreateString(measure_period_str); free(measure_period_str);
        if(measure_period_json == NULL)
            goto error;

        // update_interval
        uint8_t update_interval[] = {
            hex->data[i + 7]
        };

        char *update_interval_str = uint8_array_to_string(update_interval, 1);

        cJSON *update_interval_json = cJSON_CreateString(update_interval_str); free(update_interval_str);
        if(update_interval_json == NULL)
            goto error;

        // build json
        cJSON_AddItemToObject(descriptor, "sensor_prop_id", sensor_prop_id_json);
        cJSON_AddItemToObject(descriptor, "pos_tolerance", pos_tolerance_json);
        cJSON_AddItemToObject(descriptor, "neg_tolerance", neg_tolerance_json);
        cJSON_AddItemToObject(descriptor, "sample_function", sample_func_json);
        cJSON_AddItemToObject(descriptor, "measure_period", measure_period_json);
        cJSON_AddItemToObject(descriptor, "update_interval", update_interval_json);

    }

    json = cJSON_Print(root);

error:
    cJSON_Delete(root);
    return json;
}

/***********************************************/

/****** HELPER FUNCTIONS TO SET STRUCTURES *****/

/**
 * @brief Helper function to add a new message
 * @param m: messate_t * struct
 * @param message: string with format
 * @param args: arguments to include in message
 */
void add_message_text_plain(message_t* m, bool error_message, const char* message, ...)
{
    text_t *text = &m->m_content.text_plain;

    if(error_message)
        text->error_message = error_message;

    if(text->num_messages < MAX_NUM_MESSAGES)
    {
        char buff[MAX_LENGHT_MESSAGE];
        memset(buff,'\0', MAX_LENGHT_MESSAGE);

        va_list args;
        va_start(args, message);
        vsprintf(buff, message, args);
        va_end(args);

        strncpy(text->messages[text->num_messages], buff, MAX_LENGHT_MESSAGE - 1);
        text->num_messages++;
    }
}

/**
 * @brief Helper function to fill a measure_t struct
 * @param m: message_t struct
 * @param addr: addr to add into measure_t
 * @param sensor_prop_id: sensor prop id which measure blongs to
 * @param measure: measure to add into measure_t
 */
void add_measure_to_message(message_t* m, uint16_t addr, uint16_t sensor_prop_id, int measure)
{
    m->m_content.measure.value = measure;
    m->m_content.measure.addr = addr;
    m->m_content.measure.sensor_prop_id = sensor_prop_id;
}

/**
 * @brief Helper function to fill hex_buffer_t
 * @param m: messate_t * struct
 * @param data: uint8_t* to copy into hex_buffer_t
 * @param len: number of elements in data
 */
void add_hex_buffer(message_t* m, uint8_t* data, uint16_t len)
{
    m->m_content.hex_buffer.data = malloc(sizeof(uint8_t) * len);
    m->m_content.hex_buffer.len = len;

    for(uint16_t i = 0; i < len; i++)
    {
        m->m_content.hex_buffer.data[i] = data[i];
    }
}

/**********************************************/

/**
 * @brief Return message_t prepared based on type given
 * @param type: type of message_t we want to create
 * @retval message_t prepared
 */
message_t* create_message(message_type_t type)
{
    message_t* message = (message_t*) malloc(sizeof(message_t));

    if(type == PLAIN_TEXT || type == TASKS)
    {
        ESP_LOGI(TAG, "Creating PLAIN_TEXT, TASKS");
        message->m_content.text_plain.num_messages = 0;
        message->m_content.text_plain.error_message = false;
    }
    else if(type == GET_STATUS)
    {
        ESP_LOGI(TAG, "Creating GET_STATUS");
        message->m_content.measure.value = 0;
        message->m_content.measure.addr = 0x0000;
    }
    else if(type == HEX_BUFFER || type == GET_DESCRIPTOR)
    {
        ESP_LOGI(TAG, "Creating HEX_BUFFER, GET_DESCRIPTOR");
        message->m_content.hex_buffer.data = NULL;
        message->m_content.hex_buffer.len = 0;
    }

    message->type = type;
    return message;
}

/**
 * @brief Return a json that represent a message_t
 * @param message: message_t * which we want a json string from
 */
char* message_to_json(message_t *message)
{
    if(message->type == PLAIN_TEXT)
        return text_plain_to_json(&message->m_content.text_plain, "messages");

    if(message->type == TASKS)
        return text_plain_to_json(&message->m_content.text_plain, "tasks");

    if(message->type == GET_STATUS)
        return get_status_to_json(&message->m_content.measure);

    if(message->type == GET_DESCRIPTOR)
        return get_descriptor_to_json(&message->m_content.hex_buffer);

    if(message->type == HEX_BUFFER)
        return get_hex_buffer_to_json(&message->m_content.hex_buffer, "hex buffer");

    return NULL;
}

/**
 * @brief Free message_t struct
 * @param message: message_t *
 */
void free_message(message_t *message)
{
    if(message != NULL)
    {
        if(message->type == HEX_BUFFER || message->type == GET_DESCRIPTOR)
        {
            free(message->m_content.hex_buffer.data);
        }
        free(message);
    }
}