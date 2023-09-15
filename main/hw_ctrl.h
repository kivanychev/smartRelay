#ifndef _HW_CTRL_H_
#define _HW_CTRL_H_

#include <stdlib.h>

/**********************************
 CONSTANTS AND MACROS
***********************************/

#define LV_TICK_PERIOD_MS   1

// Covers for function names according to functional intention
#define hw_ctrl_set_Heater_state    hw_ctrl_set_Load1_state
#define hw_ctrl_set_Fan_level       hw_ctrl_set_Load2_level
#define hw_ctrl_set_Light_state     hw_ctrl_set_Load3_state
#define hw_ctrl_get_Heater_state    hw_ctrl_get_Load1_state
#define hw_ctrl_get_Fan_level       hw_ctrl_get_Load2_level
#define hw_ctrl_get_Light_state     hw_ctrl_get_Load3_state

/**********************************
 TYPES DEFINITIONS
***********************************/

typedef enum {
    HW_OFF = 0,
    HW_ON  = 1
} hw_state_t;

typedef enum {
    HW_LVL_OFF = 1,
    HW_LVL_LOW = 2,
    HW_LVL_MEDIUM = 3,
    HW_LVL_HIGH = 4,
    HW_LVL_VERY_HIGH = 5

} hw_electr_lvl_t;

/**********************************
 FUNCTION PROTTOTYPES
***********************************/

void            hw_ctrl_start();

void            hw_ctrl_set_Load2_level(hw_electr_lvl_t level);
hw_electr_lvl_t hw_ctrl_get_Load2_level(void);

void            hw_ctrl_set_Load1_state(hw_state_t state);
hw_state_t      hw_ctrl_get_Load1_state(void);

void            hw_ctrl_set_Load3_state(hw_state_t state);
hw_state_t      hw_ctrl_get_Load3_state(void);

uint32_t        hw_ctrl_get_Current(void);

void            hw_ctrl_set_LED_state(hw_state_t LED_new_state);
hw_state_t      hw_ctrl_get_LED_state(void);

#endif // _HW_CTRL_H_