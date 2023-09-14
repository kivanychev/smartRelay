#ifndef _WQTT_CLIENT_H_
#define _WQTT_CLIENT_H_

#include <stdlib.h>

#include "hw_ctrl.h"

/**********************************
 CONSTANTS AND MACROS
***********************************/

// WQTT topic names
#define Heater_topic    "Heater"
#define Fan_topic       "Fan"
#define Light_topic     "Light"
#define Current_topic   "Current"
#define LED_topic       "LED"

/**********************************
 FUNCTION PROTTOTYPES
***********************************/


void            wqtt_client_start( void );

void            wqtt_client_set_current( uint32_t Current );

void            wqtt_client_set_Fan_level(hw_electr_lvl_t level);
hw_electr_lvl_t wqtt_client_get_Fan_level(void);

void            wqtt_client_set_Heater_state(hw_state_t state);
hw_state_t      wqtt_client_get_Heater_state(void);

void            wqtt_client_set_Light_state(hw_state_t state);
hw_state_t      wqtt_client_get_Light_state(void);

uint32_t        wqtt_client_get_Current(void);

void            wqtt_client_set_LED_state(hw_state_t LED_new_state);
hw_state_t      wqtt_client_get_LED_state(void);


#endif // _WQTT_CLIENT_H_