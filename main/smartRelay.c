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
#include "wqtt_client.h"


static const char *TAG = "SMART RELAY";

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;

static uint16_t current_value = 0;
static lv_obj_t * table;
static lv_obj_t * wifi_label;

/********************************************************
 *  STATIC PROTOTYPES
 ********************************************************/

static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

static void update_current_value(void *arg);
static uint32_t current = 0;


/*******************************************************
 *   LVGL CONTROLS HANDLING
 *******************************************************/

// FAN CONTROLS

static lv_obj_t * spinbox;

static void lv_spinbox_increment_event_cb(lv_obj_t * btn, lv_event_t e)
{
    if(e == LV_EVENT_PRESSED ) {
        lv_spinbox_increment(spinbox);
    }
}

static void lv_spinbox_decrement_event_cb(lv_obj_t * btn, lv_event_t e)
{
    if(e == LV_EVENT_PRESSED ) {
        lv_spinbox_decrement(spinbox);
    }
}


// LIGHT SWITCH CONTROLS

static void sw_event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        printf("State: %s\n", lv_switch_get_state(obj) ? "On" : "Off");
    }
}


/**
 * Create a button with a label and react on click event.
 */
static void create_demo_application(void)
{

    // TABLE OF VALUES

    table = lv_table_create(lv_scr_act(), NULL);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, 2);
    lv_obj_align(table, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

    // Align the price values to the right in the 2nd column
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_RIGHT);
    lv_table_set_cell_align(table, 1, 1, LV_LABEL_ALIGN_RIGHT);

    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);


    // Fill the first column
    lv_table_set_cell_value(table, 0, 0, "Light");
    lv_table_set_cell_value(table, 1, 0, "Current");

    //Fill the second column
    lv_table_set_cell_value(table, 0, 1, "OFF");
    lv_table_set_cell_value(table, 1, 1, "1A");

    lv_table_ext_t * ext = lv_obj_get_ext_attr(table);
    ext->row_h[0] = 20;


    // FAN REGULATOR

    spinbox = lv_spinbox_create(lv_scr_act(), NULL);
    lv_spinbox_set_range(spinbox, 1, 5);
    lv_spinbox_set_digit_format(spinbox, 1, 0);
    lv_spinbox_step_prev(spinbox);
    lv_obj_set_width(spinbox, 50);
    lv_obj_align(spinbox, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_coord_t h = lv_obj_get_height(spinbox);
    lv_obj_t * btn = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_size(btn, h, h);
    lv_obj_align(btn, spinbox, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_theme_apply(btn, LV_THEME_SPINBOX_BTN);
    lv_obj_set_style_local_value_str(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_SYMBOL_PLUS);
    lv_obj_set_event_cb(btn, lv_spinbox_increment_event_cb);

    btn = lv_btn_create(lv_scr_act(), btn);
    lv_obj_align(btn, spinbox, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_set_event_cb(btn, lv_spinbox_decrement_event_cb);
    lv_obj_set_style_local_value_str(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_SYMBOL_MINUS);

    // LABEL FOR WIFI

    wifi_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_align(wifi_label, LV_ALIGN_IN_TOP_LEFT);       /*Center aligned lines*/
    lv_label_set_text(wifi_label, wifi_get_ip() );
    lv_obj_set_width(wifi_label, 150);

}

/********************************************
 GUI TASK
*********************************************/


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
        vTaskDelay(pdMS_TO_TICKS(20));

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
        if(hw_ctrl_get_lamp3_state() == HW_OFF )
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

    ESP_LOGI(TAG, "Connecting to WiFi..");

    while( wifi_get_ip()[0] == 'N')
    {
        vTaskDelay(pdMS_TO_TICKS(10));        
    }

    ESP_LOGI(TAG, "IP address=%s", wifi_get_ip() );

    hw_ctrl_start();

    wqtt_client_start();
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
