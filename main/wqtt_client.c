
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_client.h"

#include "wqtt_client.h"
#include "hw_ctrl.h"
#include "smartRelay.h"



/**********************
 *  FUNCTION PROTOTYPES
 **********************/


/**********************
 *  VARIABLES
 **********************/

static esp_mqtt_client_handle_t client;
static const char *TAG = "WQTT";


static hw_state_t       Heater_state = HW_OFF;
static hw_state_t       Light_state = HW_OFF;
static hw_state_t       LED_state = HW_OFF;
static uint32_t         Current_value = 0;
static hw_electr_lvl_t  Fan_speed = HW_LVL_OFF;

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
 * @brief Detects if the topic name coming from the event corresponds to the sample topic name
 * 
 * @param event_topic_name 
 * @param topic_len 
 * @param topic_name_to_compare_with 
 * @return true     topic names match
 * @return false    if topic names did not match
 */
static bool is_topic_equals(char* event_topic_name, int topic_len, char* topic_name_to_compare_with)
{
    for(int idx = 0; idx < topic_len; ++idx)
    {
        if(event_topic_name[idx] != topic_name_to_compare_with[idx])
        {
            return false;
        }
    }

    return true;
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
    hw_electr_lvl_t fan_value;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        msg_id = esp_mqtt_client_subscribe(client, LED_topic, 0);
        ESP_LOGI(TAG, LED_topic " subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, Heater_topic, 0);
        ESP_LOGI(TAG, Heater_topic " subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, Fan_topic, 0);
        ESP_LOGI(TAG, Fan_topic " subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, Light_topic, 0);
        ESP_LOGI(TAG, Light_topic " subscribe successful, msg_id=%d", msg_id);

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
        printf("TOPIC=%.*s  ", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);


        if(is_topic_equals(event->topic, event->topic_len, Heater_topic) == true )
        {
            if(event->data[0] == '1')
            {
                ESP_LOGI(TAG, "Heater ON");
                // HW
                hw_ctrl_set_Load1_state(HW_ON);

                // UI
                ui_set_heater_state(HW_ON);
            } 
            else if (event->data[0] == '0')
            {
                ESP_LOGI(TAG, "Heater Off");
                // HW
                hw_ctrl_set_Load1_state(HW_OFF);

                // UI
                ui_set_heater_state(HW_OFF);
            }
        } 
        else if (is_topic_equals(event->topic, event->topic_len, Fan_topic) == true)
        {
            fan_value = (hw_electr_lvl_t)(event->data[0] - '0');

            // HW
            hw_ctrl_set_Load2_level(fan_value);

            // UI
            ui_set_fan_speed(fan_value);

        }
        else if (is_topic_equals(event->topic, event->topic_len, Light_topic) == true)
        {
            if(event->data[0] == '1')
            {
                ESP_LOGI(TAG, "Light ON");

                // HW
                hw_ctrl_set_Load3_state(HW_ON);

                // UI
                ui_set_light_state(HW_ON);
            } 
            else if (event->data[0] == '0')
            {
                ESP_LOGI(TAG, "Light OFF");
                // HW
                hw_ctrl_set_Load3_state(HW_OFF);

                // UI
                ui_set_light_state(HW_OFF);
            }
        }
        else if (is_topic_equals(event->topic, event->topic_len, LED_topic) == true)
        {
            //HW
            if(event->data[0] == '1')
            {
                ESP_LOGI(TAG, "LED ON");
                hw_ctrl_set_LED_state(HW_ON);
            } 
            else if (event->data[0] == '0')
            {
                ESP_LOGI(TAG, "LED OFF");

                hw_ctrl_set_LED_state(HW_OFF);
            }

            //No UI part
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
/**************************************************
 * GET / SET FUNCTIONS
 **************************************************/

/**
 * @brief   Sets the Current value for WQTT cloud
 * 
 * @param current Value of the current in mA
 */
void wqtt_client_set_current( uint32_t current )
{
    int msg_id;
    char str[32];

    sprintf( str, "%d", current );

    msg_id = esp_mqtt_client_publish(client, Current_topic, str, 0, 1, 0);
    ESP_LOGI(TAG, Current_topic " publish successful, msg_id=%d", msg_id);
}


/**
 * @brief   Sets the Fan speed level
 * 
 * @param   HW_ON, HW_OFF
 */
void wqtt_client_set_Fan_level(hw_electr_lvl_t level)
{
    int msg_id;
    char param[] = { ' ', '\0'};

    Fan_speed = level;
    param[0] = level + '0';

    msg_id = esp_mqtt_client_publish(client, Fan_topic, param, 0, 1, 0);
    ESP_LOGI(TAG, Fan_topic " publish successful, msg_id=%d", msg_id);
}


/**
 * @brief   Gets the level of Fan speed
 * 
 * @return  HW_LVL_OFF .. HW_LVL_VERY_HIGH 
 */
hw_electr_lvl_t wqtt_client_get_Fan_level(void)
{
    return Fan_speed;
}

/**
 * @brief   Sets Heater state
 * 
 * @param   HW_ON, WH_OFF
 */
void wqtt_client_set_Heater_state(hw_state_t state)
{
    int msg_id;
    char param[] = { ' ', '\0'};

    Heater_state = state;
    param[0] = state + '0';

    msg_id = esp_mqtt_client_publish(client, Heater_topic, param, 0, 1, 0);
    ESP_LOGI(TAG, Heater_topic " publish successful, msg_id=%d", msg_id);
}

/**
 * @brief   Gets the Heater state
 * 
 * @return  HW_ON, HW_OFF
 */
hw_state_t  wqtt_client_get_Heater_state(void)
{
    return Heater_state;
}

/**
 * @brief   Sets the Light state
 * 
 * @param   HW_ON, HW_OFF
 */
void wqtt_client_set_Light_state(hw_state_t state)
{
    int msg_id;
    char param[] = { ' ', '\0'};

    Light_state = state;
    param[0] = state + '0';

    msg_id = esp_mqtt_client_publish(client, Light_topic, param, 0, 1, 0);
    ESP_LOGI(TAG, Light_topic " publish successful, msg_id=%d", msg_id);
}

/**
 * @brief   Gets Light state
 * 
 * @return  HW_ON, HW_OFF 
 */
hw_state_t wqtt_client_get_Light_state(void)
{
    return Light_state;
}

/**
 * @brief   Gets the Current value in mA
 * 
 * @return  Current value in mA
 */
uint32_t wqtt_client_get_Current(void)
{
    return Current_value;
}

/**
 * @brief   Sets new state for LED
 * 
 * @param   WH_ON, HW_OFF
 */
void wqtt_client_set_LED_state(hw_state_t LED_new_state)
{
    int msg_id;
    char param[] = { ' ', '\0'};

    LED_state = LED_new_state;
    param[0] = LED_new_state + '0';

    msg_id = esp_mqtt_client_publish(client, LED_topic, param, 0, 1, 0);
    ESP_LOGI(TAG, LED_topic " publish successful, msg_id=%d", msg_id);
}

/**
 * @brief   Gets LED state
 * 
 * @return HW_ON, HW_OFF
 */
hw_state_t  wqtt_client_get_LED_state(void)
{
    return LED_state;
}
