#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "ble_mesh_example_init.h"
#include "source/messages_parser.h"

/*
FLUJO:

- discover-unprovisioned on <segundos>
- provision <UUID>

. menu config
    - target <addr>
    - netkey-add <index netkey>
    - appkey-add <index appkey>
    - bind <addr> <index appkey> <model id>

- subscripcion (grupo)
    - sub-add <addr> <dir group> <model id>

Datos interesantes

    Dato unico que determina que modelo estamos usando. Hay uno para el cliente y otro para el servidor:
        - model_id para el cliente  (modelo sensor):
            . sensor client = 1102
        - model_id para el servidor (modelo sensor):
            . sensor server = 1100
            . sensor setup server = 1101



hb-pub-set 00ff 5 5 5 2 0
*/

static const char* TAG = "SensorClient";

#define CID_ESP             0x02E5

#define MSG_SEND_TTL        3
#define MSG_SEND_REL        false
#define MSG_TIMEOUT         0
#define MSG_ROLE            ROLE_NODE

static uint8_t dev_uuid[ESP_BLE_MESH_OCTET16_LEN] = { 0x00, 0x11 };

static struct esp_ble_mesh_key {
    uint16_t net_idx;
    uint16_t app_idx;
    uint8_t  app_key[ESP_BLE_MESH_OCTET16_LEN];
} prov_key;

static esp_ble_mesh_cfg_srv_t config_server = {
    .beacon = ESP_BLE_MESH_BEACON_DISABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

static esp_ble_mesh_client_t config_client;
static esp_ble_mesh_client_t sensor_client;

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
    ESP_BLE_MESH_MODEL_SENSOR_CLI(NULL, &sensor_client),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid
};

static void ble_mesh_set_msg_common(esp_ble_mesh_client_common_param_t *common,
                                esp_ble_mesh_model_t *model, uint32_t opcode, uint16_t addr)
{
    common->opcode = opcode;
    common->model = model;
    common->ctx.net_idx = prov_key.net_idx;
    common->ctx.app_idx = prov_key.app_idx;
    common->ctx.addr = addr;
    common->ctx.send_ttl = MSG_SEND_TTL;
    common->ctx.send_rel = MSG_SEND_REL;
    common->msg_timeout = MSG_TIMEOUT;
    common->msg_role = MSG_ROLE;
}

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08x", flags, iv_index);
    prov_key.net_idx = net_idx;
}

static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
        case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
            break;
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
            prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                param->node_prov_complete.flags, param->node_prov_complete.iv_index);
            break;
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
            break;
        case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
            break;
        default:
            break;
    }
}

void ble_mesh_send_sensor_message(uint32_t opcode, uint16_t addr, uint16_t sensor_prop_id)
{

    ESP_LOGI(TAG, "ble_mesh_send_sensor_message: 0x%04x. Addr = 0x%04x", opcode, addr);

    esp_ble_mesh_sensor_client_get_state_t get = {0};
    esp_ble_mesh_client_common_param_t common = {0};
    esp_err_t err = ESP_OK;

    ble_mesh_set_msg_common(&common, sensor_client.model, opcode, addr);
    switch (opcode) {
    case ESP_BLE_MESH_MODEL_OP_SENSOR_GET:
        if(sensor_prop_id != 0x0000)
        {
            // indicate that we send optional parameters. In this case is sensor_prop_id
            get.sensor_get.property_id = sensor_prop_id;
            get.sensor_get.op_en = true;
        }
        else
        {
            get.sensor_get.op_en = false;
        }
        break;
    case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
        if(sensor_prop_id != 0x0000)
        {
            get.descriptor_get.property_id = sensor_prop_id;
            get.descriptor_get.op_en = true;
        }
        else
        {
            get.descriptor_get.op_en = false;
        }
        break;
    default:
        break;
    }

    err = esp_ble_mesh_sensor_client_get_state(&common, &get);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send sensor message 0x%04x", opcode);
    }
}

static void publish_measure(esp_ble_mesh_sensor_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "Sensor Status, opcode 0x%04x", param->params->ctx.recv_op);

    if (param->status_cb.sensor_status.marshalled_sensor_data->len)
    {
        ESP_LOG_BUFFER_HEX("Sensor Data", param->status_cb.sensor_status.marshalled_sensor_data->data,
            param->status_cb.sensor_status.marshalled_sensor_data->len);
        uint8_t *data = param->status_cb.sensor_status.marshalled_sensor_data->data;
        uint16_t length = 0;

        if(data[0] == 0xFF) // prop id doesnt exists. Error.
        {
            uint8_t fmt      = ESP_BLE_MESH_GET_SENSOR_DATA_FORMAT(data);
            uint16_t prop_id = ESP_BLE_MESH_GET_SENSOR_DATA_PROPERTY_ID(data, fmt);

            message_t* message = create_message(ERROR);
            add_message_text_plain(message, "Sensor prop id 0x%04x doesnt exists in sensor addr 0x%04x", prop_id, param->params->ctx.addr);
            send_message_queue(message);

        }
        else
        {
            for (; length < param->status_cb.sensor_status.marshalled_sensor_data->len; )
            {
                uint8_t fmt      = ESP_BLE_MESH_GET_SENSOR_DATA_FORMAT(data);
                uint8_t data_len = ESP_BLE_MESH_GET_SENSOR_DATA_LENGTH(data, fmt);
                uint16_t prop_id = ESP_BLE_MESH_GET_SENSOR_DATA_PROPERTY_ID(data, fmt);
                uint8_t mpid_len = (fmt == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A ?
                                    ESP_BLE_MESH_SENSOR_DATA_FORMAT_A_MPID_LEN : ESP_BLE_MESH_SENSOR_DATA_FORMAT_B_MPID_LEN);

                ESP_LOGI(TAG, "Format %s, length 0x%02x, Sensor Property ID 0x%04x",
                    fmt == ESP_BLE_MESH_SENSOR_DATA_FORMAT_A ? "A" : "B", data_len, prop_id);

                if (data_len != ESP_BLE_MESH_SENSOR_DATA_ZERO_LEN)
                {
                    ESP_LOG_BUFFER_HEX("Sensor Data", data + mpid_len, data_len + 1);

                    int measure = *(data + mpid_len);
                    ESP_LOGW(TAG, "Measure %d", measure);

                    message_t* message = create_message(GET_STATUS);
                    add_measure_to_message(message, param->params->ctx.addr, prop_id, measure);
                    send_message_queue(message);

                    length += mpid_len + data_len + 1;
                    data += mpid_len + data_len + 1;
                }
                else
                {
                    length += mpid_len;
                    data += mpid_len;
                }
            }
        }
    }
}

static void ble_mesh_sensor_client_cb(esp_ble_mesh_sensor_client_cb_event_t event,
                                              esp_ble_mesh_sensor_client_cb_param_t *param)
{

    ESP_LOGI(TAG, "ble_mesh_sensor_client_cb");
    ESP_LOGI(TAG, "Sensor client, event %u, addr 0x%04x", event, param->params->ctx.addr);

    if (param->error_code) {
        ESP_LOGE(TAG, "Send sensor client message failed (err %d)", param->error_code);
        return;
    }

    message_t* messages = NULL; // Variable to store received info

    switch (event) {
    case ESP_BLE_MESH_SENSOR_CLIENT_GET_STATE_EVT:
        switch (param->params->opcode) {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
            ESP_LOGI(TAG, "Sensor Descriptor Status, opcode 0x%04x", param->params->ctx.recv_op);
            if (param->status_cb.descriptor_status.descriptor->len != ESP_BLE_MESH_SENSOR_SETTING_PROPERTY_ID_LEN &&
                param->status_cb.descriptor_status.descriptor->len % ESP_BLE_MESH_SENSOR_DESCRIPTOR_LEN) {
                ESP_LOGE(TAG, "Invalid Sensor Descriptor Status length %d", param->status_cb.descriptor_status.descriptor->len);
                return;
            }

            if (param->status_cb.descriptor_status.descriptor->len)
            {
                if(param->status_cb.descriptor_status.descriptor->len == 8)
                {
                    ESP_LOG_BUFFER_HEX("Sensor Descriptor", param->status_cb.descriptor_status.descriptor->data,
                    param->status_cb.descriptor_status.descriptor->len);

                    messages = create_message(GET_DESCRIPTOR);
                    add_hex_buffer(messages,
                            param->status_cb.descriptor_status.descriptor->data,
                            param->status_cb.descriptor_status.descriptor->len
                    );
                    send_message_queue(messages);
                }
                else
                {
                    messages = create_message(ERROR);
                    add_message_text_plain(messages, "Incorrect sensor prop id for device 0x%04x", param->params->ctx.addr);
                    send_message_queue(messages);
                }

            }
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_GET:
            ESP_LOGI(TAG, "Sensor Cadence Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.cadence_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Cadence", param->status_cb.cadence_status.sensor_cadence_value->data,
                param->status_cb.cadence_status.sensor_cadence_value->len);

                messages = create_message(HEX_BUFFER);
                add_hex_buffer(messages,
                        param->status_cb.cadence_status.sensor_cadence_value->data,
                        param->status_cb.cadence_status.sensor_cadence_value->len
                );
                send_message_queue(messages);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTINGS_GET:
            ESP_LOGI(TAG, "Sensor Settings Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.settings_status.sensor_property_id);
            ESP_LOG_BUFFER_HEX("Sensor Settings", param->status_cb.settings_status.sensor_setting_property_ids->data,
                param->status_cb.settings_status.sensor_setting_property_ids->len);

                messages = create_message(HEX_BUFFER);
                add_hex_buffer(messages,
                        param->status_cb.settings_status.sensor_setting_property_ids->data,
                        param->status_cb.settings_status.sensor_setting_property_ids->len
                );
                send_message_queue(messages);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_GET:
            ESP_LOGI(TAG, "Sensor Setting Status, opcode 0x%04x, Sensor Property ID 0x%04x, Sensor Setting Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.setting_status.sensor_property_id,
                param->status_cb.setting_status.sensor_setting_property_id);
            if (param->status_cb.setting_status.op_en) {
                ESP_LOGI(TAG, "Sensor Setting Access 0x%02x", param->status_cb.setting_status.sensor_setting_access);
                ESP_LOG_BUFFER_HEX("Sensor Setting Raw", param->status_cb.setting_status.sensor_setting_raw->data,
                    param->status_cb.setting_status.sensor_setting_raw->len);

                messages = create_message(HEX_BUFFER);
                add_hex_buffer(messages,
                        param->status_cb.setting_status.sensor_setting_raw->data,
                        param->status_cb.setting_status.sensor_setting_raw->len
                );
                send_message_queue(messages);
            }
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_GET: /* Read temperature */
                publish_measure(param);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_COLUMN_GET:
            ESP_LOGI(TAG, "Sensor Column Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.column_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Column", param->status_cb.column_status.sensor_column_value->data,
                param->status_cb.column_status.sensor_column_value->len);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SERIES_GET:
            ESP_LOGI(TAG, "Sensor Series Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.series_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Series", param->status_cb.series_status.sensor_series_value->data,
                param->status_cb.series_status.sensor_series_value->len);
            break;
        default:
            ESP_LOGE(TAG, "Unknown Sensor Get opcode 0x%04x", param->params->ctx.recv_op);
            break;
        }
        break;
    case ESP_BLE_MESH_SENSOR_CLIENT_SET_STATE_EVT:
        switch (param->params->opcode) {
        case ESP_BLE_MESH_MODEL_OP_SENSOR_CADENCE_SET:
            ESP_LOGI(TAG, "Sensor Cadence Status, opcode 0x%04x, Sensor Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.cadence_status.property_id);
            ESP_LOG_BUFFER_HEX("Sensor Cadence", param->status_cb.cadence_status.sensor_cadence_value->data,
                param->status_cb.cadence_status.sensor_cadence_value->len);
            break;
        case ESP_BLE_MESH_MODEL_OP_SENSOR_SETTING_SET:
            ESP_LOGI(TAG, "Sensor Setting Status, opcode 0x%04x, Sensor Property ID 0x%04x, Sensor Setting Property ID 0x%04x",
                param->params->ctx.recv_op, param->status_cb.setting_status.sensor_property_id,
                param->status_cb.setting_status.sensor_setting_property_id);
            if (param->status_cb.setting_status.op_en) {
                ESP_LOGI(TAG, "Sensor Setting Access 0x%02x", param->status_cb.setting_status.sensor_setting_access);
                ESP_LOG_BUFFER_HEX("Sensor Setting Raw", param->status_cb.setting_status.sensor_setting_raw->data,
                    param->status_cb.setting_status.sensor_setting_raw->len);
            }
            break;
        default:
            ESP_LOGE(TAG, "Unknown Sensor Set opcode 0x%04x", param->params->ctx.recv_op);
            break;
        }
        break;
    /*
        Si los datos se piden a una direccion de grupo (suscripcion)
        Los grupos en realidad es una suscripcion por lo que entrada por
        este evento.
    */
    case ESP_BLE_MESH_SENSOR_CLIENT_PUBLISH_EVT:
        ESP_LOGI(TAG, "Receive message from group. opcode 0x%04x", param->params->opcode);
        switch(param->params->opcode)
        {
            case ESP_BLE_MESH_MODEL_OP_SENSOR_STATUS:
                publish_measure(param);
                break;
            case ESP_BLE_MESH_MODEL_OP_SENSOR_DESCRIPTOR_GET:
                if(param->status_cb.descriptor_status.descriptor->len == 8)
                {
                    ESP_LOG_BUFFER_HEX("Sensor Descriptor", param->status_cb.descriptor_status.descriptor->data,
                    param->status_cb.descriptor_status.descriptor->len);

                    messages = create_message(GET_DESCRIPTOR);
                    add_hex_buffer(messages,
                            param->status_cb.descriptor_status.descriptor->data,
                            param->status_cb.descriptor_status.descriptor->len
                    );
                    send_message_queue(messages);
                }
                else
                {
                    messages = create_message(ERROR);
                    add_message_text_plain(messages, "Incorrect sensor prop id for device 0x%04x", param->params->ctx.addr);
                    send_message_queue(messages);
                }
                break;
        }
        break;
    case ESP_BLE_MESH_SENSOR_CLIENT_TIMEOUT_EVT:
        ESP_LOGI(TAG, "Timeout: opcode 0x%04x, destination 0x%04x", param->params->opcode, param->params->ctx.addr);

        messages = create_message(TIMEOUT);
        add_message_text_plain(messages,
            "Timeout: opcode 0x%04x, destination 0x%04x",
            param->params->opcode, param->params->ctx.addr
        );
        send_message_queue(messages);
    default:
        break;
    }
}

static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                      esp_ble_mesh_cfg_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "ble_mesh_config_server_cb: event = %d", event);
    if(event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT){
        switch(param->ctx.recv_op){
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(TAG, "Receiving app key after provisioning");
            ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                param->value.state_change.appkey_add.net_idx,
                param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey received -> ", param->value.state_change.appkey_add.app_key, 16);
            break;
        /*
           Una vez provisionado, tenemos que asociar una clave a un modelo que implemente el dispositivo para que sepa
           que app key debe usar con que modelo ya que puede tener varios
        */
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                param->value.state_change.mod_app_bind.element_addr,
                param->value.state_change.mod_app_bind.app_idx,
                param->value.state_change.mod_app_bind.company_id,
                param->value.state_change.mod_app_bind.model_id);
            if(param->value.state_change.mod_app_bind.company_id == 0xFFFF &&
               param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_MODEL_ID_SENSOR_CLI){
                // Guardo app key
                prov_key.app_idx = param->value.state_change.mod_app_bind.app_idx;
               }
            break;
        }

    }
}

esp_err_t ble_mesh_init(void)
{

    esp_err_t err = ESP_OK;

    err = bluetooth_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return err;
    }

    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_sensor_client_callback(ble_mesh_sensor_client_cb);
    esp_ble_mesh_register_config_server_callback(ble_mesh_config_server_cb);

    ble_mesh_get_dev_uuid(dev_uuid);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack");
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh sensor client initialized");

    return ESP_OK;
}