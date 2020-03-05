/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTButton.h
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/


/* Button interface declaration ********************************************** */

/* Button type and macro definitions */
#define CALLBACK_PARAMETER_PB1 UINT32_C(0x11)     /**< Macro represents callback parameter for Button1 */
#define CALLBACK_PARAMETER_PB2 UINT32_C(0x12)     /**< Macro represents callback parameter for Button2 */

/* global function prototype declarations */
Retcode_T MQTTButton_Init(void * CmdProcessorHandle);

/* global variable declarations */

/* global inline function definitions */
