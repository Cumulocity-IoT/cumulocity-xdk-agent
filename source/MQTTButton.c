/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTButton.c
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/* module includes ********************************************************** */

/* system header files */

/* own header files */
#include "AppController.h"
#include "MQTTFlash.h"
#include "MQTTOperation.h"
#include "MQTTButton.h"

/* additional interface header files */
#include "BSP_BoardType.h"
#include "BCDS_BSP_Button.h"
#include "BCDS_CmdProcessor.h"
#include <stdio.h>


/* constant definitions ***************************************************** */

/* local variables ********************************************************** */
static CmdProcessor_T *AppCmdProcessor; /**< Application Command Processor Instance */


/* global variables ********************************************************* */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
/*
 * @brief Callback function for a button press
 *
 * @param[in] handle - UNUSED
 * @param[in] userParameter - User defined parameter to identify which button is pressed
 *
 * @return NONE
 */

static void processbuttonCallback1 (void * param1, uint32_t buttonstatus)
{
	static uint8_t toogleButton_1 = 0;
    BCDS_UNUSED(param1);

    if (DEBUG_LEVEL <= FINEST ) printf("MQTTButton: Status button %d %lu %i\n\r", toogleButton_1, buttonstatus, BSP_XDK_BUTTON_PRESS);

	if (BSP_XDK_BUTTON_PRESS == buttonstatus ) {
		if (toogleButton_1 == 0) {
			CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_StopTimer, NULL, buttonstatus);
		} else {
			CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_StartTimer, NULL, buttonstatus);
		}
		toogleButton_1 =! toogleButton_1;
	}
}



static void processbuttonCallback2 (void * param1, uint32_t buttonstatus)
{
    BCDS_UNUSED(param1);
	if (BSP_XDK_BUTTON_PRESS == buttonstatus ) {
		MQTTFlash_WriteBootStatus(NO_BOOT_PENDING);
	}

}


/*Callback for Button 1 */
void button1Callback(uint32_t data)
{
    Retcode_T returnValue = CmdProcessor_EnqueueFromIsr(AppCmdProcessor, processbuttonCallback1, NULL, data);
    if (RETCODE_OK != returnValue)
    {
        printf("MQTTButton: Enqueuing for Button 1 callback failed\n\r");
    }
}


/*Callback for Button 1 */
void button2Callback(uint32_t data)
{
    Retcode_T returnValue = CmdProcessor_EnqueueFromIsr(AppCmdProcessor, processbuttonCallback2, NULL, data);
    if (RETCODE_OK != returnValue)
    {
        printf("MQTTButton: Enqueuing for Button 2 callback failed\n\r");
    }
}



/* global functions ********************************************************* */
/*
 * @brief Initializes the buttons
 *
 * @return NONE
 */
APP_RESULT MQTTButton_Init(void * CmdProcessorHandle)
{
        AppCmdProcessor = (CmdProcessor_T *) CmdProcessorHandle;

	    Retcode_T returnVal = RETCODE_OK;
	    returnVal = BSP_Button_Connect();
	    if (RETCODE_OK != returnVal) {
	    	return APP_ERROR;
	    }
	    returnVal = BSP_Button_Enable((uint32_t) BSP_XDK_BUTTON_1, button1Callback);
	    if (RETCODE_OK != returnVal) {
	    	return APP_ERROR;
	    }
        returnVal = BSP_Button_Enable((uint32_t) BSP_XDK_BUTTON_2, button2Callback);
	    if (RETCODE_OK != returnVal) {
	    	return APP_ERROR;
	    }
	    return APP_OK;
}
