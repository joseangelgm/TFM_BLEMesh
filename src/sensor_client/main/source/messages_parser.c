#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

#include "source/messages_parser.h"

static const char *TAG = "MSG_PARSER";

static QueueHandle_t queue_message;

/**
 * @brief Initialize the queue. It will be used for:
 *  - Get the responses from ble
 *  - Get errors when create or delete tasks
*/
void initialize_messages_parser_queue(QueueHandle_t queue)
{
    queue_message = queue;
}

/**
 * @brief queue a message_t. It will be process in mqtt module.
 */
void send_message_queue(message_t *m)
{
    xQueueSendToBack(queue_message, m, 0);
}

/**
 * @brief obtain a string from a TEXT_PLAIN or TASKS message type
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
 * @brief obtaint a string from MEASURE type
 */
static char* measure_to_json(measure_t *m)
{

    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

    char addr_str[5];
    memset(addr_str, '\0', sizeof(char) * 5);
    sprintf(addr_str, "%X", m->addr);

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
 * @brief Return a string json that represent a message
 * Internally, transform the message based on message_type_t
 */
char* message_to_json(message_t *message)
{
    if(message->type == PLAIN_TEXT)
        return text_plain_to_json(&message->m_content.text_plain, "messages");

    if(message->type == TASKS)
        return text_plain_to_json(&message->m_content.text_plain, "tasks");

    if(message->type == MEASURE)
        return measure_to_json(&message->m_content.measure);

    return NULL;
}

/**
 * @brief Return message_t prepared for the type given
 */
message_t* create_message(message_type_t type)
{
    message_t* message = NULL;

    if(type == PLAIN_TEXT || type == TASKS)
    {
        message = (message_t*) malloc(sizeof(message_t));
        message->m_content.text_plain.num_messages = 0;
        message->type = type;
        return message;
    }

    if(type == MEASURE)
    {
        message = (message_t*) malloc(sizeof(message_t));
        message->m_content.measure.value = 0;
        message->m_content.measure.addr = 0x0000;
        message->type = type;
        return message;
    }

    return NULL;
}

/**
 * add a message to a text_plain message type
 */
void add_message_text_plain(text_t* text, char* string)
{
    if(text->num_messages < MAX_NUM_MESSAGES)
    {
        strncpy(text->messages[text->num_messages], string, MAX_LENGHT_MESSAGE);
        text->num_messages++;
    }
}

void add_measure_to_message(message_t* m, uint16_t addr, int measure)
{
    m->m_content.measure.value = measure;
    m->m_content.measure.addr = addr;
}