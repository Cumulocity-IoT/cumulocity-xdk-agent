/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTRegistration.c
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
#include "MQTTRegistration.h"
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
#include "math.h"
#include "XDK_SNTP.h"
#include "XDK_WLAN.h"

/* constant definitions ***************************************************** */

/* local variables ********************************************************** */

// Client Task/Timer Variables
static xTimerHandle clientRegistrationTimerHandle = NULL; // timer handle for data stream

// Subscribe topics variables
static char appIncomingMsgTopicBuffer[SIZE_SMALL_BUF];/**< Incoming message topic buffer */
static char appIncomingMsgPayloadBuffer[SIZE_LARGE_BUF];/**< Incoming message payload buffer */
static uint16_t connectAttemps = 0UL;

/* global variables ********************************************************* */
extern AssetDataBuffer assetStreamBuffer;
extern MQTT_Setup_TZ MqttSetupInfo;
extern MQTT_Connect_TZ MqttConnectInfo;
extern MQTT_Credentials_TZ MqttCredentials;

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
static void MQTTRegistration_ClientReceive(MQTT_SubscribeCBParam_TZ param);
static void MQTTRegistration_ClientPublish(void );
static void MQTTRegistration_StartRestartTimer(int period);
static void MQTTRegistration_RestartCallback(xTimerHandle xTimer);
static Retcode_T MQTTRegistration_ValidateWLANConnectivity(bool force);
static void MQTTRegistration_PrepareNextRegistrationMsg (xTimerHandle xTimer);

static MQTT_Subscribe_TZ MqttSubscribeInfo = { .Topic = TOPIC_CREDENTIAL, .QoS =
		1UL, .IncomingPublishNotificationCB = MQTTRegistration_ClientReceive, };/**< MQTT subscribe parameters */

static MQTT_Publish_TZ MqttPublishInfo = { .Topic = TOPIC_REGISTRATION, .QoS =
		1UL, .Payload = NULL, .PayloadLength = 0UL, };/**< MQTT publish parameters */

static void MQTTRegistration_PrepareNextRegistrationMsg (xTimerHandle xTimer){
	(void) xTimer;
	LOG_AT_DEBUG(("MQTTRegistration: requesting credentials ...\n\r"));
	// send empty message to Cumulocity to trigger acceptance of the regitration in WebUI
	assetStreamBuffer.length += snprintf(assetStreamBuffer.data + assetStreamBuffer.length, sizeof (assetStreamBuffer.data) - assetStreamBuffer.length, "  \n");
}

/**
 * @brief callback function for subriptions
 *        toggles LEDS or sets read data flag
 *
 * @param[in] md - received message from the MQTT Broker
 *
 * @return NONE
 */
static void MQTTRegistration_ClientReceive(MQTT_SubscribeCBParam_TZ param) {
	/* Initialize Variables */
	/* sample msg: 70,iotconnected,device_XDK_boot_123,l5vHRc%lFt */

	memset(appIncomingMsgPayloadBuffer, 0, sizeof(appIncomingMsgPayloadBuffer));
	memset(appIncomingMsgTopicBuffer, 0, sizeof(appIncomingMsgTopicBuffer));

	strncpy(appIncomingMsgTopicBuffer, (const char *) param.Topic, fmin(param.TopicLength, (sizeof(appIncomingMsgTopicBuffer) - 1U)));
	strncpy(appIncomingMsgPayloadBuffer, (const char *) param.Payload, fmin(param.PayloadLength , (sizeof(appIncomingMsgPayloadBuffer) - 1U)));

	LOG_AT_DEBUG(("MQTTRegistration: Received new message on topic: %s from upstream: [%s]\n\r",
			appIncomingMsgTopicBuffer, appIncomingMsgPayloadBuffer));
	if ((strncmp(param.Topic, TOPIC_CREDENTIAL, param.TopicLength) == 0)) {
		AppController_SetStatus(APP_STATUS_REGISTERED);

		char username[SIZE_SMALL_BUF] = {0};
		char tenant[SIZE_XSMALL_BUF] = {0};
		char password[SIZE_XSMALL_BUF] = {0};

		//extract password and  username
		int token_pos = 0; //mark position of token
		int command_pos = 0; //mark positon for command which is parsed
		char *token = strtok(appIncomingMsgPayloadBuffer, ",");
		while (token != NULL) {
			LOG_AT_TRACE(("MQTTRegistration: Parsed token: [%s], command_pos: %i token_pos: %i \n\r",
					token, command_pos, token_pos));
			if (token_pos == 0 && strcmp(token, TEMPLATE_STD_CREDENTIALS) == 0) {
				LOG_AT_TRACE(("MQTTRegistration: Correct message type \n\r"));
				command_pos = 1; // mark that we are in a credential notification message
			} else if (command_pos == 1 && token_pos == 1) {
				// found tenant
				snprintf(tenant,SIZE_XSMALL_BUF,token);
				LOG_AT_TRACE(("MQTTRegistration: Found tenant: [%s]\n\r", token));
			} else if (command_pos == 1 && token_pos == 2) {
				// found username
				snprintf(username, SIZE_XSMALL_BUF,"%s/%s", tenant, token);
				LOG_AT_TRACE(("MQTTRegistration: Found username: [%s], [%s]\n\r", token, username));
			} else if (command_pos == 1 && token_pos == 3) {
				snprintf(password, SIZE_XSMALL_BUF, token);
				LOG_AT_TRACE(("MQTTRegistration: Found password: [%s]\n\r", token));
			} else {
				LOG_AT_TRACE(("MQTTRegistration: Wrong message format\n\r"));
				assert(0);
			}
			token = strtok(NULL, ", ");
			token_pos++;
		}

		// append credentials at the end of config.txt
		char credentials[SIZE_SMALL_BUF] = {0};
		snprintf(credentials,SIZE_SMALL_BUF,"\n%s=%s\n%s=%s\n", ATT_KEY_NAME[5], username, ATT_KEY_NAME[6], password );
		MQTTFlash_SDAppendCredentials(credentials);

		MQTTCfgParser_SetMqttUser(username);
		MQTTCfgParser_SetMqttPassword(password);
		MQTTCfgParser_FLWriteConfig();
		MQTTRegistration_StartRestartTimer(2000);
	}

}

static void MQTTRegistration_StartRestartTimer(int period) {
	xTimerHandle timerHandle = xTimerCreate((const char * const ) "Restart Timer", // used only for debugging purposes
			MILLISECONDS(period), // timer period
			pdFALSE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			MQTTRegistration_RestartCallback // static callback function
	);
	xTimerStart(timerHandle, MILLISECONDS(5));
}

static void MQTTRegistration_RestartCallback(xTimerHandle xTimer) {
	(void) xTimer;
	MQTTRegistration_DeInit();
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
static void MQTTRegistration_ClientPublish(void) {

	LOG_AT_DEBUG(("MQTTRegistration: MQTTRegistration_ClientPublish starting \n\r"));
	Retcode_T retcode = RETCODE_OK;
	while (1) {
		/* Resetting / clearing the necessary buffers / variables for re-use */
		if (assetStreamBuffer.length > NUMBER_UINT32_ZERO) {
			/* Check whether the WLAN network connection is available */
			retcode = MQTTRegistration_ValidateWLANConnectivity(false);
			if (RETCODE_OK == retcode) {

				MqttPublishInfo.Payload = assetStreamBuffer.data;
				MqttPublishInfo.PayloadLength = assetStreamBuffer.length;

				retcode = MQTT_PublishToTopic_Z(&MqttPublishInfo,
						MQTT_PUBLISH_TIMEOUT_IN_MS);
				if (RETCODE_OK != retcode) {
					LOG_AT_ERROR(("MQTTRegistration: MQTT publish failed \n\r"));
					retcode = MQTTRegistration_ValidateWLANConnectivity(true);
					Retcode_RaiseError(retcode);
				}
			}
			if (RETCODE_OK != retcode) {
				Retcode_RaiseError(retcode);
			}
			memset(assetStreamBuffer.data, 0x00, SIZE_SMALL_BUF);
			assetStreamBuffer.length = NUMBER_UINT32_ZERO;
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

/* global functions ********************************************************* */

/**
 * @brief starts the data streaming timer
 *
 * @return NONE
 */
void MQTTRegistration_StartTimer(void){

	memset(assetStreamBuffer.data, 0x00, SIZE_SMALL_BUF);
	assetStreamBuffer.length = NUMBER_UINT32_ZERO;
	/* Start the timers */
	LOG_AT_TRACE(("MQTTRegistration: Start publishing ...\n\r"));
	xTimerStart(clientRegistrationTimerHandle, UINT32_MAX);
	AppController_SetStatus(APP_STATUS_REGISTERING);
	return;
}

/**
 * @brief Initializes the MQTT Paho Client, set up subscriptions and initializes the timers and tasks
 *
 * @return NONE
 */
void MQTTRegistration_Init(void) {

	Retcode_T retcode = RETCODE_OK;

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
						("MQTTRegistration: MQTT connection to the broker failed \n\r"));
				connectAttemps++;
			}
		} while (RETCODE_OK != retcode && connectAttemps < 10);

		connectAttemps = 0UL;
	}

	if (RETCODE_OK == retcode) {
		retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeInfo,
				MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
		if (RETCODE_OK != retcode) {
			LOG_AT_ERROR(("MQTTRegistration: MQTT subscribe failed \n\r"));
		}
	}

	if (RETCODE_OK != retcode) {
		/* We raise error and still proceed to publish data periodically */
		Retcode_RaiseError(retcode);
		vTaskSuspend(NULL);
	}

	LOG_AT_INFO(("MQTTRegistration: Successfully connected to [%s:%d]\n\r",
				MqttConnectInfo.BrokerURL, MqttConnectInfo.BrokerPort));

	int tickRate = (int) pdMS_TO_TICKS(MQTT_REGISTRATION_TICKRATE);
	clientRegistrationTimerHandle = xTimerCreate(
			(const char * const ) "Data Stream", tickRate,
			pdTRUE,
			NULL, MQTTRegistration_PrepareNextRegistrationMsg);


	MQTTRegistration_StartTimer();
	MQTTRegistration_ClientPublish();
}

/**
 * @brief Disconnect from the MQTT Client
 *
 * @return NONE
 */
void MQTTRegistration_DeInit(void) {

	LOG_AT_INFO(("MQTTRegistration: Calling DeInit\n\r"));
	MQTT_UnSubsribeFromTopic_Z(&MqttSubscribeInfo,
			MQTT_SUBSCRIBE_TIMEOUT_IN_MS);
	Mqtt_DisconnectFromBroker_Z();
}

/**
 * @brief This will validate the WLAN network connectivity
 *
 * If there is no connectivity then it will scan for the given network and try to reconnect
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
static Retcode_T MQTTRegistration_ValidateWLANConnectivity(bool force) {
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

		if (RETCODE_OK == retcode) {
			retcode = MQTT_ConnectToBroker_Z(&MqttConnectInfo,
					MQTT_CONNECT_TIMEOUT_IN_MS, &MqttCredentials);
			if (RETCODE_OK == retcode) {
				retcode = MQTT_SubsribeToTopic_Z(&MqttSubscribeInfo,
						MQTT_SUBSCRIBE_TIMEOUT_IN_MS);			}
		}

		if (RETCODE_OK != retcode) {
			LOG_AT_ERROR(("MQTTRegistration: MQTT connection to the broker failed, try again : [%hu] ... \r\n", connectAttemps ));
			vTaskDelay(pdMS_TO_TICKS(3000));
		} else {
			//reset connection counter
			connectAttemps = 0L;
		}
	}

	// test if we have to reboot
	if (connectAttemps > 10) {
		LOG_AT_WARNING(("MQTTOperation: Now calling SoftReset and reboot to recover\r\n"));
		//MQTTOperation_DeInit();
		// wait one minute before reboot
		vTaskDelay(pdMS_TO_TICKS(30000));
		BSP_Board_SoftReset();
	}
	return retcode;
}
