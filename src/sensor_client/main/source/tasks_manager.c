#include <string.h>

#include "source/tasks_manager.h"

// pointer to tasks_t struct to manage tasks
static tasks_t *task_manager;

/**
 * @brief initialize task_manager
 */
void init_tasks_manager()
{
    task_manager = (tasks_t *)malloc(sizeof(tasks_t));
    task_manager->first = NULL;
    task_manager->last = NULL;
    task_manager->num_tasks = 0;
}

/**
 * @brief free task struct only
 */
static void free_task(task_t *task){
    free(task->name);
}

/**
 * @brief free node struct and task
 */
static void free_node(node_t *node)
{
    free_task(node->task);
    free(node);
}

/**
 * @brief Free all nodes contains within task_manager
 *      - task_manager->first = NULL
 *      - task_manager->last = NULL
 *      - task_manager->num_tasks = 0
 */
void free_tasks_manager()
{
    node_t *first = task_manager->first;
    node_t *aux;

    while(first != NULL){
        aux = first->next;
        free_node(first);
        first = aux;
    }

    task_manager->first = NULL;
    task_manager->last = NULL;
    task_manager->num_tasks = 0;
}

/**
 * @brief compare two task_t
 */
static bool equals(task_t *task_l, task_t *task_r)
{
    bool equals = false;
    if(strcmp(task_l->name, task_r->name) == 0){ //equals, task exists
        equals = true;
    }
    return equals;
}

/**
 * @brief check if a task exists within tasks_manager
 */
status_t task_exists(task_t *new_task)
{
    node_t *aux = task_manager->first;
    status_t status = NOT_EXISTS;
    while(status == NOT_EXISTS && aux != NULL){
        if(equals(aux->task, new_task)){
            status = EXISTS;
        }
        else{
            aux = aux->next;
        }
    }
    return status;
}

/**
 * @brief Add a new tasks
 */
static void add_task(task_t *new_task)
{
    node_t *new_elem = (node_t *)malloc(sizeof(node_t));
    new_elem->task = new_task;
    new_elem->next = NULL;

    if(task_manager->first == NULL){
        task_manager->first = new_elem;
        task_manager->last = new_elem;
    }
    else{
        task_manager->last->next = new_elem;
        task_manager->last = new_elem;
    }

    task_manager->num_tasks++;
}

/**
 * @brief Add a new task and check if exists. If not, the task is added.
 */
status_t add_new_task_if_not_exists(task_t *new_task)
{
    status_t status = EXISTS;
    if(task_exists(new_task) != EXISTS){
        add_task(new_task);
        status = CREATED;
    }
    return status;
}

/**
 * @brief obtain the task's data based on a given name
 */
task_t* obtain_task(char *name)
{
    task_t *task = NULL;
    node_t *aux = task_manager->first;

    task_t task_found = {
        .name = name,
    };

    status_t status = NOT_EXISTS;

    while(status == NOT_EXISTS && aux != NULL){
        if(equals(aux->task, &task_found)){
            status = EXISTS;
            task = aux->task;
        }
        else{
            aux = aux->next;
        }
    }
    return task;
}

/**
 * @brief remove a task from list based on a given name
 */
void remove_task(char *name)
{
    node_t *aux = task_manager->first;
    node_t *before = NULL;

    task_t task = {
        .name = name,
    };

    status_t status = NOT_EXISTS;

    while(status == NOT_EXISTS && aux != NULL){
        if(equals(aux->task, &task)){
            status = EXISTS;
            before->next = aux->next;
            free_node(aux);
        }
        else{
            before = aux;
            aux = aux->next;
        }
    }
}