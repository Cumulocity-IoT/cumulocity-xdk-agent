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
#include "MQTTButton.h"
#include "MQTTOperation.h"

/* additional interface header files */
#include "BSP_BoardType.h"
#include "BCDS_BSP_Button.h"
#include "BCDS_CmdProcessor.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"


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


/*Callback for Button 1 */
void button1Callback(uint32_t buttonstatus) {
	static TickType_t time_start = 0;

	LOG_AT_TRACE(("MQTTButton: Status button %d %lu %i\r\n", AppController_GetStatus(), buttonstatus, BSP_XDK_BUTTON_PRESS));
	if (BSP_XDK_BUTTON_PRESSED == buttonstatus ) {
		time_start = xTaskGetTickCountFromISR();
	} else {
		TickType_t time_passed = xTaskGetTickCountFromISR() - time_start;
		if (time_passed > pdMS_TO_TICKS(3000)) {
			CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_QueueCommand, "511,DUMMY,requestCommands", UINT32_C(0));
			LOG_AT_TRACE(("MQTTButton: Button1 pressed long: %lu\r\n", time_passed));
		} else {
			// only use button 1 when in operation mode
			if (AppController_GetStatus() == APP_STATUS_OPERATING_STARTED ) {
				CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_QueueCommand, "511,DUMMY,stopButton", UINT32_C(0));
			} else if (AppController_GetStatus() == APP_STATUS_OPERATING_STOPPED) {
				CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_QueueCommand, "511,DUMMY,startButton", UINT32_C(0));
			}
		}
	}
}

/*Callback for Button 2 */
void button2Callback(uint32_t buttonstatus) {
	static TickType_t time_start = 0;
	if (BSP_XDK_BUTTON_PRESSED == buttonstatus ) {
		time_start = xTaskGetTickCountFromISR();

	} else if (BSP_XDK_BUTTON_RELEASED == buttonstatus){
		TickType_t time_passed = xTaskGetTickCountFromISR() - time_start;
		if (time_passed > pdMS_TO_TICKS(3000)) {
			CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_QueueCommand, "511,DUMMY,resetBootstatus", UINT32_C(0));
			LOG_AT_TRACE(("MQTTButton: Button2 pressed long: %lu\r\n", time_passed));
		} else {
			CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_QueueCommand, "511,DUMMY,printConfig", UINT32_C(0));
			LOG_AT_TRACE(("MQTTButton: Button2 pressed for: %lu\r\n", time_passed));
		}
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
		return APP_RESULT_ERROR;
	}
	returnVal = BSP_Button_Enable((uint32_t) BSP_XDK_BUTTON_1, button1Callback);
	if (RETCODE_OK != returnVal) {
		return APP_RESULT_ERROR;
	}
	returnVal = BSP_Button_Enable((uint32_t) BSP_XDK_BUTTON_2, button2Callback);
	if (RETCODE_OK != returnVal) {
		return APP_RESULT_ERROR;
	}
	return APP_RESULT_OK;
}
