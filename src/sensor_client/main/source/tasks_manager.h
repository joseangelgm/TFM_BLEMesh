#ifndef _TASK_MANAGER_
#define _TASK_MANAGER_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct task_t {
    char *name;
    TaskHandle_t task_handler;
} task_t;

typedef struct node_t {
    task_t *task;
    struct node_t *next;
    struct node_t *prev;
} node_t;

typedef struct tasks_t {
    node_t *first;
    node_t *last;
    unsigned int num_tasks;
} tasks_t;

typedef enum {
    EXISTS,
    NOT_EXISTS,
    NOT_FOUND,
    CREATED
} status_t;

/* Creation */
void init_tasks_manager();
void free_tasks_manager();

/* Compare */
status_t task_exists(task_t *new_task);

/* Add */
status_t add_new_task_if_not_exists(task_t *new_task);

/* Obtain */
task_t* obtain_task(task_t* task);

/* Remove */
status_t remove_task(task_t* remove_task);

/* Queue a message_t with task info */
void queue_list_task();

#endif