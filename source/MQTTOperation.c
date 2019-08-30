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
#include "MQTTFlash.h"
#include "MQTTCfgParser.h"

/* additional interface header files */
#include "BSP_BoardType.h"
#include "BCDS_BSP_LED.h"
#include "BCDS_BSP_Board.h"
#include "BCDS_WlanNetworkConfig.h"
#include "BCDS_WlanNetworkConnect.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "math.h"
#include "Serval_Mqtt.h"
#include <XDK_TimeStamp.h>
#include "XDK_SNTP.h"
#include "XDK_WLAN.h"

/* constant definitions ***************************************************** */

/* local variables ********************************************************** */
// Buffers
static char appIncomingMsgTopicBuffer[SENSOR_SMALL_BUF_SIZE];/**< Incoming message topic buffer */
static char appIncomingMsgPayloadBuffer[SENSOR_LARGE_BUF_SIZE];/**< Incoming message payload buffer */
static int tickRateMS;
static bool deviceRunning = true;
static SensorDataBuffer sensorStreamBuffer;
static AssetDataBuffer assetStreamBuffer;
static DEVICE_OPERATION rebootProgress = DEVICE_OPERATION_WAITING;
static DEVICE_OPERATION commandProgress = DEVICE_OPERATION_WAITING;
static char *commandType;
static uint16_t connectAttemps = 0UL;

/* global variables ********************************************************* */
// Network and Client Configuration
/* global variable declarations */
extern char deviceId[];

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
static void MQTTOperation_ClientReceive(MQTT_SubscribeCBParam_TZ param);
static void MQTTOperation_ClientPublish(void);
static void MQTTOperation_AssetUpdate(void);
static Retcode_T MQTTOperation_SubscribeTopics(void);
static void MQTTOperation_StartRestartTimer(int period);
static void MQTTOperation_StartLEDBlinkTimer(int period);
static void MQTTOperation_ToogleLEDCallback(xTimerHandle xTimer);
static void MQTTOperation_RestartCallback(xTimerHandle xTimer);
static Retcode_T MQTTOperation_ValidateWLANConnectivity(bool force);
static void MQTTOperation_SensorUpdate(void);

static MQTT_Subscribe_TZ MqttSubscribeCommandInfo = { .Topic = TOPIC_DOWNSTREAM_CUSTOM,
		.QoS = MQTT_QOS_AT_MOST_ONE, .IncomingPublishNotificationCB =
				MQTTOperation_ClientReceive, };/**< MQTT subscribe parameters */

static MQTT_Subscribe_TZ MqttSubscribeRestartInfo = { .Topic = TOPIC_DOWNSTREAM_STANDARD,
		.QoS = MQTT_QOS_AT_MOST_ONE, .IncomingPublishNotificationCB =
				MQTTOperation_ClientReceive, };/**< MQTT subscribe parameters */

static MQTT_Publish_TZ MqttPublishAssetInfo = { .Topic = TOPIC_ASSET_STREAM,
		.QoS = MQTT_QOS_AT_MOST_ONE, .Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */

static MQTT_Publish_TZ MqttPublishDataInfo = { .Topic = TOPIC_DATA_STREAM,
		.QoS = MQTT_QOS_AT_MOST_ONE, .Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */

static MQTT_Setup_TZ MqttSetupInfo;
static MQTT_Connect_TZ MqttConnectInfo;
static MQTT_Credentials_TZ MqttCredentials;
static Sensor_Setup_T SensorSetup;

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
	memset(appIncomingMsgTopicBuffer, 0, sizeof(appIncomingMsgTopicBuffer));
	memset(appIncomingMsgPayloadBuffer, 0, sizeof(appIncomingMsgPayloadBuffer));

	strncpy(appIncomingMsgTopicBuffer, (const char *) param.Topic, fmin(param.TopicLength, (sizeof(appIncomingMsgTopicBuffer) - 1U)));
	strncpy(appIncomingMsgPayloadBuffer, (const char *) param.Payload, fmin(param.PayloadLength , (sizeof(appIncomingMsgPayloadBuffer) - 1U)));

	printf("MQTTOperation: Topic: %.*s, Msg Received: %.*s\n\r",
			(int) param.TopicLength, appIncomingMsgTopicBuffer,
			(int) param.PayloadLength, appIncomingMsgPayloadBuffer);

	if ((strncmp(appIncomingMsgTopicBuffer, TOPIC_DOWNSTREAM_CUSTOM,
			strlen(TOPIC_DOWNSTREAM_CUSTOM)) == 0)) {
		printf("MQTTOperation: Received command \n\r");
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y,
				(uint32_t) BSP_LED_COMMAND_TOGGLE);
		if (commandProgress == DEVICE_OPERATION_WAITING) {
			commandProgress = DEVICE_OPERATION_BEFORE_EXECUTING;
			commandType = "c8y_Message";
		}
	} else if ((strncmp(appIncomingMsgTopicBuffer, TOPIC_DOWNSTREAM_STANDARD,
			strlen(TOPIC_DOWNSTREAM_STANDARD)) == 0)) {
		// split payload into tokens
		int pos = 0;
		int phase = 0;
		C8Y_COMMAND command;
		char *token = strtok(appIncomingMsgPayloadBuffer, ",:");

		while (token != NULL) {
			printf("MQTTOperation: Processing token: [%s], phase: %i, position: %i \n\r",
					token, phase, pos);

		    switch (pos) {
		    case 0:
				if (strcmp(token, "510") == 0) {
						printf("MQTTOperation: Starting restart \n\r");
						MQTTOperation_StartLEDBlinkTimer(2000);
						MQTTOperation_StartLEDBlinkTimer(4000);
						// set flag so that XDK acknowledges reboot command
						if (rebootProgress == DEVICE_OPERATION_WAITING) {
							rebootProgress = DEVICE_OPERATION_EXECUTING;
							assetStreamBuffer.length += sprintf(
									assetStreamBuffer.data + assetStreamBuffer.length,
									"501,c8y_Restart\n\r");
						} else {
							assetStreamBuffer.length += sprintf(
									assetStreamBuffer.data + assetStreamBuffer.length,
									"502,c8y_Restart\n\r");
						}
						commandType = "c8y_Restart";
						command = CMD_RESTART;
						MQTTOperation_StartRestartTimer(REBOOT_DELAY);
						printf("MQTTOperation: Ending restart\n\r");
				} else if (strcmp(token, "511") == 0) {
						phase = 1;  // mark that we are in a command
						if (commandProgress == DEVICE_OPERATION_WAITING) {
							commandProgress = DEVICE_OPERATION_BEFORE_EXECUTING;
						}
						commandType = "c8y_Command";
				}
		        break;
			case 1:
				//do nothing, ignore the device ID
				break;
		    case 2:
				if (strcmp(token, "speed") == 0 && phase == 1) {
						phase = 2;  // prepare to read the speed
						printf("MQTTOperation: Phase command speed: %i position: %i\n\r", phase,pos);
						command = CMD_SPEED;
				} else if (strcmp(token, "toggle") == 0 && phase == 1) {
						BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_TOGGLE);
						command = CMD_TOGGLE;
						printf("MQTTOperation: Phase command toggle: %i position: %i\n\r", phase,pos);
				} else {
						commandProgress = DEVICE_OPERATION_BEFORE_FAILED;
						printf("MQTTOperation: Unknown command: %s\n\r", token);
				}
		        break;
			case 3:
				if (phase == 2) {
					if (command == CMD_SPEED){
						printf("MQTTOperation: Phase execute: %i position: %i\n\r", phase,
								pos);
						int speed = strtol(token, (char **) NULL, 10);
						printf("MQTTOperation: New speed: %i\n\r", speed);
						tickRateMS = (int) pdMS_TO_TICKS(speed);
						MQTTOperation_AssetUpdate();

					}
				}
		        break;
		    default:
		        printf("MQTTOperation: Error parsing command!\n\r");
		        break;
		    }
			token = strtok(NULL, ", ");
			pos++;
		}

	}

}

static void MQTTOperation_StartLEDBlinkTimer(int period) {
	xTimerHandle timerHandle = xTimerCreate((const char * const ) "LED Timer", // used only for debugging purposes
			MILLISECONDS(period), // timer period
			pdFALSE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			MQTTOperation_ToogleLEDCallback // static callback function
			);
	xTimerStart(timerHandle, MILLISECONDS(10));
}

static void MQTTOperation_StartRestartTimer(int period) {
	xTimerHandle timerHandle = xTimerCreate(
			(const char * const ) "Restart Timer", // used only for debugging purposes
			MILLISECONDS(period), // timer period
			pdFALSE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			MQTTOperation_RestartCallback // static callback function
			);
	xTimerStart(timerHandle, MILLISECONDS(10));
}

static void MQTTOperation_ToogleLEDCallback(xTimerHandle xTimer) {
	(void) xTimer;
	BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_TOGGLE);
}

static void MQTTOperation_RestartCallback(xTimerHandle xTimer) {
	(void) xTimer;
	printf("MQTTOperation: Now calling SoftReset\n\r");
	MQTTFlash_WriteBootStatus(BOOT_PENDING);
	MQTTOperation_DeInit();
	deviceRunning = false;
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

	Retcode_T retcode = RETCODE_OK;
	bool assetUpdateYetPublished = false;

	// initialize buffers
	memset(sensorStreamBuffer.data, 0x00, SENSOR_LARGE_BUF_SIZE);
	sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
	memset(assetStreamBuffer.data, 0x00, SENSOR_SMALL_BUF_SIZE);
	assetStreamBuffer.length = NUMBER_UINT32_ZERO;
	MQTTOperation_AssetUpdate();

	/* A function that implements a task must not exit or attempt to return to
	 its caller function as there is nothing to return to. */
	while (1) {
		if (deviceRunning) {
			MQTTOperation_SensorUpdate();
			/* Check whether the WLAN network connection is available */
			retcode = MQTTOperation_ValidateWLANConnectivity(false);
			if (DEBUG_LEVEL <= DEBUG)
				printf(	"MQTTOperation: Publishing sensor data length:%ld and content:\n\r%s\n\r",
					sensorStreamBuffer.length, sensorStreamBuffer.data);
			if (RETCODE_OK == retcode) {
				MqttPublishDataInfo.Payload = sensorStreamBuffer.data;
				MqttPublishDataInfo.PayloadLength = sensorStreamBuffer.length;

				retcode = MQTT_PublishToTopic_Z(&MqttPublishDataInfo,
				MQTT_PUBLISH_TIMEOUT_IN_MS);
				if (RETCODE_OK != retcode) {
					printf("MQTTOperation: MQTT publish failed \n\r");
					retcode = MQTTOperation_ValidateWLANConnectivity(true);
					Retcode_RaiseError(retcode);
				}

				memset(sensorStreamBuffer.data, 0x00,
						sensorStreamBuffer.length);
				sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
			} else {
				// ignore previous measurements in order to prevent buffer overrun
				memset(sensorStreamBuffer.data, 0x00,
						sensorStreamBuffer.length);
				sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
			}

			if (assetStreamBuffer.length > NUMBER_UINT32_ZERO) {
				if (RETCODE_OK == retcode) {
					if (DEBUG_LEVEL <= DEBUG)
						printf(	"MQTTOperation: Publishing asset data length:%ld and content:\n\r%s\n\r",
								assetStreamBuffer.length, assetStreamBuffer.data);
					MqttPublishAssetInfo.Payload = assetStreamBuffer.data;
					MqttPublishAssetInfo.PayloadLength =
							assetStreamBuffer.length;

					retcode = MQTT_PublishToTopic_Z(&MqttPublishAssetInfo,
					MQTT_PUBLISH_TIMEOUT_IN_MS);
					if (RETCODE_OK != retcode) {
						printf("MQTTOperation: MQTT publish failed \n\r");
						Retcode_RaiseError(retcode);
					}

					memset(assetStreamBuffer.data, 0x00,
							assetStreamBuffer.length);
					assetStreamBuffer.length = NUMBER_UINT32_ZERO;

					if (!assetUpdateYetPublished) {
						assetUpdateYetPublished = true;
						// wait an extra tick rate until topic are created in Cumulocity
						// topics are only created after the device is created
						vTaskDelay(pdMS_TO_TICKS(5000));
						retcode = MQTTOperation_SubscribeTopics();
						if (RETCODE_OK != retcode) {
							assert(0);
						}
					}
				}

			}
		}
		vTaskDelay(pdMS_TO_TICKS(tickRateMS));
	}

}

/* global functions ********************************************************* */

/**
 * @brief starts the data streaming timer
 *
 * @return NONE
 */
void MQTTOperation_StartTimer(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	/* Start the timers */
	if (DEBUG_LEVEL <= INFO)
		printf("MQTTOperation: Start publishing ...\n\r");
	deviceRunning = true;
	BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_ON);
	return;
}
/**
 * @brief stops the data streaming timer
 *
 * @return NONE
 */
void MQTTOperation_StopTimer(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);
	/* Stop the timers */
	if (DEBUG_LEVEL <= INFO)
		printf("MQTTOperation: Stop publishing!\n\r");
	deviceRunning = false;
	BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_OFF);
	return;
}

static Retcode_T MQTTOperation_SubscribeTopics(void) {
	Retcode_T retcode;
	retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeCommandInfo,
	MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
	if (RETCODE_OK != retcode) {
		printf("MQTTOperation: MQTT subscribe command topic failed\n\r");
	} else {
		printf("MQTTOperation: MQTT subscribe command topic successful\n\r");
	}

	if (RETCODE_OK == retcode) {
		retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeRestartInfo,
		MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
		if (RETCODE_OK != retcode) {
			printf("MQTTOperation: MQTT subscribe restart failed \n\r");
		} else {
			printf("MQTTOperation: MQTT subscribe command successful\n\r");
		}
	}
	return retcode;
}

/**
 * @brief Initializes the MQTT Paho Client, set up subscriptions and initializes the timers and tasks
 *
 * @return NONE
 */
void MQTTOperation_Init(MQTT_Setup_TZ MqttSetupInfo_P,
		MQTT_Connect_TZ MqttConnectInfo_P,
		MQTT_Credentials_TZ MqttCredentials_P, Sensor_Setup_T SensorSetup_P) {

	const TickType_t xDelayMS = 2000;

	/* Initialize Variables */
	MqttSetupInfo = MqttSetupInfo_P;
	MqttConnectInfo = MqttConnectInfo_P;
	MqttCredentials = MqttCredentials_P;
	SensorSetup = SensorSetup_P;

	Retcode_T retcode = RETCODE_OK;
	tickRateMS = (int) pdMS_TO_TICKS(MQTTCfgParser_GetStreamRate());

	if (MqttSetupInfo.IsSecure == true) {

		uint64_t sntpTimeStampFromServer = 0UL;

		/* We Synchronize the node with the SNTP server for time-stamp.
		 * Since there is no point in doing a HTTPS communication without a valid time */
		do {
			retcode = SNTP_GetTimeFromServer(&sntpTimeStampFromServer,
			APP_RESPONSE_FROM_SNTP_SERVER_TIMEOUT);
			if ((RETCODE_OK != retcode) || (0UL == sntpTimeStampFromServer)) {
				printf(
						"MQTTOperation: SNTP server time was not synchronized. Retrying...\r\n");
			}
		} while (0UL == sntpTimeStampFromServer);

		struct tm time;
		char timezoneISO8601format[40];
		TimeStamp_SecsToTm(sntpTimeStampFromServer, &time);
		TimeStamp_TmToIso8601(&time, timezoneISO8601format, 40);

		BCDS_UNUSED(sntpTimeStampFromServer); /* Copy of sntpTimeStampFromServer will be used be HTTPS for TLS handshake */
	}

	if (RETCODE_OK == retcode) {
		/*Connect to mqtt broker */
		do {
			retcode = MQTT_ConnectToBroker_Z(&MqttConnectInfo,
			MQTT_CONNECT_TIMEOUT_IN_MS, &MqttCredentials);
			if (RETCODE_OK != retcode) {
				printf("MQTTOperation: MQTT connection to the broker failed, try again : [%hu] ... \n\r", connectAttemps );
				connectAttemps ++;
			}
		} while (RETCODE_OK != retcode && connectAttemps < 10 );

		connectAttemps = 0UL;
	}

	if (RETCODE_OK == retcode) {
		printf("MQTTOperation: Successfully connected to [%s:%d]\n\r",
					MqttConnectInfo.BrokerURL, MqttConnectInfo.BrokerPort);
		/* Turn ON Orange LED to indicate Initialization Complete */
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_ON);
		vTaskDelay(pdMS_TO_TICKS(xDelayMS));
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O,
				(uint32_t) BSP_LED_COMMAND_OFF);
		vTaskDelay(pdMS_TO_TICKS(xDelayMS));
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_ON);
		MQTTOperation_ClientPublish();
	} else {
		//reboot to recover
		printf("MQTTOperation: Now calling SoftReset and reboot to recover\n\r");
		//MQTTOperation_DeInit();
		BSP_Board_SoftReset();
		//assert(0);
	}
}

/**
 * @brief Disconnect from the MQTT Client
 *
 * @return NONE
 */
void MQTTOperation_DeInit(void) {
	Retcode_T retcode = RETCODE_OK;

	if (DEBUG_LEVEL <= INFO)
		printf("MQTTOperation: Calling DeInit\n\r");
	MQTT_UnSubsribeFromTopic_Z(&MqttSubscribeCommandInfo,
			MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS);
	MQTT_UnSubsribeFromTopic_Z(&MqttSubscribeRestartInfo,
			MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS);
	//ignore return code
	retcode = Mqtt_DisconnectFromBroker_Z();
}

/**
 * @brief This will validate the WLAN network connectivity
 *
 * If there is no connectivity then it will scan for the given network and try to reconnect
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
static Retcode_T MQTTOperation_ValidateWLANConnectivity(bool force) {
	Retcode_T retcode = RETCODE_OK;
	WlanNetworkConnect_IpStatus_T nwStatus;

	nwStatus = WlanNetworkConnect_GetIpStatus();
	if (WLANNWCT_IPSTATUS_CT_AQRD != nwStatus || force) {

		// increase connect attemps
		connectAttemps = connectAttemps + 1;
		// before resetting connection try to disconnect
		// ignore return code
		//retcode = Mqtt_DisconnectFromBroker_Z();
		MQTTOperation_DeInit();

		// reset WLAN and connect to MQTT broker
		if (MqttSetupInfo.IsSecure == true) {
			static bool isSntpDisabled = false;
			if (false == isSntpDisabled) {
				retcode = SNTP_Disable();
			}
			if (RETCODE_OK == retcode) {
				isSntpDisabled = true;
				retcode = WLAN_Reconnect();
			}
			if (RETCODE_OK == retcode) {
				retcode = SNTP_Enable();
			}
		} else {
			retcode = WLAN_Reconnect();
		}

		if (RETCODE_OK == retcode) {
			retcode = MQTT_ConnectToBroker_Z(&MqttConnectInfo,
			MQTT_CONNECT_TIMEOUT_IN_MS, &MqttCredentials);
			if (RETCODE_OK == retcode) {
				retcode = MQTTOperation_SubscribeTopics();
			}
		}
		if (RETCODE_OK != retcode) {
			printf("MQTTOperation: MQTT connection to the broker failed, try again : [%hu] ... \n\r", connectAttemps );
		} else {
			//reset connection counter
			connectAttemps = 0L;
		}

	}

	// test if we have to reboot
	if (connectAttemps > 10) {
		printf("MQTTOperation: Now calling SoftReset and reboot to recover\n\r");
		//MQTTOperation_DeInit();
		BSP_Board_SoftReset();
	}

	return retcode;
}

/**
 * @brief Read the sensors and fill the stream data buffer
 *
 * @param[in] pvParameters - UNUSED
 *
 * @return NONE
 */
static void MQTTOperation_AssetUpdate(void) {
	if (DEBUG_LEVEL <= FINEST)
		printf("MQTTOperation: Starting buffering device data ...\n\r");


	/* Initialize Variables */
	char readbuffer[FILE_TINY_BUFFER_SIZE]; /* Temporary buffer for write file */

	MQTTFlash_ReadBootStatus(readbuffer);
	printf("MQTTOperation: Reading boot status: [%s]\n\r", readbuffer);

	if ((strncmp(readbuffer, BOOT_PENDING, strlen(BOOT_PENDING)) == 0)) {
		printf("MQTTOperation: Confirm successful reboot\n\r");
		assetStreamBuffer.length += sprintf(
				assetStreamBuffer.data + assetStreamBuffer.length,
				"503,c8y_Restart\n\r");
		MQTTFlash_WriteBootStatus(NO_BOOT_PENDING);
	}

	assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length,
			"100,\"%s\",c8y_XDKDevice\n\r", deviceId);
	assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length,
			"114,c8y_Restart,c8y_Message,c8y_Command\n\r");
	assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length,
			"113,\"%s = %ld ms\n%s = %i\n%s = %i\n%s = %i\n%s = %i\n%s = %i\n\"\n\r",
			A9Name, tickRateMS,
			A10Name, MQTTCfgParser_IsAccelEnabled(),
			A11Name, MQTTCfgParser_IsGyroEnabled(),
			A12Name, MQTTCfgParser_IsMagnetEnabled(),
			A13Name, MQTTCfgParser_IsEnvEnabled(),
			A14Name, MQTTCfgParser_IsLightEnabled());

	assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length, "117,5\n\r");

	Utils_GetXdkVersionString(readbuffer);
	assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length,
			"115,XDK,%s\n\r", readbuffer);

	if (DEBUG_LEVEL <= FINEST)
		printf("MQTTOperation: Finished buffering device data\n\r");
}

static void MQTTOperation_SensorUpdate(void) {
	Sensor_Value_T sensorValue;
	Retcode_T retcode = RETCODE_OK;
	if (RETCODE_OK == retcode) {
		retcode = Sensor_GetData(&sensorValue);
	}
	if (SensorSetup.Enable.Accel) {
		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"991,,%ld,%ld,%ld\n\r", sensorValue.Accel.X, sensorValue.Accel.Y,
				sensorValue.Accel.Z);
	}
	if (SensorSetup.Enable.Gyro) {
		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"992,,%ld,%ld,%ld\n\r", sensorValue.Gyro.X, sensorValue.Gyro.Y,
				sensorValue.Gyro.Z);
	}
	if (SensorSetup.Enable.Mag) {

		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"993,,%ld,%ld,%ld\n\r", sensorValue.Mag.X, sensorValue.Mag.Y,
				sensorValue.Mag.Z);
	}
	if (SensorSetup.Enable.Light) {
		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"994,,%ld\n\r", sensorValue.Light);
	}
	// only all three at the same time can be enabled
	if (SensorSetup.Enable.Temp) {
		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"995,,%ld\n\r", sensorValue.RH);
		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"996,,%.2lf\n\r", sensorValue.Temp / 972.3);
		sensorStreamBuffer.length += sprintf(
				sensorStreamBuffer.data + sensorStreamBuffer.length,
				"997,,%ld\n\r", sensorValue.Pressure);
		//sensorStreamBuffer.length += sprintf(sensorStreamBuffer.data + sensorStreamBuffer.length, "998,,%.2f\n", sensorValue.Noise);
	}

	if (commandProgress == DEVICE_OPERATION_BEFORE_EXECUTING) {
		commandProgress = DEVICE_OPERATION_EXECUTING;
		assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length, "501,%s\n\r", commandType);
	} else if (commandProgress == DEVICE_OPERATION_BEFORE_FAILED) {
		commandProgress = DEVICE_OPERATION_FAILED;
		assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length, "501,%s\n\r", commandType);
	} else if (commandProgress == DEVICE_OPERATION_EXECUTING) {
		commandProgress = DEVICE_OPERATION_WAITING;
		assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length, "503,%s\n\r", commandType);
	} else if  (commandProgress == DEVICE_OPERATION_FAILED) {
		commandProgress = DEVICE_OPERATION_WAITING;
		assetStreamBuffer.length += sprintf(
			assetStreamBuffer.data + assetStreamBuffer.length, "502,%s,\"Command unknown\"\n\r",commandType);
	}

}
