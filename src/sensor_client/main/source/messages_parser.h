#ifndef _MESSAGES_PARSER_H_
#define _MESSAGES_PARSER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MAX_NUM_MESSAGES 20
#define MAX_LENGHT_MESSAGE 81// +1 -> \0

// Type of the messages, this will affect to message's parser
typedef enum {
    PLAIN_TEXT, // simple message with info, errors
    TASKS,
    MEASURE,
    GET_STATUS,
    GET_CADENCE,
    GET_DESCRIPTOR,
    GET_SETTINGS,
    GET_SERIES
} message_type_t;

/*********** Types of messages ******************/
// plain text: errors, info messages
typedef struct text_t {
    int num_messages;
    char messages[MAX_NUM_MESSAGES][MAX_LENGHT_MESSAGE];
} text_t;

/* A message will only have one of the following fields */
typedef union message_content_t {
    text_t text_plain;
} message_content_t;

/* General structure for every message */
typedef struct message_t {
    message_type_t type;
    message_content_t m_content;
} message_t;

/**
 * @brief Initialize the queue
 */
void initialize_messages_parser_queue(QueueHandle_t queue);

/**
 * @brief queue a message_t
 */
void send_message_queue(message_t *message);

/**
 * @brief Return a string json that represent a message
 * Internally, transform the message based on message_type_t
 */
char* message_to_json(message_t *message);

/**
 * @brief Return message_t prepared to be PLAIN_TEXT type
 */
message_t* create_message_text_plain();

/**
 * @brief Return message_t prepared to be TASKS type
 */
message_t* create_message_tasks();

/**
 * @brief add a new message to a message of type text_plain
 */
void add_message_text_plain(text_t* text, char* string);

#endif