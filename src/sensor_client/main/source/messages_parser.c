#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

#include "source/messages_parser.h"

#define ADDR_SIZE 5

static const char *TAG = "MSG_PARSER";

static QueueHandle_t queue_message;
static char hex[] = "0123456789ABCDEF";

/**
 * @brief Initialize the queue. This queue is used in mqtt.c.
 * When mqtt.c receive a message_t struct, call message_parser.c
 * to obtain a json.
 */
void initialize_messages_parser_queue(QueueHandle_t queue)
{
    queue_message = queue;
}

/**
 * @brief queue a message_t
 */
void send_message_queue(message_t *m)
{
    xQueueSendToBack(queue_message, m, 0);
}

/****** FUNCTIONS TO PARSE message_type_t ******/

/**
 * @brief obtain a json from a TEXT_PLAIN or TASKS message type
 */
static char* text_plain_to_json(text_t *t, char* key)
{
    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

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
 */
static char* measure_to_json(measure_t *m)
{

    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

    char addr_str[ADDR_SIZE];
    sprintf(addr_str, "%X", m->addr);

    size_t size = strlen(addr_str);
    if(size < ADDR_SIZE - 1)
    {
        int shift = ADDR_SIZE - 1 - size;
        int i = 0;
        // shift
        for(i = ADDR_SIZE - 2; i >= shift; i--)
            addr_str[i] = addr_str[i - shift];

        for(; i >= 0; i--)
            addr_str[i] = '0';
    }

    cJSON *addr = cJSON_CreateString(addr_str);
    if(addr == NULL)
        goto error;

    cJSON *measure = cJSON_CreateNumber(m->value);
    if(measure == NULL)
        goto error;

    cJSON_AddItemToObject(root, "addr", addr);
    cJSON_AddItemToObject(root, "measure", measure);

    json = cJSON_Print(root);

error:
    cJSON_Delete(root);
    return json;
}

/**
 * @brief Get char representation from a uint8_t.
 * value is decode into ms and ls.
 */
static void uint8_to_char(uint8_t value, char* ms, char* ls)
{
    uint8_t ms_value = value >> 4;

    *ms = hex[(int)ms_value];
    *ls = hex[(int)(value & 0xF)];
}

/**
 * @brief Get char* representation from a uint8_t*.
 * Use uint8_to_char function
 */
static char* uint8_array_to_string(uint8_t *val, uint16_t len)
{
    size_t size = (sizeof(char) * len * 2);
    char* buff = (char *)malloc(size + 1); // '\0'
    memset(buff, '\0', size + 1);

    for (size_t i = 0; i < len ; i++)
	{
        uint8_to_char(val[i], &buff[i * 2], &buff[(i * 2) + 1]);
	}

    return buff;
}

/**
 * @brief obtain a json from a HEX_BUFFER type
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

/***********************************************/

/****** HELPER FUNCTIONS TO SET STRUCTURES *****/

/**
 * @brief Helper function to add a new message
 */
void add_message_text_plain(text_t* text, const char* string)
{
    if(text->num_messages < MAX_NUM_MESSAGES)
    {
        strncpy(text->messages[text->num_messages], string, MAX_LENGHT_MESSAGE);
        text->num_messages++;
    }
}

/**
 * @brief Helper function to a measure
 */
void add_measure_to_message(message_t* m, uint16_t addr, int measure)
{
    m->m_content.measure.value = measure;
    m->m_content.measure.addr = addr;
}

/**
 * @brief Helper function to set hex buffer
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
 */
message_t* create_message(message_type_t type)
{
    message_t* message = (message_t*) malloc(sizeof(message_t));

    if(type == PLAIN_TEXT || type == TASKS)
    {
        message->m_content.text_plain.num_messages = 0;
    }
    else if(type == MEASURE)
    {
        message->m_content.measure.value = 0;
        message->m_content.measure.addr = 0x0000;
    }
    else if(type == HEX_BUFFER)
    {
        message->m_content.hex_buffer.data = NULL;
        message->m_content.hex_buffer.len = 0;
    }

    message->type = type;
    return message;
}

/**
 * @brief Return a json that represent a message_t
 */
char* message_to_json(message_t *message)
{
    if(message->type == PLAIN_TEXT)
        return text_plain_to_json(&message->m_content.text_plain, "messages");

    if(message->type == TASKS)
        return text_plain_to_json(&message->m_content.text_plain, "tasks");

    if(message->type == MEASURE)
        return measure_to_json(&message->m_content.measure);

    if(message->type == HEX_BUFFER)
        return get_hex_buffer_to_json(&message->m_content.hex_buffer, "descriptor");

    return NULL;
}

/**
 * @brief Free message_t struct
 */
void free_message(message_t *message)
{
    if(message->type == HEX_BUFFER)
    {
        free(message->m_content.hex_buffer.data);
    }

    free(message);
}