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
#include "MQTTCfgParser.h"

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

static void processbuttonCallback1 (void * param1, uint32_t buttonstatus)
{
	BCDS_UNUSED(param1);

	LOG_AT_TRACE(("MQTTButton: Status button %d %lu %i\r\n", AppController_GetStatus(), buttonstatus, BSP_XDK_BUTTON_PRESS));

	// only use button 1 when in operation mode
	if (AppController_GetStatus() == APP_STATUS_OPERATING_STARTED && BSP_XDK_BUTTON_PRESSED == buttonstatus)
		CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_StopTimer, NULL, buttonstatus);
	if (AppController_GetStatus() == APP_STATUS_OPERATING_STOPPED && BSP_XDK_BUTTON_PRESSED == buttonstatus)
		CmdProcessor_EnqueueFromIsr(AppCmdProcessor, MQTTOperation_StartTimer, NULL, buttonstatus);
}


static void processbuttonCallback2 (void * param1, uint32_t buttonstatus)
{
	static TickType_t time_start = 0;
	BCDS_UNUSED(param1);
	if (BSP_XDK_BUTTON_PRESSED == buttonstatus ) {
		time_start = xTaskGetTickCountFromISR();

	} else if (BSP_XDK_BUTTON_RELEASED == buttonstatus){
		TickType_t time_passed = xTaskGetTickCountFromISR() - time_start;
		if (time_passed > pdMS_TO_TICKS(3000)) {
			MQTTFlash_FLWriteBootStatus((uint8_t*) NO_BOOT_PENDING);
			LOG_AT_TRACE(("MQTTButton: Button pressed long: %lu\r\n", time_passed));
		} else {
			ConfigDataBuffer localbuffer;
			localbuffer.length = NUMBER_UINT32_ZERO;
			memset(localbuffer.data, 0x00, SIZE_XXLARGE_BUF);
			MQTTFlash_FLReadConfig(&localbuffer);
			LOG_AT_DEBUG(("MQTTButton: Current configuration in flash:\r\n%s\r\n", localbuffer.data));

			localbuffer.length = NUMBER_UINT32_ZERO;
			memset(localbuffer.data, 0x00, SIZE_XXLARGE_BUF);
			MQTTCfgParser_GetConfig(&localbuffer, CFG_FALSE);
			LOG_AT_DEBUG(("MQTTButton: Currently used configuration:\r\n%s\r\n", localbuffer.data));
			LOG_AT_TRACE(("MQTTButton: Button pressed for: %lu\r\n", time_passed));
		}
	}

}


/*Callback for Button 1 */
void button1Callback(uint32_t data)
{
	Retcode_T returnValue = CmdProcessor_EnqueueFromIsr(AppCmdProcessor, processbuttonCallback1, NULL, data);
	if (RETCODE_OK != returnValue)
	{
		LOG_AT_ERROR(("MQTTButton: Enqueuing for Button 1 callback failed\r\n"));
	}
}


/*Callback for Button 2 */
void button2Callback(uint32_t data)
{
	Retcode_T returnValue = CmdProcessor_EnqueueFromIsr(AppCmdProcessor, processbuttonCallback2, NULL, data);
	if (RETCODE_OK != returnValue)
	{
		LOG_AT_ERROR(("MQTTButton: Enqueuing for Button 2 callback failed [%lu]\r\n", returnValue));
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
