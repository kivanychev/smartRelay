
#ifndef _SMART_RELAY_H_
#define _SMART_RELAY_H_

#include <stdlib.h>

#include "hw_ctrl.h"


void ui_set_fan_speed(uint32_t new_fan_speed);
void ui_set_light_state(hw_state_t new_state);
void ui_set_heater_state(hw_state_t new_state);
void ui_set_current_value(uint32_t new_current_value);



#endif // _SMART_RELAY_H_