/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTOperation.c
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/* system header files */

/* own header files */

#include "AppController.h"
#include "MQTTOperation.h"
#include "MQTTStorage.h"
#include "MQTTCfgParser.h"

/* additional interface header files */
#include "BSP_BoardType.h"
#include "BCDS_BSP_LED.h"
#include "BCDS_BSP_Board.h"
#include "BCDS_WlanNetworkConfig.h"
#include "BCDS_WlanNetworkConnect.h"
#include "BCDS_Orientation.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "math.h"
#include "Serval_Mqtt.h"
#include "XDK_TimeStamp.h"
#include "XDK_SNTP.h"
#include "XDK_WLAN.h"
#include "BatteryMonitor.h"
#include "XdkSensorHandle.h"
#include "XdkCommonInfo.h"

static const int MINIMAL_SPEED = 25;
/* constant definitions ***************************************************** */
const float aku340ConversionRatio = 0.01258925411794167210423954106396; //pow(10,(-38/20));
/* local variables ********************************************************** */
static int tickRateMS;
static APP_ASSET_UPDATE_STATUS assetUpdateProcess = APP_ASSET_INITIAL;
static DEVICE_OPERATION commandProgress = DEVICE_OPERATION_WAITING;
static C8Y_COMMAND command = CMD_UNKNOWN;
static uint16_t connectAttemps = 0UL;
static xTimerHandle timerHandleSensor;
static xTimerHandle timerHandleAsset;
static int errorCountSemaphore = 0;
static int errorCountPublish = 0;
SemaphoreHandle_t semaphoreAssetBuffer;
SemaphoreHandle_t semaphoreSensorBuffer;
QueueHandle_t commandQueue;

/* global variables ********************************************************* */
extern SensorDataBuffer sensorStreamBuffer;
extern AssetDataBuffer assetStreamBuffer;
extern MQTT_Setup_TZ MqttSetupInfo;
extern MQTT_Connect_TZ MqttConnectInfo;
extern MQTT_Credentials_TZ MqttCredentials;
extern Sensor_Setup_T SensorSetup;
extern xTaskHandle AppControllerHandle;
extern CmdProcessor_T MainCmdProcessor;

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
static void MQTTOperation_ClientReceive(MQTT_SubscribeCBParam_TZ param);
static void MQTTOperation_ClientPublish(void);
static void MQTTOperation_AssetUpdate(xTimerHandle xTimer);
static Retcode_T MQTTOperation_SubscribeTopics(void);
static void MQTTOperation_StartRestartTimer(int period);
static void MQTTOperation_StartTimer(void);
static void MQTTOperation_StopTimer(void);
static void MQTTOperation_RestartCallback(xTimerHandle xTimer);
static Retcode_T MQTTOperation_ValidateWLANConnectivity(void);
static void MQTTOperation_SensorUpdate(xTimerHandle xTimer);
static float MQTTOperation_CalcSoundPressure(float acousticRawValue);
static void MQTTOperation_ExecuteCommand(char * commandBuffer);
static void MQTTOperation_PrepareAssetUpdate(void);

static MQTT_Subscribe_TZ MqttSubscribeCommandInfo = { .Topic =
		TOPIC_DOWNSTREAM_CUSTOM, .QoS = MQTT_QOS_AT_MOST_ONE,
		.IncomingPublishNotificationCB = MQTTOperation_ClientReceive, };/**< MQTT subscribe parameters */

static MQTT_Subscribe_TZ MqttSubscribeRestartInfo = { .Topic =
		TOPIC_DOWNSTREAM_STANDARD, .QoS = MQTT_QOS_AT_MOST_ONE,
		.IncomingPublishNotificationCB = MQTTOperation_ClientReceive, };/**< MQTT subscribe parameters */

static MQTT_Subscribe_TZ MqttSubscribeErrorInfo = { .Topic =
		TOPIC_DOWNSTREAM_ERROR, .QoS = MQTT_QOS_AT_MOST_ONE,
		.IncomingPublishNotificationCB = MQTTOperation_ClientReceive, };/**< MQTT subscribe parameters */

static MQTT_Publish_TZ MqttPublishAssetInfo = { .Topic = TOPIC_ASSET_STREAM,
		.QoS = MQTT_QOS_AT_MOST_ONE, .Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */

static MQTT_Publish_TZ MqttPublishDataInfo = { .Topic = TOPIC_DATA_STREAM,
		.QoS = MQTT_QOS_AT_MOST_ONE, .Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */

/**
 * @brief callback function for subriptions
 *        toggles LEDS or sets read data flag
 *
 * @param[in] md - received message from the MQTT Broker
 *
 * @return NONE
 */
static void MQTTOperation_ClientReceive(MQTT_SubscribeCBParam_TZ param) {

	/* Initialize Variables */
	static char appIncomingMsgTopicBuffer[SIZE_SMALL_BUF];/**< Incoming message topic buffer */
	static char appIncomingMsgPayloadBuffer[SIZE_LARGE_BUF];/**< Incoming message payload buffer */

	memset(appIncomingMsgTopicBuffer, 0, sizeof(appIncomingMsgTopicBuffer));
	memset(appIncomingMsgPayloadBuffer, 0, sizeof(appIncomingMsgPayloadBuffer));

	strncpy(appIncomingMsgTopicBuffer, (const char *) param.Topic,
			fmin(param.TopicLength, (sizeof(appIncomingMsgTopicBuffer) - 1U)));
	strncpy(appIncomingMsgPayloadBuffer, (const char *) param.Payload,
			fmin(param.PayloadLength,
					(sizeof(appIncomingMsgPayloadBuffer) - 1U)));

	if (strncmp(appIncomingMsgTopicBuffer, TOPIC_DOWNSTREAM_ERROR,
			strlen(TOPIC_DOWNSTREAM_ERROR)) == 0) {
		LOG_AT_ERROR(
				("MQTTOperation: Error from upstream: %.*s, Error Msg : %.*s\r\n", (int) param.TopicLength, appIncomingMsgTopicBuffer, (int) param.PayloadLength, appIncomingMsgPayloadBuffer));
	} else {
		LOG_AT_INFO(
				("MQTTOperation: Upstream msg: Topic: %.*s, Msg Received: %.*s\r\n", (int) param.TopicLength, appIncomingMsgTopicBuffer, (int) param.PayloadLength, appIncomingMsgPayloadBuffer));

		AppController_SetCmdStatus(APP_STATUS_COMMAND_RECEIVED);
		// split batch of commands in single commands
		char *token = strtok(appIncomingMsgPayloadBuffer, "\n");
		while (token != NULL) {
			LOG_AT_ERROR(
					("MQTTOperation: Try to place command [%s] in queue!\r\n", token));
			if (xQueueSend(commandQueue,token, 0) != pdTRUE) {
				LOG_AT_ERROR(
						("MQTTOperation_ClientReceive: Could not buffer command!\r\n"));
				break;
			}
			token = strtok(NULL, "\n");
		}

	}

}

static void MQTTOperation_ExecuteCommand(char * commandBuffer) {
	/* Initialize Variables */

	command = CMD_UNKNOWN;

	LOG_AT_INFO(("MQTTOperation: Execute command: [%s]\r\n", commandBuffer));

	// split payload into tokens
	int token_pos = 0;
	int config_index = -1;
	bool commandComplete = false;
	char *token = strtok(commandBuffer, ",:");

	while (token != NULL) {
		LOG_AT_TRACE(("MQTTOperation: Processing token: [%s], token_pos: [%i] \r\n", token, token_pos));

		switch (token_pos) {
		case 0:
			if (strcmp(token, TEMPLATE_STD_RESTART) == 0) {
				LOG_AT_TRACE(("MQTTOperation: Starting restart \r\n"));
				AppController_SetAppStatus(APP_STATUS_REBOOT);
				// set flag so that XDK acknowledges reboot command
				command = CMD_RESTART;
				commandComplete = true;
				MQTTOperation_StartRestartTimer(REBOOT_DELAY);
				LOG_AT_TRACE(("MQTTOperation: Ending restart\r\n"));
			} else if (strcmp(token, TEMPLATE_STD_COMMAND) == 0) {
				command = CMD_COMMAND;
			} else if (strcmp(token, TEMPLATE_STD_FIRMWARE) == 0) {
				command = CMD_FIRMWARE;
			} else if (strcmp(token, TEMPLATE_CUS_MESSAGE) == 0) {
				command = CMD_MESSAGE;
			} else {
				// set command as unknown
				command = CMD_UNKNOWN;
			}
			break;
		case 1:
			//do nothing, ignore the device ID
			break;
		case 2:
			if (command == CMD_COMMAND) {
				if (strcmp(token, "speed") == 0) {
					command = CMD_SPEED;
				} else if (strcmp(token, "toggle") == 0) {
					BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_TOGGLE);
					command = CMD_TOGGLE;
					commandComplete = true;
					// skip phase BEFORE_EXECUTING, because LED is switched on immediately
					commandProgress = DEVICE_OPERATION_IMMEDIATE_CMD;
				} else if (strcmp(token, "start") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_CMD;
					command = CMD_PUBLISH_START;
					commandComplete = true;
					MQTTOperation_StartTimer();
				} else if (strcmp(token, "startButton") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_BUTTON;
					command = CMD_PUBLISH_START;
					commandComplete = true;
					MQTTOperation_StartTimer();
				} else if (strcmp(token, "stop") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_CMD;
					command = CMD_PUBLISH_STOP;
					commandComplete = true;
					MQTTOperation_StopTimer();
				} else if (strcmp(token, "stopButton") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_BUTTON;
					command = CMD_PUBLISH_STOP;
					commandComplete = true;
					MQTTOperation_StopTimer();
				} else if (strcmp(token, "printConfig") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_BUTTON;
					command = CMD_COMMAND;
					commandComplete = true;

					ConfigDataBuffer localbuffer;
					localbuffer.length = NUMBER_UINT32_ZERO;
					memset(localbuffer.data, 0x00, SIZE_XXLARGE_BUF);
					MQTTStorage_Flash_ReadConfig(&localbuffer);
					LOG_AT_DEBUG(
							("MQTTOperation: Current configuration in flash:\r\n%s\r\n", localbuffer.data));

					localbuffer.length = NUMBER_UINT32_ZERO;
					memset(localbuffer.data, 0x00, SIZE_XXLARGE_BUF);
					MQTTCfgParser_GetConfig(&localbuffer, CFG_FALSE);
					LOG_AT_DEBUG(
							("5s: Currently used configuration:\r\n%s\r\n", localbuffer.data));
				} else if (strcmp(token, "resetBootstatus") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_BUTTON;
					command = CMD_COMMAND;
					commandComplete = true;
					MQTTStorage_Flash_WriteBootStatus(
							(uint8_t*) NO_BOOT_PENDING);
				} else if (strcmp(token, "requestCommands") == 0) {
					commandProgress = DEVICE_OPERATION_IMMEDIATE_BUTTON;
					command = CMD_REQUEST;
					commandComplete = true;
				} else if (strcmp(token, "sensor") == 0) {
					command = CMD_SENSOR;
				} else if (strcmp(token, "config") == 0) {
					command = CMD_CONFIG;
				} else if (strcmp(token, "restartConfirm") == 0) {
					command = CMD_RESTART;
					commandComplete = true;
					commandProgress = DEVICE_OPERATION_EXECUTING;
					MQTTStorage_Flash_WriteBootStatus((uint8_t*) NO_BOOT_PENDING);
				} else {
					commandProgress = DEVICE_OPERATION_BEFORE_FAILED;
					LOG_AT_WARNING(("MQTTOperation: Unknown command: %s\r\n", token));
				}
				LOG_AT_DEBUG(
						("MQTTOperation: Token: [%s] recognized as command: [%i]\r\n", token, command));
			} else if (command == CMD_FIRMWARE) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase parse command firmware name: token_pos: [%i]\r\n",token_pos));
				MQTTCfgParser_SetFirmwareName(token);
			} else if (command == CMD_MESSAGE) {
				BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y,
						(uint32_t) BSP_LED_COMMAND_TOGGLE);
				commandComplete = true;
				// skip phase BEFORE_EXECUTING, because LED is switched on immediately
				commandProgress = DEVICE_OPERATION_IMMEDIATE_CMD;
			}
			break;
		case 3:
			if (command == CMD_SPEED) {
				int speed = strtol(token, (char **) NULL, 10);
				speed = (speed <= 2 * MINIMAL_SPEED) ?
						2 * MINIMAL_SPEED : speed;
				LOG_AT_DEBUG(
						("MQTTOperation: Phase execute command speed, new speed: [%i]\r\n", speed));
				tickRateMS = (int) pdMS_TO_TICKS(speed);
				xTimerChangePeriod(timerHandleSensor, tickRateMS,
						UINT32_C(0xffff));
				MQTTCfgParser_SetStreamRate(speed);
				MQTTCfgParser_FLWriteConfig();
				assetUpdateProcess = APP_ASSET_WAITING;
				commandComplete = true;
			} else if (command == CMD_SENSOR) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase parse command sensor: token_pos: [%i]\r\n", token_pos));
				for (int var = ATT_IDX_ACCEL; var <= ATT_IDX_NOISE; ++var) {
					if (strcmp(token, ATT_KEY_NAME[var]) == 0) {
						config_index = var;
						break;
					}
				}
				if (config_index == -1) {
					commandProgress = DEVICE_OPERATION_BEFORE_FAILED;
					LOG_AT_WARNING(
							("MQTTOperation: Sensor not supported: %s\r\n", token));
				}
			} else if (command == CMD_FIRMWARE) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase execute command firmware version: token_pos: [%i]\r\n", token_pos));
				MQTTCfgParser_SetFirmwareVersion(token);
			} else if (command == CMD_CONFIG) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase parse command config: token_pos: [%i]\r\n", token_pos));
				for (int var = 0; var < ATT_IDX_SIZE; ++var) {
					if (strcmp(token, ATT_KEY_NAME[var]) == 0) {
						config_index = var;
						break;
					}
				}
				if (config_index == -1) {
					commandProgress = DEVICE_OPERATION_BEFORE_FAILED;
					LOG_AT_WARNING(
							("MQTTOperation: Config change not supported: %s\r\n", token));
				}
			}
			break;
		case 4:
			if (command == CMD_SENSOR) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase execute command sensor: token_pos: [%i]\r\n", token_pos));
				MQTTCfgParser_SetSensor(token, config_index);
				MQTTCfgParser_FLWriteConfig();
				assetUpdateProcess = APP_ASSET_WAITING;
				commandComplete = true;
			} else if (command == CMD_CONFIG) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase execute command config: token_pos: [%i]\r\n", token_pos));
				MQTTCfgParser_SetConfig(token, config_index);
				MQTTCfgParser_FLWriteConfig();
				assetUpdateProcess = APP_ASSET_WAITING;
				commandComplete = true;
			} else if (command == CMD_FIRMWARE) {
				LOG_AT_DEBUG(
						("MQTTOperation: Phase parse firmware url: token_pos: [%i]\r\n", token_pos));
				MQTTCfgParser_SetFirmwareURL(token);
				MQTTCfgParser_FLWriteConfig();
				assetUpdateProcess = APP_ASSET_WAITING;
				commandComplete = true;
			}
			break;
		default:
			LOG_AT_WARNING(("MQTTOperation: Error parsing command!\r\n"));
			break;
		}
		token = strtok(NULL, ", ");
		token_pos++;
	}
	// test if command was complete
	if (commandComplete == false) {
		commandProgress = DEVICE_OPERATION_BEFORE_FAILED;
		LOG_AT_ERROR(("MQTTOperation: Incomplete command!\r\n"));
	}

}

static void MQTTOperation_StartRestartTimer(int period) {
	xTimerHandle timerHandle = xTimerCreate(
			(const char * const ) "Restart Timer", // used only for debugging purposes
			MILLISECONDS(period), // timer period
			pdFALSE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			MQTTOperation_RestartCallback // static callback function
			);
	xTimerStart(timerHandle, UINT32_C(0xffff));
}

static void MQTTOperation_RestartCallback(xTimerHandle xTimer) {
	(void) xTimer;
	LOG_AT_INFO(("MQTTOperation: Now calling SoftReset ...\r\n"));
	MQTTStorage_Flash_WriteBootStatus((uint8_t *) BOOT_PENDING);
	MQTTOperation_DeInit();
	xTimerStop(timerHandleAsset, UINT32_C(0xffff));
	xTimerStop(timerHandleSensor, UINT32_C(0xffff));
	BSP_Board_SoftReset();
}

/**
 * @brief publish sensor data, get sensor data, or
 *        yield mqtt client to check subscriptions
 *
 * @param[in] pvParameters UNUSED/PASSED THROUGH
 *
 * @return NONE
 */
static void MQTTOperation_ClientPublish(void) {

	LOG_AT_INFO(("MQTTOperation: Start publishing ...\r\n"));

	semaphoreAssetBuffer = xSemaphoreCreateBinary();
	xSemaphoreGive(semaphoreAssetBuffer);
	semaphoreSensorBuffer = xSemaphoreCreateBinary();
	xSemaphoreGive(semaphoreSensorBuffer);
	commandQueue = xQueueCreate(4, SIZE_XSMALL_BUF);

	Retcode_T retcode = RETCODE_OK;
	// initialize buffers
	memset(sensorStreamBuffer.data, 0x00, sizeof(sensorStreamBuffer.data));
	sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
	memset(assetStreamBuffer.data, 0x00, sizeof(assetStreamBuffer.data));
	assetStreamBuffer.length = NUMBER_UINT32_ZERO;

	timerHandleAsset = xTimerCreate((const char * const ) "Asset Update Timer", // used only for debugging purposes
			MILLISECONDS(1000), // timer period
			pdTRUE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			MQTTOperation_AssetUpdate // static callback function
			);
	xTimerStart(timerHandleAsset, UINT32_C(0xffff));

	timerHandleSensor = xTimerCreate(
			(const char * const ) "Sensor Update Timer", // used only for debugging purposes
			pdMS_TO_TICKS(tickRateMS), // timer period
			pdTRUE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			MQTTOperation_SensorUpdate // static callback function
			);
	xTimerStart(timerHandleSensor, UINT32_C(0xffff));


	// ckeck if reboot process is pending to be confirmed
	char readbuffer[SIZE_SMALL_BUF]; /* Temporary buffer for write file */
	MQTTStorage_Flash_ReadBootStatus((uint8_t *) readbuffer);
	LOG_AT_DEBUG(("MQTTOperation: Reading boot status: [%s]\r\n", readbuffer));

	if ((strncmp(readbuffer, BOOT_PENDING, strlen(BOOT_PENDING)) == 0)) {
		if (xQueueSend(commandQueue,(char *) "511,DUMMY,restartConfirm", 0) != pdTRUE) {
			LOG_AT_ERROR(("MQTTOperation: Could not buffer command!\r\n"));
		}
	}


	uint32_t measurementCounter = 0;
	/* A function that implements a task must not exit or attempt to return to
	 its caller function as there is nothing to return to. */
	while (1) {

		/* Check whether the WLAN network connection is available */
		retcode = MQTTOperation_ValidateWLANConnectivity();
		if (assetStreamBuffer.length > NUMBER_UINT32_ZERO) {
			if (RETCODE_OK == retcode) {
				BaseType_t semaphoreResult = xSemaphoreTake(
						semaphoreAssetBuffer, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT));
				if (pdPASS == semaphoreResult) {
					LOG_AT_DEBUG(
							("MQTTOperation: Publishing asset data: length [%ld], content:\r\n%s", assetStreamBuffer.length, assetStreamBuffer.data));
					MqttPublishAssetInfo.Payload = assetStreamBuffer.data;
					MqttPublishAssetInfo.PayloadLength =
							assetStreamBuffer.length;
					retcode = MQTT_PublishToTopic_Z(&MqttPublishAssetInfo,
					MQTT_PUBLISH_TIMEOUT_IN_MS);
					memset(assetStreamBuffer.data, 0x00,
							assetStreamBuffer.length);
					assetStreamBuffer.length = NUMBER_UINT32_ZERO;
				}
				xSemaphoreGive(semaphoreAssetBuffer);

				if (RETCODE_OK != retcode) {
					LOG_AT_ERROR(("MQTTOperation: MQTT publish failed \r\n"));
					Retcode_RaiseError(retcode);
					errorCountPublish++;
				}

				if (assetUpdateProcess == APP_ASSET_PUBLISHED
						&& RETCODE_OK == retcode) {
					// wait an extra tick rate until topic are created in Cumulocity
					// topics are only created after the device is created
					vTaskDelay(pdMS_TO_TICKS(1000));
					retcode = MQTTOperation_SubscribeTopics();
					if (RETCODE_OK != retcode) {
						LOG_AT_ERROR(
								("MQTTOperation: MQTT subscription failed!\r\n"));
						retcode = MQTTOperation_ValidateWLANConnectivity();
					} else {
						assetUpdateProcess = APP_ASSET_COMPLETED;
					}
				}
			}
		}

		if (sensorStreamBuffer.length > NUMBER_UINT32_ZERO) {
			AppController_SetAppStatus(APP_STATUS_OPERATING_STARTED);
			if (RETCODE_OK == retcode) {
				BaseType_t semaphoreResult = xSemaphoreTake(
						semaphoreSensorBuffer,
						pdMS_TO_TICKS(SEMAPHORE_TIMEOUT));
				if (pdPASS == semaphoreResult) {
					measurementCounter++;
					LOG_AT_DEBUG(
							("MQTTOperation: Publishing sensor data: length [%ld], message [%lu], content:\r\n%s", sensorStreamBuffer.length, measurementCounter, sensorStreamBuffer.data));
					MqttPublishDataInfo.Payload = sensorStreamBuffer.data;
					MqttPublishDataInfo.PayloadLength =
							sensorStreamBuffer.length;
					retcode = MQTT_PublishToTopic_Z(&MqttPublishDataInfo,
							MQTT_PUBLISH_TIMEOUT_IN_MS);
					memset(sensorStreamBuffer.data, 0x00,
							sensorStreamBuffer.length);
					sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
					if (RETCODE_OK != retcode) {
						LOG_AT_ERROR(
								("MQTTOperation: MQTT publish failed trying to ignore\r\n"));
						retcode = RETCODE_OK;
					}

				}
				xSemaphoreGive(semaphoreSensorBuffer);

				if (RETCODE_OK != retcode) {
					LOG_AT_ERROR(("MQTTOperation: MQTT publish failed \r\n"));
					Retcode_RaiseError(retcode);
					errorCountPublish++;
				}

			} else {
				// ignore previous measurements in order to prevent buffer overrun
				memset(sensorStreamBuffer.data, 0x00,
						sensorStreamBuffer.length);
				sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
			}
		}

		char commandBuffer[SIZE_XSMALL_BUF] = { 0 };
		// test if some commands are pending
		if (uxQueueMessagesWaiting(commandQueue) > 0
				&& commandProgress == DEVICE_OPERATION_WAITING) {
			if (xQueueReceive(commandQueue, &commandBuffer, 0) != pdTRUE)
				LOG_AT_ERROR(
						("MQTTOperation: Could read command from buffer!\r\n"));
			else {
				commandProgress = DEVICE_OPERATION_BEFORE_EXECUTING;
				LOG_AT_DEBUG(
						("MQTTOperation: Execute command from buffer: [%s]!\r\n", commandBuffer));
				MQTTOperation_ExecuteCommand(commandBuffer);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(MINIMAL_SPEED));
	}

}

/**
 * @brief starts the data streaming timer
 *
 * @return NONE
 */
static void MQTTOperation_StartTimer(void) {
	LOG_AT_INFO(("MQTTOperation: Start publishing: ...\r\n"));
	xTimerStart(timerHandleSensor, UINT32_C(0xffff));
	AppController_SetAppStatus(APP_STATUS_OPERATING_STARTED);
	return;
}
/**
 * @brief stops the data streaming timer
 *
 * @return NONE
 */
static void MQTTOperation_StopTimer(void) {
	LOG_AT_INFO(("MQTTOperation: Stopped publishing!\r\n"));
	xTimerStop(timerHandleSensor, UINT32_C(0xffff));
	AppController_SetAppStatus(APP_STATUS_OPERATING_STOPPED);
	return;
}

static Retcode_T MQTTOperation_SubscribeTopics(void) {
	Retcode_T retcode;
	retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeCommandInfo,
	MQTT_SUBSCRIBE_TIMEOUT_IN_MS);

	if (RETCODE_OK != retcode) {
		LOG_AT_ERROR(
				("MQTTOperation: MQTT subscribe command topic failed\r\n"));
	} else {
		LOG_AT_TRACE(("MQTTOperation: MQTT subscribe command topic successful\r\n"));

		retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeRestartInfo,
		MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
		retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeErrorInfo,
		MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
		if (RETCODE_OK != retcode) {
			LOG_AT_ERROR(
					("MQTTOperation: MQTT subscribe restart topic failed \r\n"));
		} else {
			LOG_AT_TRACE(("MQTTOperation: MQTT subscribe restart topic successful\r\n"));
		}
	}

	return retcode;
}

/**
 * @brief This will validate the WLAN network connectivity
 *
 * If there is no connectivity then it will scan for the given network and try to reconnect
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */

static Retcode_T MQTTOperation_ValidateWLANConnectivity(void) {
	Retcode_T retcode = RETCODE_OK;
	WlanNetworkConnect_IpStatus_T nwStatus;

	nwStatus = WlanNetworkConnect_GetIpStatus();
	if (WLANNWCT_IPSTATUS_CT_AQRD != nwStatus) {
		AppController_SetAppStatus(APP_STATUS_ERROR);

		// reset WLAN and connect to MQTT broker
		if (MqttSetupInfo.IsSecure == true) {
			static bool isSntpDisabled = false;
			if (false == isSntpDisabled) {
				retcode = SNTP_Disable();
			}
			if (RETCODE_OK == retcode) {
				isSntpDisabled = true;
				retcode = WLAN_Reconnect();
			} else {
				// reconnecting might have failed since the connection is still active try to continue anyway
				retcode = RETCODE_OK;
			}
			if (RETCODE_OK == retcode) {
				retcode = SNTP_Enable();
			}
		} else {
			retcode = WLAN_Reconnect();
		}
	}

	/* Check for MQTT broker connection */
	retcode = MQTT_IsConnected_Z();
	if (RETCODE_OK != retcode) {

		AppController_SetAppStatus(APP_STATUS_ERROR);
		// increase connect attemps
		connectAttemps = connectAttemps + 1;

		retcode = MQTT_ConnectToBroker_Z(&MqttConnectInfo,
		MQTT_CONNECT_TIMEOUT_IN_MS, &MqttCredentials);
		if (RETCODE_OK == retcode) {
			retcode = MQTTOperation_SubscribeTopics();
		}

		if (RETCODE_OK != retcode) {
			LOG_AT_ERROR(
					("MQTTOperation: MQTT connection to the broker failed, try again : [%hu] ... \r\n", connectAttemps ));
			vTaskDelay(pdMS_TO_TICKS(3000));
		} else {
			//reset connection counter
			connectAttemps = 0L;
		}
	}

	// test if we have to reboot
	if (connectAttemps > 10) {
		LOG_AT_WARNING(
				("MQTTOperation: Now calling SoftReset and reboot to recover\r\n"));
		// wait one minute before reboot
		vTaskDelay(pdMS_TO_TICKS(30000));
		BSP_Board_SoftReset();
	}

	return retcode;
}

static void MQTTOperation_PrepareAssetUpdate() {
	assetStreamBuffer.length += snprintf(
			assetStreamBuffer.data + assetStreamBuffer.length,
			sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
			"113,\"%s=%i\n%s=%i\n%s=%i\n%s=%i\n%s=%i\n%s=%i\n%s=%i\"\r\n",
			ATT_KEY_NAME[8], tickRateMS, ATT_KEY_NAME[9],
			MQTTCfgParser_IsAccelEnabled(), ATT_KEY_NAME[10],
			MQTTCfgParser_IsGyroEnabled(), ATT_KEY_NAME[11],
			MQTTCfgParser_IsMagnetEnabled(), ATT_KEY_NAME[12],
			MQTTCfgParser_IsEnvEnabled(), ATT_KEY_NAME[13],
			MQTTCfgParser_IsLightEnabled(), ATT_KEY_NAME[14],
			MQTTCfgParser_IsNoiseEnabled());
}

/**
 * @brief Read the sensors and fill the stream data buffer
 *
 * @param[in] pvParameters - UNUSED
 *
 * @return NONE
 */
static void MQTTOperation_AssetUpdate(xTimerHandle xTimer) {
	(void) xTimer;

	// counter to send every 60 seconds a keep alive msg.
	static uint32_t keepAlive = 0;
	static uint32_t mvoltage = 0, battery = 0;

	//LOG_AT_TRACE(("MQTTOperation: Starting buffering device data ...\r\n"));

	// take semaphore to avoid publish thread to access the buffer
	BaseType_t semaphoreResult = xSemaphoreTake(semaphoreAssetBuffer,
			pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_NULL));
	if (pdPASS == semaphoreResult) {

		switch (assetUpdateProcess) {
		case APP_ASSET_INITIAL:
			assetUpdateProcess = APP_ASSET_PUBLISHED;
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"100,\"%s\",c8y_XDKDevice\r\n", MqttConnectInfo.ClientId);

			char readbuffer[SIZE_SMALL_BUF]; /* Temporary buffer for write file */
			Utils_GetXdkVersionString((uint8_t *) readbuffer);
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"110,%s,XDK,%s\r\n", MqttConnectInfo.ClientId, readbuffer);
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"114,c8y_Restart,c8y_Message,c8y_Command,c8y_Firmware\r\n");
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"115,%s,%s,%s\r\n", MQTTCfgParser_GetFirmwareName(),
					MQTTCfgParser_GetFirmwareVersion(),
					MQTTCfgParser_GetFirmwareURL());
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"117,5\r\n");
			MQTTOperation_PrepareAssetUpdate();
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"400,xdk_StartEvent,\"XDK started!\"\r\n");
			break;
		case APP_ASSET_WAITING:
			switch (command) {
			case CMD_FIRMWARE:
				assetUpdateProcess = APP_ASSET_COMPLETED;
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length, "115,%s,%s,%s\r\n",
						MQTTCfgParser_GetFirmwareName(),
						MQTTCfgParser_GetFirmwareVersion(),
						MQTTCfgParser_GetFirmwareURL());
				assetStreamBuffer.length +=
						snprintf(
								assetStreamBuffer.data
										+ assetStreamBuffer.length,
								sizeof(assetStreamBuffer.data)
										- assetStreamBuffer.length,
								"400,xdk_FirmwareChangeEvent,\"Firmware updated!\"\r\n");
				break;
			default:
				assetUpdateProcess = APP_ASSET_COMPLETED;
				MQTTOperation_PrepareAssetUpdate();
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length,
						"400,xdk_ConfigChangeEvent,\"Config changed!\"\r\n");
				break;
			}
			break;
		default:
			break;
		}

		switch (commandProgress) {
		case DEVICE_OPERATION_BEFORE_EXECUTING:
			// if restart is triggered nothing else can be initiated
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"501,%s\r\n", commands[command]);
			if (command != CMD_RESTART) {
				commandProgress = DEVICE_OPERATION_EXECUTING;
			} else {
				commandProgress = DEVICE_OPERATION_BLOCKING;
			}
			break;
		case DEVICE_OPERATION_BEFORE_FAILED:
			commandProgress = DEVICE_OPERATION_FAILED;
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"501,%s\r\n", commands[command]);
			break;
		case DEVICE_OPERATION_FAILED:
			commandProgress = DEVICE_OPERATION_WAITING;
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"502,%s,\"Command unknown\"\r\n", commands[command]);
			break;
		case DEVICE_OPERATION_EXECUTING:
			commandProgress = DEVICE_OPERATION_WAITING;
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"503,%s\r\n", commands[command]);
			break;
		case DEVICE_OPERATION_IMMEDIATE_CMD:
			commandProgress = DEVICE_OPERATION_WAITING;
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"501,%s\r\n", commands[command]);
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"503,%s\r\n", commands[command]);

			switch (command) {
			case CMD_PUBLISH_START:
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length,
						"400,xdk_StatusChangeEvent,\"Publish started!\"\r\n");
				break;
			case CMD_PUBLISH_STOP:
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length,
						"400,xdk_StatusChangeEvent,\"Publish stopped!\"\r\n");
				break;
			default:
				break;
			}
			break;
		case DEVICE_OPERATION_IMMEDIATE_BUTTON:
			commandProgress = DEVICE_OPERATION_WAITING;

			switch (command) {
			case CMD_PUBLISH_START:
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length,
						"400,xdk_StatusChangeEvent,\"Publish started!\"\r\n");
				break;
			case CMD_PUBLISH_STOP:
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length,
						"400,xdk_StatusChangeEvent,\"Publish stopped!\"\r\n");
				break;
			case CMD_REQUEST:
				assetStreamBuffer.length += snprintf(
						assetStreamBuffer.data + assetStreamBuffer.length,
						sizeof(assetStreamBuffer.data)
								- assetStreamBuffer.length, "500\r\n");
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		// send keep alive message every 60 seconds
		keepAlive++;
		if (keepAlive >= 60) {
			BatteryMonitor_MeasureSignal(&mvoltage);
			// Max = 4.3V, Min = 3.3V
			battery = (mvoltage - 3300.0) / 1000.0 * 100.0;
			assetStreamBuffer.length += snprintf(
					assetStreamBuffer.data + assetStreamBuffer.length,
					sizeof(assetStreamBuffer.data) - assetStreamBuffer.length,
					"212,%ld\r\n", battery);
			keepAlive = 0;

			uint64_t sntpTimeStamp = 0UL;
			uint32_t timeLapseInMs = 0UL;
			SNTP_GetTimeFromSystem(&sntpTimeStamp, &timeLapseInMs);

			struct tm time;
			char timezoneISO8601format[40] = { 0 };
			TimeStamp_SecsToTm(sntpTimeStamp, &time);
			TimeStamp_TmToIso8601(&time, timezoneISO8601format, 40);

			LOG_AT_TRACE(("MQTTOperation: current time: %s\r\n", timezoneISO8601format));

			assetStreamBuffer.length +=
					snprintf(assetStreamBuffer.data + assetStreamBuffer.length,
							sizeof(assetStreamBuffer.data)
									- assetStreamBuffer.length,
							"400,xdk_ErrorCountEvent,\"Errors: Collision Semaphore/Error Publish:%i/%i!\"\r\n",
							errorCountSemaphore, errorCountPublish);

#if INCLUDE_uxTaskGetStackHighWaterMark
			uint32_t everFreeHeap = xPortGetMinimumEverFreeHeapSize();
			uint32_t freeHeap = xPortGetFreeHeapSize();
			//UBaseType_t stackHighWaterMark = 0;
			UBaseType_t stackHighWaterMarkApp = uxTaskGetStackHighWaterMark(AppControllerHandle);
			UBaseType_t stackHighWaterMarkMain = uxTaskGetStackHighWaterMark(MainCmdProcessor.task);
			printf("MQTTOperation_SensorUpdate: Memory stat: everFreeHeap:[%lu], freeHeap:[%lu], stackHighWaterMarkApp:[%lu],stackHighWaterMarkMain:[%lu]\r\n", everFreeHeap, freeHeap, stackHighWaterMarkApp, stackHighWaterMarkMain);
#endif

		}

	}

	// release semaphore and let publish thread access the buffer
	xSemaphoreGive(semaphoreAssetBuffer);

	//LOG_AT_TRACE(("MQTTOperation: Finished buffering device data\r\n"));
}

static void MQTTOperation_SensorUpdate(xTimerHandle xTimer) {
	(void) xTimer;

	Sensor_Value_T sensorValue;
	Retcode_T retcode = Sensor_GetData(&sensorValue);

	//printf("MQTTOperation: Before Semi ... \r\n");
	BaseType_t semaphoreResult = xSemaphoreTake(semaphoreSensorBuffer,
			pdMS_TO_TICKS(SEMAPHORE_TIMEOUT));
	if (pdPASS == semaphoreResult) {
		//printf("MQTTOperation: In Semi ... \r\n");

#if ENABLE_SENSOR_TOOLBOX
		// update inventory with latest measurements
		Orientation_EulerData_T eulerValueInDegree = { 0.0F, 0.0F, 0.0F, 0.0F };
		retcode = Orientation_readEulerRadianVal(&eulerValueInDegree);

		if (retcode == RETCODE_SUCCESS) {

			// update orientation
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1990,%s,%.3lf,%.3lf,%.3lf,%.3lf\r\n",
					MqttConnectInfo.ClientId, eulerValueInDegree.heading,
					eulerValueInDegree.pitch, eulerValueInDegree.roll,
					eulerValueInDegree.yaw);
		}
#endif

		if (SensorSetup.Enable.Accel) {
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"991,,%.3lf,%.3lf,%.3lf\r\n", sensorValue.Accel.X / 1000.0,
					sensorValue.Accel.Y / 1000.0, sensorValue.Accel.Z / 1000.0);
			// update inventory with latest measurements
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1991,%s,%.3lf,%.3lf,%.3lf\r\n", MqttConnectInfo.ClientId,
					sensorValue.Accel.X / 1000.0, sensorValue.Accel.Y / 1000.0,
					sensorValue.Accel.Z / 1000.0);
		}
		if (SensorSetup.Enable.Gyro) {
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"992,,%ld,%ld,%ld\r\n", sensorValue.Gyro.X,
					sensorValue.Gyro.Y, sensorValue.Gyro.Z);
			// update inventory with latest measurements
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1992,%s,%ld,%ld,%ld\r\n", MqttConnectInfo.ClientId,
					sensorValue.Gyro.X, sensorValue.Gyro.Y, sensorValue.Gyro.Z);
		}
		if (SensorSetup.Enable.Mag) {

			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"993,,%ld,%ld,%ld\r\n", sensorValue.Mag.X,
					sensorValue.Mag.Y, sensorValue.Mag.Z);
			// update inventory with latest measurements
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1993,%s,%ld,%ld,%ld\r\n", MqttConnectInfo.ClientId,
					sensorValue.Mag.X, sensorValue.Mag.Y, sensorValue.Mag.Z);
		}
		if (SensorSetup.Enable.Light) {
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"994,,%.2lf\r\n", sensorValue.Light / 1000.0);
			// update inventory with latest measurements
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1994,%s,%.2lf\r\n", MqttConnectInfo.ClientId,
					sensorValue.Light / 1000.0);
		}
		// only all three at the same time can be enabled
		if (SensorSetup.Enable.Temp) {
			// update inventory with latest measurements
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"995,,%ld\r\n", sensorValue.RH);

			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1995,%s,%ld\r\n", MqttConnectInfo.ClientId,
					sensorValue.RH);

			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"996,,%.2lf\r\n", sensorValue.Temp / 972.3);

			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1996,%s,%.2lf\r\n", MqttConnectInfo.ClientId,
					sensorValue.Temp / 972.3);

			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"997,,%.2lf\r\n", sensorValue.Pressure / 100.0);

			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1997,%s,%.2lf\r\n", MqttConnectInfo.ClientId,
					sensorValue.Pressure / 100.0);
		}

		if (SensorSetup.Enable.Noise) {
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"998,,%.4f\r\n",
					MQTTOperation_CalcSoundPressure(sensorValue.Noise));
			// update inventory with latest measurements
			sensorStreamBuffer.length += snprintf(
					sensorStreamBuffer.data + sensorStreamBuffer.length,
					sizeof(sensorStreamBuffer.data) - sensorStreamBuffer.length,
					"1998,%s,%.4f\r\n", MqttConnectInfo.ClientId,
					MQTTOperation_CalcSoundPressure(sensorValue.Noise));
		}
	} else
		errorCountSemaphore++;
	//	printf("MQTTOperation: Sorry Semi ... \r\n");
	xSemaphoreGive(semaphoreSensorBuffer);

}

static float MQTTOperation_CalcSoundPressure(float acousticRawValue) {
	return (acousticRawValue / aku340ConversionRatio);
}

/* global functions ********************************************************* */

/**
 * @brief stops the data streaming timer
 *
 * @return NONE
 */
void MQTTOperation_QueueCommand(void * param1, uint32_t param2) {
	BCDS_UNUSED(param2);
	if (xQueueSend(commandQueue,(char *) param1, 0) != pdTRUE) {
		LOG_AT_ERROR(("MQTTOperation: Could not buffer command!\r\n"));
	}
	return;
}

/**
 * @brief Initializes the MQTT Paho Client, set up subscriptions and initializes the timers and tasks
 *
 * @return NONE
 */
void MQTTOperation_Init(void* pvParameters) {
	BCDS_UNUSED(pvParameters);

	Retcode_T retcode = RETCODE_OK;
	tickRateMS = (int) pdMS_TO_TICKS(MQTTCfgParser_GetStreamRate());

	if (MqttSetupInfo.IsSecure == true) {
		retcode = AppController_SyncTime();
	}

	if (RETCODE_OK == retcode) {
		/*Connect to mqtt broker */
		do {
			retcode = MQTT_ConnectToBroker_Z(&MqttConnectInfo,
			MQTT_CONNECT_TIMEOUT_IN_MS, &MqttCredentials);
			if (RETCODE_OK != retcode) {
				LOG_AT_ERROR(
						("MQTTOperation: MQTT connection to broker failed [%hu] time, try again ... \r\n", connectAttemps ));
				connectAttemps++;
			}
		} while (RETCODE_OK != retcode && connectAttemps < 10);

		connectAttemps = 0UL;
	}

	if (RETCODE_OK == retcode) {
		LOG_AT_INFO(
				("MQTTOperation: Successfully connected to [%s:%d]\r\n", MqttConnectInfo.BrokerURL, MqttConnectInfo.BrokerPort));
		MQTTOperation_ClientPublish();
	} else {
		//reboot to recover
		LOG_AT_WARNING(
				("MQTTOperation: Now calling SoftReset and reboot to recover\r\n"));
		//MQTTOperation_DeInit();
		AppController_SetAppStatus(APP_STATUS_ERROR);
		// wait one minute before reboot
		vTaskDelay(pdMS_TO_TICKS(30000));
		BSP_Board_SoftReset();
	}
}

/**
 * @brief Disconnect client from the MQTT broker
 *
 * @return NONE
 */
void MQTTOperation_DeInit(void) {
	LOG_AT_DEBUG(("MQTTOperation: Calling DeInit\r\n"));
	MQTT_UnSubsribeFromTopic_Z(&MqttSubscribeCommandInfo,
	MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS);
	MQTT_UnSubsribeFromTopic_Z(&MqttSubscribeRestartInfo,
	MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS);
	MQTT_UnSubsribeFromTopic_Z(&MqttSubscribeErrorInfo,
	MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS);
	//ignore return code
	Retcode_T retcode = RETCODE_OK;

	retcode = Mqtt_DisconnectFromBroker_Z();
	if (retcode != RETCODE_OK) {
		LOG_AT_DEBUG(("MQTTOperation: Error diconnecting from Broker\r\n"));
		Retcode_RaiseError(retcode);
	}
}
