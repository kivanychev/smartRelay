#include "esp_log.h"
#include "esp_adc_cal.h"
#include "esp_system.h"

#include "driver/adc.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hw_ctrl.h"
#include "wqtt_client.h"


/*******************************************************
 CONSTANTS
 *******************************************************/

#define CURRENT_COEFF           1 / 10
#define ADC1_CURRENT_CHANNEL    ADC1_CHANNEL_4

//ADC Attenuation
#define ADC_EXAMPLE_ATTEN       ADC_ATTEN_DB_11

//ADC Calibration
#define ADC_EXAMPLE_CALI_SCHEME ESP_ADC_CAL_VAL_EFUSE_VREF

// Pin assignments
#define LED4_GPIO               27
#define LOAD1_PIN               22   
#define LOAD2_PIN               13
#define LOAD3_PIN               17      // RELAY
#define ZERO_PIN                16

/*******************************************************
 MACROS
 *******************************************************/

#define LED_ON()       gpio_set_level(LED4_GPIO, 0)
#define LED_OFF()      gpio_set_level(LED4_GPIO, 1)

#define LOAD1_ON()     gpio_set_level(LED4_GPIO, 1)
#define LOAD1_OFF()    gpio_set_level(LED4_GPIO, 0)

// RELAY
#define LOAD3_ON()     gpio_set_level(LED4_GPIO, 1)
#define LOAD3_OFF()    gpio_set_level(LED4_GPIO, 0)


/*******************************************************
 FUNCTION PROTOTYPES
 *******************************************************/

static bool adc_calibration_init(void);
static void update_current_value(void *arg);

/*******************************************************
 LOCAL VARIABLES
 *******************************************************/
//ADC Channels
static const char *TAG = "HW CTRL";

static uint32_t voltage = 0;

static esp_adc_cal_characteristics_t adc1_chars;
static uint16_t current_value = 0;


static hw_electr_lvl_t  Load2_level = HW_LVL_OFF;
static hw_state_t       Load1_state = HW_OFF;
static hw_state_t       Load3_state = HW_OFF;
static uint32_t         Current = 0;

static hw_state_t       LED_state = HW_OFF;




/*******************************************************
 STATIC FUNCTION DEFINITIONS
 *******************************************************/


static void hw_ctrl_task(void *pvParameter)
{
    bool cali_enable = adc_calibration_init();
    static int adc_raw;

    //ADC1 config
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CURRENT_CHANNEL, ADC_EXAMPLE_ATTEN));

    // Configure LED4 pin for output
    gpio_reset_pin(LED4_GPIO);
    gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);    // Set the GPIO as a push/pull output

    // Configure Load1 pin for output
    gpio_reset_pin(LOAD1_PIN);
    gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);    // Set the GPIO as a push/pull output
    
    // Configure Load2 (variable power) pin for output
    gpio_reset_pin(LOAD2_PIN);
    gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);    // Set the GPIO as a push/pull output
    
    // Configure Load3 (RELAY) pin for output
    gpio_reset_pin(LOAD3_PIN);
    gpio_set_direction(LED4_GPIO, GPIO_MODE_OUTPUT);    // Set the GPIO as a push/pull output
    
    // Configure ZERO sensor pin for input
    gpio_reset_pin(ZERO_PIN);
    gpio_set_direction(LED4_GPIO, GPIO_MODE_INPUT);    // Set the GPIO as input

    LED_OFF();

    /* Create and start a periodic timer interrupt to call update_current_value() */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &update_current_value,
        .name = "periodic_current"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000000));


    while(1)
    {
        adc_raw = adc1_get_raw(ADC1_CURRENT_CHANNEL);
        ESP_LOGI(TAG, "Current raw data: %d", adc_raw);
        if (cali_enable)
        {
            voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc1_chars);
            ESP_LOGI(TAG, ">>>>>>>>>> cali data: %d mV", voltage);

            Current = voltage * CURRENT_COEFF;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));          // 1s
    }
}


/**
 * @brief Updates the value of Current for WQTT cloud
 * 
 * @param arg Arguments
 */
static void update_current_value(void *arg)
{
    wqtt_client_set_current( Current );
}


/**
 * The function `adc_calibration_init` checks if the ADC calibration scheme is supported and if the
 * eFuse is burnt, and then characterizes the ADC if both conditions are met.
 * 
 * @return a boolean value, which indicates whether the ADC calibration initialization was successful
 * or not. If the calibration was successful, the function returns true. Otherwise, it returns false.
 */
static bool adc_calibration_init(void)
{
    esp_err_t ret;
    bool cali_enable = false;

    ret = esp_adc_cal_check_efuse(ADC_EXAMPLE_CALI_SCHEME);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Calibration scheme not supported, skip software calibration");
    } else if (ret == ESP_ERR_INVALID_VERSION) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else if (ret == ESP_OK) {
        cali_enable = true;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_EXAMPLE_ATTEN, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);
    } else {
        ESP_LOGE(TAG, "Invalid arg");
    }

    return cali_enable;
}

/****************************************
 EXTERNAL FUNCTIONS
*****************************************/

/**
 * The function "hw_ctrl_start" creates a task called "hw_ctrl_task" with a stack size of 4096*2 and a
 * priority of 10.
 */
void hw_ctrl_start(void)
{
    xTaskCreate(hw_ctrl_task, "hw_ctrl_task", 4096*2, NULL, 10, NULL);
}

/**
 * @brief   Set level for Load2 within 0 .. 100%
 * 
 * @param level 0..100 (%)
 */
void hw_ctrl_set_Load2_level(hw_electr_lvl_t level)
{
    if(level > HW_LVL_VERY_HIGH )
    {
        level = HW_LVL_VERY_HIGH;
    }

    Load2_level = level;
}

/**
 * @brief   Get Load2 level
 * 
 * @return uint8_t Level of 2 ( 0 .. 100% )
 */
hw_electr_lvl_t hw_ctrl_get_Load2_level(void)
{
    return Load2_level;
}

/**
 * @brief   Set Load1 state
 * 
 * @param state HW_ON, HW_OFF
 */
void hw_ctrl_set_Load1_state(hw_state_t state)
{
    Load1_state = state;

    if(Load1_state == HW_ON)
    {
        LOAD1_ON();
    } else {
        LOAD1_OFF();
    }

}

/**
 * @brief   Get Load1 level
 * 
 * @return hw_state_t HW_ON, HW_OFF
 */
hw_state_t hw_ctrl_get_Load1_state(void)
{
    return Load1_state;
}

/**
 * @brief   Get Load3 level
 * 
 * @param state     HW_ON, HW_OFF
 */
void hw_ctrl_set_Load3_state(hw_state_t state)
{
    Load3_state = state;

    if(Load3_state == HW_ON)
    {
        LOAD3_ON();
    } else {
        LOAD3_OFF();
    }
}

/**
 * @brief   Get Load3 state
 * 
 * @return hw_state_t HW_ON, HW_OFF
 */
hw_state_t hw_ctrl_get_Load3_state(void)
{
    return Load3_state;
}

/**
 * @brief Gets a current value in mA
 * 
 * @return Current value in mA
 */
uint32_t hw_ctrl_get_Current(void)
{
    return Current;
}

/**
 * @brief Sets LED state
 * 
 * @param LED_new_state: HW_ON, HW_OFF
 */
void hw_ctrl_set_LED_state(hw_state_t LED_new_state)
{
    LED_state = LED_new_state;

    if(LED_state == HW_ON)
    {
        LED_ON();
    } else {
        LED_OFF();
    }
}

/**
 * @brief Get Lamp3 state
 * 
 * @return HW_ON, HW_OFF
 */
hw_state_t  hw_ctrl_get_LED_state(void)
{
    return LED_state;
}