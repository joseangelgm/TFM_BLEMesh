#ifndef _MESSAGES_PARSER_H_
#define _MESSAGES_PARSER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MAX_NUM_MESSAGES 20
#define MAX_LENGHT_MESSAGE 81// +1 -> \0

// Type of the messages, this will affect to message's parser
typedef enum {
    PLAIN_TEXT, // simple message with info, errors
    TASKS, // tasks list
    MEASURE,
    HEX_BUFFER
//    GET_STATUS,
//    GET_CADENCE,
//    GET_DESCRIPTOR,
//    GET_SETTINGS,
//    GET_SERIES
} message_type_t;

/*********** Types of messages ******************/
// plain text: errors, info messages, tasks list
typedef struct text_t {
    int num_messages;
    char messages[MAX_NUM_MESSAGES][MAX_LENGHT_MESSAGE];
} text_t;

typedef struct measure_t {
    uint16_t addr;
    int value;
} measure_t;

typedef struct hex_buffer_t {
    uint8_t* data;
    uint16_t len;
} hex_buffer_t;

/************************************************/


/* A message will only have one of the following fields */
typedef union message_content_t {
    text_t text_plain;
    measure_t measure;
    hex_buffer_t hex_buffer;
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
 * @brief Return message_t prepared for the type given
 */
message_t* create_message(message_type_t type);

/**
 * @brief add a new message to a message of type text_plain
 */
void add_message_text_plain(text_t* text, const char* string);

void add_measure_to_message(message_t* m, uint16_t addr, int measure);

void add_hex_buffer(message_t* m, uint8_t* data, uint16_t len);

#endif