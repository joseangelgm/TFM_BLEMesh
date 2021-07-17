#ifndef _MESSAGES_PARSER_H_
#define _MESSAGES_PARSER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdint.h>

#define MAX_NUM_MESSAGES 20
#define MAX_LENGHT_MESSAGE 81// +1 -> \0

// Type of the messages, this will affect to message's parser
typedef enum {
    PLAIN_TEXT, // simple message with info, errors
    TASKS, // tasks list
    GET_STATUS,
//    GET_CADENCE,
    GET_DESCRIPTOR,
//    GET_SETTINGS,
//    GET_SERIES
    HEX_BUFFER
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
 * @brief Initialize the queue. This queue is used in mqtt.c.
 * When mqtt.c receive a message_t struct, call message_parser.c
 * to obtain a json.
 * @param queue: queue used to push elements
 */
void initialize_messages_parser_queue(QueueHandle_t queue);

/**
 * @brief queue a message_t*
 * @param m: message_t to queue
 */
void send_message_queue(message_t *message);

/**
 * @brief Helper function to add a new message
 * @param m: messate_t * struct
 * @param message: string with format
 * @param args: arguments to include in message
 */
void add_message_text_plain(message_t* m, const char* message, ...);

/**
 * @brief Helper function to fill a measure_t struct
 * @param m: message_t struct
 * @param addr: addr to add into measure_t
 * @param measure: measure to add into measure_t
 */
void add_measure_to_message(message_t* m, uint16_t addr, int measure);

/**
 * @brief Helper function to fill hex_buffer_t
 * @param m: messate_t * struct
 * @param data: uint8_t* to copy into hex_buffer_t
 * @param len: number of elements in data
 */
void add_hex_buffer(message_t* m, uint8_t* data, uint16_t len);

/**
 * @brief Return a string json that represent a message
 * Internally, transform the message based on message_type_t
 */
message_t* create_message(message_type_t type);

/**
 * @brief Return a json that represent a message_t
 * @param message: message_t * which we want a json string from
 */
char* message_to_json(message_t *message);

/**
 * @brief Free message_t struct
 * @param message: message_t *
 */
void free_message(message_t *message);

#endif