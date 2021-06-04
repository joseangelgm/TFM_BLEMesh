#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

#include "source/messages_parser.h"

static const char *TAG = "MES_PARSER";

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
 * @brief queue a message_t
 */
void send_message_queue(message_t *m)
{
    xQueueSendToBack(queue_message, m, 0);
}

static char* message_to_text_plain(text_t *t)
{
    char* json = NULL;
    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
        goto error;

    cJSON *messages = cJSON_CreateArray();
    if (messages == NULL)
        goto error;

    ESP_LOGW(TAG, "Processing messages -> %d", t->num_messages);

    cJSON_AddItemToObject(root, "messages", messages);
    cJSON *message = NULL;
    char buff[MAX_LENGHT_MESSAGE];

    for(int i = 0; i < t->num_messages; i++)
    {
        memset(buff, '\0',  MAX_LENGHT_MESSAGE);
        strcpy (buff, t->messages[i]);
        ESP_LOGW(TAG, "messages %d -> %s", i, t->messages[i]);

        message = cJSON_CreateString(buff);
        if(message == NULL)
            goto error;
        cJSON_AddItemToArray(messages, message);
    }
    ESP_LOGW(TAG, "Terminando");
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
        return message_to_text_plain(&message->m_content.text_plain);

    return NULL;
}

message_t* create_message_text_plain()
{
    message_t* message = malloc(sizeof(message_t));
    message->m_content.text_plain.num_messages = 0;
    message->type = PLAIN_TEXT;
    return message;
}

void add_message_text_plain(text_t* text, char* string)
{
    if(text->num_messages < MAX_NUM_MESSAGES)
    {
        strcpy(text->messages[text->num_messages], string);
        text->num_messages++;
    }
}