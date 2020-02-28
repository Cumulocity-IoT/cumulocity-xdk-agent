/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTOperation.h
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/
#include "XDK_MQTT_Z.h"
#include "XDK_MQTT.h"
#include "XDK_Sensor.h"

/* header definition ******************************************************** */
#ifndef _MQTT_OPERATION_H_
#define _MQTT_OPERATION_H_

/* Cumulocity MQTT declaration ********************************************** */

//Cumulocity standard mqtt tmeplate number
#define TEMPLATE_STD_CREDENTIALS  	"70"
#define TEMPLATE_STD_RESTART    	"510"
#define TEMPLATE_STD_COMMAND    	"511"
#define TEMPLATE_STD_FIRMWARE    	"515"
#define TEMPLATE_CUS_MESSAGE    	"999"


//Cumulocity topics to send data
#define TOPIC_ASSET_STREAM    	     "s/us"
#define TOPIC_DATA_STREAM    	     "s/uc/XDK"

//Topic for template data stream s/us/<xId>
#define TOPIC_DOWNSTREAM_CUSTOM      "s/dc/XDK"
#define TOPIC_DOWNSTREAM_STANDARD    "s/ds"
#define TOPIC_DOWNSTREAM_ERROR       "s/e"

#define MILLISECONDS(x) ((portTickType) x / portTICK_RATE_MS)
#define SECONDS(x) ((portTickType) (x * 1000) / portTICK_RATE_MS)


/* global function prototype declarations */
void MQTTOperation_Init(void);
void MQTTOperation_DeInit(void);
void MQTTOperation_QueueCommand(void * param1, uint32_t param2);


typedef enum
{
	DEVICE_OPERATION_WAITING,
	DEVICE_OPERATION_BEFORE_EXECUTING,
	DEVICE_OPERATION_BEFORE_FAILED,
	DEVICE_OPERATION_EXECUTING,
	DEVICE_OPERATION_FAILED,
	DEVICE_OPERATION_SUCCESSFUL,
	DEVICE_OPERATION_BEFORE_PENDING,
	DEVICE_OPERATION_PENDING,
	DEVICE_OPERATION_AFTER_PENDING,
	DEVICE_OPERATION_IMMEDIATE_BUTTON,
	DEVICE_OPERATION_IMMEDIATE_CMD,
	DEVICE_OPERATION_BEFORE_SUCCESSFUL,
} DEVICE_OPERATION;

typedef struct commands_S commands_T;

struct commands_S
{
	/// name of command
	const char * name;
	/// to be used when acknowledge command
	const char * type;
};

typedef enum
{
	CMD_TOGGLE,
	CMD_RESTART,
	CMD_SPEED,
	CMD_MESSAGE,
	CMD_SENSOR,
	CMD_START,
	CMD_STOP,
	CMD_CONFIG,
	CMD_FIRMWARE,
	CMD_UNKNOWN,
	CMD_REQUEST,
	CMD_COMMAND,
} C8Y_COMMAND;


static const char * const commands[] = {
		"c8y_Command",
		"c8y_Restart",
		"c8y_Command",
		"c8y_Message",
		"c8y_Command",
		"c8y_Command",
		"c8y_Command",
		"c8y_Command",
		"c8y_Firmware",
		"c8y_Command",
		"c8y_Command",
		"c8y_Command",
};

/* global variable declarations */

/* global inline function definitions */

#endif /* _MQTT_OPERATION_H_ */
