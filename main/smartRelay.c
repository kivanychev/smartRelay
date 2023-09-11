/*
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_system.h"

#include "nvs_flash.h"
#include "mqtt_client.h"


#include "lvgl.h"
#include "lvgl_helpers.h"
#include "wifi.h"
#include "hw_ctrl.h"

#define LAMP_OFF    0
#define LAMP_ON     1

#define LED4_GPIO   27
#define LV_TICK_PERIOD_MS 1


static const char *TAG = "SMART RELAY";
static int lamp3_state = LAMP_OFF;

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static uint16_t current_value = 0;
static lv_obj_t * table;
static lv_obj_t * wifi_label;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

static void update_current_value(void *arg);
static uint32_t current = 0;

/**********************
 *   MQTT - WQTT
 **********************/

static esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
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
            gpio_set_level(LED4_GPIO, 0);
            lamp3_state = LAMP_ON;
        } else if (event->data[0] == '0')
        {
            gpio_set_level(LED4_GPIO, 1);
            lamp3_state = LAMP_OFF;
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

static void mqtt_app_start(void)
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

    /* Create and start a periodic timer interrupt to call update_current_value */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &update_current_value,
        .name = "periodic_current"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000000));

}


/**********************
 *   LVGL FUNCTIONS
 **********************/


static void btn_event_handler(lv_obj_t * obj, lv_event_t event)
{
    char str[32];

    if(event == LV_EVENT_CLICKED) {
        printf("Clicked\n");
    }
    else if(event == LV_EVENT_VALUE_CHANGED) {
        printf("Toggled\n");
    }

    sprintf(str, "%d", current_value);
    lv_table_set_cell_value(table, 1, 1, str);

    current_value++;
}



static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
    }
}


/**
 * Create a button with a label and react on click event.
 */
static void create_demo_application(void)
{
    table = lv_table_create(lv_scr_act(), NULL);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, 3);
    lv_obj_align(table, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

    /*Align the price values to the right in the 2nd column*/
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_RIGHT);
    lv_table_set_cell_align(table, 1, 1, LV_LABEL_ALIGN_RIGHT);
    lv_table_set_cell_align(table, 2, 1, LV_LABEL_ALIGN_RIGHT);

    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);


    /*Fill the first column*/
    lv_table_set_cell_value(table, 0, 0, "Light");
    lv_table_set_cell_value(table, 1, 0, "Current");
    lv_table_set_cell_value(table, 2, 0, "Fan state");

    /*Fill the second column*/
    lv_table_set_cell_value(table, 0, 1, "OFF");
    lv_table_set_cell_value(table, 1, 1, "1A");
    lv_table_set_cell_value(table, 2, 1, "OFF");

    lv_table_ext_t * ext = lv_obj_get_ext_attr(table);
    ext->row_h[0] = 20;



    // BUTTONS

    lv_obj_t * label;

    lv_obj_t * btn1 = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(btn1, btn_event_handler);
    lv_obj_align(btn1, NULL, LV_ALIGN_CENTER, 0, 30);

    label = lv_label_create(btn1, NULL);
    lv_label_set_text(label, "Light off");

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(btn2, btn_event_handler);
    lv_obj_align(btn2, NULL, LV_ALIGN_CENTER, 0, 90);
    lv_btn_set_checkable(btn2, true);
    lv_btn_toggle(btn2);
    lv_btn_set_fit2(btn2, LV_FIT_NONE, LV_FIT_TIGHT);

    label = lv_label_create(btn2, NULL);
    lv_label_set_text(label, "Light on");


    // LABEL FOR WIFI

    wifi_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_align(wifi_label, LV_ALIGN_IN_TOP_LEFT);       /*Center aligned lines*/
    lv_label_set_text(wifi_label, wifi_get_ip() );
    lv_obj_set_width(wifi_label, 150);

}



static void guiTask(void *pvParameter) 
{
    char str[32];
    esp_err_t ret = ESP_OK;

    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;


    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;


    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);
#endif

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_demo_application();

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
        
            xSemaphoreGive(xGuiSemaphore);
       }

        // Update Current value on the screen
        current = hw_ctrl_get_Current();
        sprintf(str, "%d", current);
        lv_table_set_cell_value(table, 1, 1, str);

        /* Update Wifi connection IP address */
        lv_label_set_text(wifi_label, wifi_get_ip() );

        // Lamp3 state display
        if(lamp3_state == LAMP_OFF )
        {
            lv_table_set_cell_value(table, 0, 1, "OFF");
        } else {
            lv_table_set_cell_value(table, 0, 1, "ON");
        }

    }

    /* A task should NEVER return */
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    free(buf2);
#endif
    vTaskDelete(NULL);
}



/**********************
 *   APPLICATION MAIN
 **********************/


void app_main(void)
{
    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);

    wifi_start();

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    // Configure LED4 pin for output
    gpio_reset_pin(LED4_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);
    // OFF the light
    gpio_set_level(LED4_GPIO, 1);

    ESP_LOGI(TAG, "Connecting to WiFi..");

    while( wifi_get_ip()[0] == 'N')
    {
        vTaskDelay(pdMS_TO_TICKS(10));        
    }

    ESP_LOGI(TAG, "IP address=%s", wifi_get_ip() );

    hw_ctrl_start();

    mqtt_app_start();
}



/**
 * @brief Calls lv_tick_inc() periodically for getting LVGL to work
 * 
 * @param arg Arguments
 */
static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}


/**
 * @brief Updates the value of Current on the TFT diaplay
 * 
 * @param arg Arguments
 */
static void update_current_value(void *arg)
{
    int msg_id;
    char str[32];

    sprintf( str, "%d", current );

    msg_id = esp_mqtt_client_publish(client, "Current", str, 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}