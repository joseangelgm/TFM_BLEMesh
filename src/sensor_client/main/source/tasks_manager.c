#include <string.h>
#include "esp_log.h"

#include "source/tasks_manager.h"
#include "source/messages_parser.h"

static const char* TAG = "TaskManager";

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
 * @brief Free all nodes within task_manager
 *      - task_manager->first = NULL
 *      - task_manager->last = NULL
 *      - task_manager->num_tasks = 0
 */
void free_tasks_manager()
{
    for(node_t* temp = task_manager->first->next; temp != NULL; temp = temp->next)
    {
        free_node(temp->prev);
    }

    free_node(task_manager->last);

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
    if(task_l != NULL && task_r != NULL
       && strcmp(task_l->name, task_r->name) == 0)
    {
        equals = true;
    }
    return equals;
}

/**
 * @brief check if a task exists within tasks_manager
 */
status_t task_exists(task_t *new_task)
{
    status_t status = NOT_EXISTS;

    if(task_manager->first != NULL && task_manager->last != NULL)
    {

        if(equals(task_manager->first->task, new_task))
        {
            status = EXISTS;
        }
        else if(equals(task_manager->last->task, new_task))
        {
            status = EXISTS;
        }
        else if(task_manager->first != task_manager->last)
        {
            for(node_t *temp = task_manager->first->next;
                temp != task_manager->last && status == NOT_EXISTS;
                temp = temp->next)
            {
                if(equals(temp->task, new_task))
                {
                    status = EXISTS;
                }
            }
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
    new_elem->prev = NULL;

    if(task_manager->first == NULL)
    {
        task_manager->first = new_elem;
        task_manager->last = new_elem;
    }
    else
    {
        node_t *aux = task_manager->last;
        task_manager->last = new_elem;
        aux->next = task_manager->last;
        task_manager->last->prev = aux;
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
task_t* obtain_task(task_t* task)
{

    if(task_manager->first != NULL && task_manager->last != NULL)
    {
        if(equals(task_manager->first->task, task))
            return task_manager->first->task;

        if(equals(task_manager->last->task, task))
            return task_manager->last->task;

        task_t *found_task = NULL;
        status_t status = NOT_EXISTS;

        for(node_t *temp = task_manager->first->next;
            temp != task_manager->last->prev && status == NOT_EXISTS;
            temp = temp->next)
        {
            if(equals(temp->task, task))
            {
                status = EXISTS;
                found_task = temp->task;
            }
        }
        return found_task;
    }
    return NULL;
}

/**
 * @brief remove a task from list based on a given name
 */
status_t remove_task(task_t* remove_task)
{
    status_t status = NOT_EXISTS;

    // if it is the first element, remove and point to the next element
    if(task_manager->first != NULL && equals(task_manager->first->task, remove_task))
    {
        if(task_manager->first == task_manager->last)// if we have one element
        {
            free_node(task_manager->first);
        }
        else
        {
            task_manager->first = task_manager->first->next;
            free_node(task_manager->first->prev);
        }
        status = EXISTS;
        task_manager->num_tasks--;
    }
     // if it is the last element, remove and point to the previous element
    else if(task_manager->last != NULL && equals(task_manager->last->task, remove_task))
    {
        if(task_manager->first == task_manager->last)
        {
            free_node(task_manager->first);
        }
        else
        {
            task_manager->last = task_manager->last->prev;
            free_node(task_manager->last->next);
        }
        status = EXISTS;
        task_manager->num_tasks--;
    }
    else
    {
        for(node_t* temp = task_manager->first->next; temp != task_manager->last->prev && status == NOT_EXISTS; temp = temp->next)
        {
            if(equals(temp->task, remove_task))
            {
                status = EXISTS;
                temp->prev->next = temp->next;
                temp->next->prev = temp->prev;
                free_node(temp);
                task_manager->num_tasks--;
            }
        }
    }
    return status;
}

/**
 * @brief Queue a message_t with task info
 */
void queue_list_task()
{
    message_t* tasks_info = create_message_text_plain();
    char buff[MAX_LENGHT_MESSAGE];

    if(task_manager->num_tasks > 0)
    {
        for(node_t *temp = task_manager->first; temp != NULL; temp = temp->next)
        {
            memset(buff,'\0',MAX_LENGHT_MESSAGE);
            sprintf(buff, "Task: Name -> %s", temp->task->name);
            add_message_text_plain(&tasks_info->m_content.text_plain, buff);
        }
    }
    else
    {
        memset(buff,'\0',MAX_LENGHT_MESSAGE);
        sprintf(buff, "No tasks!!");
        add_message_text_plain(&tasks_info->m_content.text_plain, buff);
    }

    send_message_queue(tasks_info);
}