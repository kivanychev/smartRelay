#ifndef _HW_CTRL_H_
#define _HW_CTRL_H_

#include <stdlib.h>



/**********************************
 TYPES DEFINITIONS
***********************************/
typedef enum {
    HW_OFF = 0,
    HW_ON  = 1
} hw_state_t;

/**********************************
 FUNCTION PROTTOTYPES
***********************************/

void        hw_ctrl_start();

void        hw_ctrl_set_Load2_level(uint8_t level);     // Fan
uint8_t     hw_ctrl_get_Load2_level(void);

void        hw_ctrl_set_Load1_state(hw_state_t state);
hw_state_t  hw_ctrl_get_Load1_state(void);

void        hw_ctrl_set_Load3_state(hw_state_t state);
hw_state_t  hw_ctrl_get_Load3_state(void);

uint32_t    hw_ctrl_get_Current(void);

#endif // _HW_CTRL_H_