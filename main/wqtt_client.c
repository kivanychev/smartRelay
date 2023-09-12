
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_client.h"

#include "wqtt_client.h"
#include "hw_ctrl.h"



/**********************
 *  FUNCTION PROTOTYPES
 **********************/


/**********************
 *  VARIABLES
 **********************/

static esp_mqtt_client_handle_t client;
static const char *TAG = "WQTT";


/***********************
 *  FUNCTION DEFINITIONS
 ***********************/

/**
 * The function logs an error message if the error code is non-zero.
 * 
 * @param message The message parameter is a string that describes the error or the context in which
 * the error occurred. It is used to provide additional information about the error in the log message.
 * @param error_code The error code that needs to be checked.
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");


        msg_id = esp_mqtt_client_subscribe(client, "Lamp3", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
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
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        if(event->data[0] == '1')
        {
            hw_ctrl_set_lamp3_state(HW_ON);
        } else if (event->data[0] == '0')
        {
            hw_ctrl_set_lamp3_state(HW_OFF);
        }

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}



/**
 * Initializes and starts an MQTT client, registers an event handler, and
 * creates a periodic timer to call the `update_current_value` function.
 */
void wqtt_client_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://m3.wqtt.ru",
        .username = "u_BFZH1K",
        .password = "3vGW4o04",
        .port = 8817
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

}

/**
 * @brief Updates the Current value for WQTT cloud
 * 
 * @param Current Value of the current in mA
 */
void wqtt_client_set_current( uint32_t current )
{
    int msg_id;
    char str[32];

    sprintf( str, "%d", current );

    msg_id = esp_mqtt_client_publish(client, "Current", str, 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}
