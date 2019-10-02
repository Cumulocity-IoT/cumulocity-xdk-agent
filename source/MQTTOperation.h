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


//Cumulocity topics to send data
#define TOPIC_ASSET_STREAM    	 "s/us"
#define TOPIC_DATA_STREAM    	 "s/uc/XDK"

//Topic for template data stream s/us/<xId>
#define TOPIC_DOWNSTREAM_CUSTOM        "s/dc/XDK"
#define TOPIC_DOWNSTREAM_STANDARD        "s/ds"

#define MILLISECONDS(x) ((portTickType) x / portTICK_RATE_MS)
#define SECONDS(x) ((portTickType) (x * 1000) / portTICK_RATE_MS)


/* global function prototype declarations */
void MQTTOperation_Init(MQTT_Setup_TZ, MQTT_Connect_TZ, MQTT_Credentials_TZ, Sensor_Setup_T);
void MQTTOperation_DeInit(void);
void MQTTOperation_StartTimer(void *, uint32_t);
void MQTTOperation_StopTimer(void *, uint32_t);


typedef enum
{
	DEVICE_OPERATION_WAITING = INT16_C(0),
	DEVICE_OPERATION_BEFORE_EXECUTING = INT16_C(1),
	DEVICE_OPERATION_BEFORE_FAILED = INT16_C(2),
	DEVICE_OPERATION_EXECUTING = INT16_C(501),
	DEVICE_OPERATION_FAILED = INT16_C(502),
	DEVICE_OPERATION_SUCCESSFUL = INT16_C(503),
	DEVICE_OPERATION_IMMEDIATE= INT16_C(599),
} DEVICE_OPERATION;


typedef enum
{
	CMD_TOGGLE= INT8_C(1),
	CMD_RESTART = INT8_C(2),
	CMD_SPEED= INT8_C(3),
	CMD_MESSAGE= INT8_C(4),
	CMD_SENSOR= INT8_C(5),
} C8Y_COMMAND;

/* global variable declarations */

/* global inline function definitions */

#endif /* _MQTT_OPERATION_H_ */
