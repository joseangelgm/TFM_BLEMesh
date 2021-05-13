#include "esp_log.h"
#include "mqtt_client.h"

#include "source/mqtt.h"
#include "source/ble_cmd.h"

static const char *TAG = "MQTT";
//static const char *PUB_TOPIC = "/sensors/results"; // to publish data
static const char *SUB_TOPIC = "/sensors/commands"; // to execute commands

// mqtt client to send messages to PUB_TOPIC
static esp_mqtt_client_handle_t client_mqtt;

// queue to receive json and porse it
static QueueHandle_t queue_receive;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            ESP_LOGW(TAG, "Suscribing to %s", SUB_TOPIC);
            esp_mqtt_client_subscribe(client, SUB_TOPIC, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");

            /*
            // Get topic
            char *topic = malloc(event->topic_len * sizeof(char) + 1);
            memset(topic, '\0', event->topic_len * sizeof(char) + 1);
            strncpy(topic, event->topic, event->topic_len);

            // Get data
            char *data = malloc(event->data_len * sizeof(char) + 1);
            memset(data, '\0', event->data_len * sizeof(char) + 1);
            strncpy(data, event->data, event->data_len);

            ESP_LOGI(TAG, "Topic: %s, data: %s", topic, data);
            */

            mqtt_json json = {
                .json = event->data,
                .size = event->data_len,
            };
            xQueueSendToBack(queue_receive, &json, 0);

            /*
            free(topic);
            free(data);
            */

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(esp_mqtt_client_handle_t client_mqtt)
{
    esp_mqtt_client_register_event(client_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, client_mqtt);
    esp_mqtt_client_start(client_mqtt);
}

esp_err_t init_mqtt()
{

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    queue_receive = xQueueCreate(4, sizeof(mqtt_json));

    // ble cmd task
    xTaskCreate(&task_parse_json, "task_parse_json", 2048, (void *) &queue_receive, 4, NULL);

    client_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    mqtt_app_start(client_mqtt);

    return ESP_OK;
}