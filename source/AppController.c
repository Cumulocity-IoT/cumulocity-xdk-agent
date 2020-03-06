/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	AppController.c
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/* own header files */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* own header files */
#include "AppController.h"

/* system header files */
#include <stdio.h>

/* additional interface header files */
#include "BSP_BoardType.h"
#include "BCDS_BSP_Board.h"
#include "BCDS_BSP_Button.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_Assert.h"
#include "BCDS_BSP_LED.h"
#include "Serval_Ip.h"
#include "XDK_Utils.h"
#include "XDK_WLAN.h"
#include "XDK_MQTT_Z.h"
#include "XDK_Sensor.h"
#include "XDK_SNTP.h"
#include "XDK_ServalPAL.h"
#include "XDK_Storage.h"
#include "XDK_TimeStamp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "Serval_Ip.h"
#include "BatteryMonitor.h"
#include "MQTTStorage.h"
#include "MQTTCfgParser.h"
#include "MQTTRegistration.h"
#include "MQTTOperation.h"
#include "MQTTButton.h"

#include "XdkSensorHandle.h"
#include "BCDS_Orientation.h"

/* constant definitions ***************************************************** */

/* local variables ********************************************************** */

static WLAN_Setup_T WLANSetupInfo = { .IsEnterprise = false, .IsHostPgmEnabled =
		false, .SSID = WLAN_SSID, .Username = WLAN_PSK, .Password = WLAN_PSK,
		.IsStatic = WLAN_STATIC_IP, .IpAddr = WLAN_IP_ADDR, .GwAddr =
				WLAN_GW_ADDR, .DnsAddr = WLAN_DNS_ADDR, .Mask = WLAN_MASK, };

static SNTP_Setup_T SNTPSetupInfo; /**< SNTP setup parameters */

static CmdProcessor_T * AppCmdProcessor; /**< Handle to store the main Command processor handle to be used by run-time event driven threads */

static void AppController_Enable (void *, uint32_t);
static void AppController_SetClientId (const char * clientId);
static void AppController_StartLEDBlinkTimer (int);

/* global variables ********************************************************* */
char clientId[14] = { 0 };  // MAC address 6*2 + \0 'terminating'
APP_STATUS app_status = APP_STATUS_UNKNOWN;
APP_STATUS cmd_status = APP_STATUS_UNKNOWN;
APP_STATUS boot_mode = APP_STATUS_UNKNOWN;
uint16_t connectAttemps = 0UL;
SensorDataBuffer sensorStreamBuffer;
AssetDataBuffer assetStreamBuffer;

xTaskHandle AppControllerHandle = NULL;/**< OS thread handle for Application controller to be used by run-time blocking threads */
MQTT_Setup_TZ MqttSetupInfo;
MQTT_Connect_TZ MqttConnectInfo;
MQTT_Credentials_TZ MqttCredentials;
Storage_Setup_T StorageSetup;
Sensor_Setup_T SensorSetup;

/* inline functions ********************************************************* */

/* local functions ********************************************************** */

/**
 * @brief To setup the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP (if secure communication)
 * - MQTT
 * - Sensor
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppController_Setup(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	Retcode_T retcode = Storage_Setup(&StorageSetup);
	retcode = Storage_Enable();

	if ((Retcode_T) RETCODE_STORAGE_SDCARD_NOT_AVAILABLE
			== Retcode_GetCode(retcode)) {
		/* This is only a warning error. So we will raise and proceed */
		LOG_AT_WARNING(
				("AppController_Setup: SD card missing, using config from WIFI chip!\r\n"));
		retcode = RETCODE_OK; /* SD card was not inserted */
	}

	/* Initialize variables from flash */
	if (MQTTStorage_Init() != RETCODE_OK) {
		assert(0);
	}

	/* Initialize Buttons */
	retcode = MQTTButton_Init(AppCmdProcessor);
	if (retcode != RETCODE_OK) {
		LOG_AT_ERROR(("AppController_Setup: Boot error!\r\n"));
		assert(0);
	}

	//test if button 2 is pressed
	if (BSP_Button_GetState((uint32_t) BSP_XDK_BUTTON_2) == 1) {
		LOG_AT_INFO(
				("AppController_Setup: Button 2 was pressed at startup, deleting config stored on WIFI chip!\r\n"));
		MQTTStorage_Flash_DeleteConfig();
		BSP_Board_SoftReset();
	}

	retcode = MQTTCfgParser_Init();

	// set boot mode: operation or registration
	boot_mode = MQTTCfgParser_GetMode();
	if (retcode != RETCODE_OK) {
		LOG_AT_ERROR(
				("AppController_Setup: Boot error. Inconsistent configuration!\r\n"));
		assert(0);
	}

	if (boot_mode == APP_STATUS_OPERATION_MODE) {
		MqttCredentials.Username = MQTTCfgParser_GetMqttUser();
		MqttCredentials.Password = MQTTCfgParser_GetMqttPassword();
		MqttCredentials.Anonymous = MQTTCfgParser_IsMqttAnonymous();
	} else {
		MqttCredentials.Username = MQTT_REGISTRATION_USERNAME;
		MqttCredentials.Password = MQTT_REGISTRATION_PASSWORD;
		MqttCredentials.Anonymous = FALSE;
	}

	if (RETCODE_OK == retcode) {
		// set cfg parameter for WIFI access
		WLANSetupInfo.SSID = MQTTCfgParser_GetWlanSSID();
		WLANSetupInfo.Username = MQTTCfgParser_GetWlanPassword();
		WLANSetupInfo.Password = MQTTCfgParser_GetWlanPassword();

		retcode = WLAN_Setup(&WLANSetupInfo);
		if (RETCODE_OK == retcode) {
			retcode = ServalPAL_Setup(AppCmdProcessor);
		}

	}

	MqttSetupInfo.IsSecure = MQTTCfgParser_IsMqttSecureEnabled();
	if (MqttSetupInfo.IsSecure == true) {
		if (RETCODE_OK == retcode) {
			SNTPSetupInfo = (SNTP_Setup_T ) { .ServerUrl =
							MQTTCfgParser_GetSntpName(), .ServerPort =
							MQTTCfgParser_GetSntpPort() };

			LOG_AT_INFO(
					("AppController_Setup: SNTP server: [%s:%d]\r\n", SNTPSetupInfo.ServerUrl, SNTPSetupInfo.ServerPort));
			retcode = SNTP_Setup(&SNTPSetupInfo);
		}
	}

	if (RETCODE_OK == retcode) {
		retcode = MQTT_Setup_Z(&MqttSetupInfo);
	}

	if (RETCODE_OK == retcode && boot_mode == APP_STATUS_OPERATION_MODE) {
		SensorSetup.CmdProcessorHandle = AppCmdProcessor;
		SensorSetup.Enable.Accel = MQTTCfgParser_IsAccelEnabled();
		SensorSetup.Enable.Gyro = MQTTCfgParser_IsGyroEnabled();
		SensorSetup.Enable.Humidity = MQTTCfgParser_IsEnvEnabled();
		SensorSetup.Enable.Light = MQTTCfgParser_IsLightEnabled();
		SensorSetup.Enable.Mag = MQTTCfgParser_IsMagnetEnabled();
		SensorSetup.Enable.Pressure = MQTTCfgParser_IsEnvEnabled();
		SensorSetup.Enable.Temp = MQTTCfgParser_IsEnvEnabled();
		SensorSetup.Enable.Noise = MQTTCfgParser_IsNoiseEnabled();
		retcode = Sensor_Setup(&SensorSetup);
#if ENABLE_SENSOR_TOOLBOX
		retcode = Orientation_init(xdkOrientationSensor_Handle);
#endif
	}

	if (RETCODE_OK == retcode) {
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppController_Enable,
				NULL, UINT32_C(0));
	}
	if (RETCODE_OK != retcode) {
		LOG_AT_ERROR(("AppController_Setup: Failed \r\n"));
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

/**
 * @brief To enable the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP (if secure communication)
 * - MQTT
 * - Sensor
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppController_Enable(void * param1, uint32_t param2) {
	BCDS_UNUSED(param1);
	BCDS_UNUSED(param2);

	Retcode_T retcode = WLAN_Enable();
	if (RETCODE_OK == retcode) {
		retcode = ServalPAL_Enable();
	}

	if (MqttSetupInfo.IsSecure == true) {
		if (RETCODE_OK == retcode) {
			retcode = SNTP_Enable();
		}
	}
	if (RETCODE_OK == retcode) {
		retcode = MQTT_Enable_Z();
	}
	if (RETCODE_OK == retcode && boot_mode == APP_STATUS_OPERATION_MODE) {
		retcode = Sensor_Enable();
	}

	MqttConnectInfo.BrokerURL = MQTTCfgParser_GetMqttBrokerName();
	MqttConnectInfo.BrokerPort = MQTTCfgParser_GetMqttBrokerPort();
	MqttConnectInfo.CleanSession = true;
	MqttConnectInfo.KeepAliveInterval = 100;
	AppController_SetClientId(MqttConnectInfo.ClientId);

	LOG_AT_INFO(
			("AppController_Enable: Device ID for registration in Cumulocity %s.\r\n", MqttConnectInfo.ClientId));

	if (RETCODE_OK == retcode) {
		BaseType_t task_result;
		if (boot_mode == APP_STATUS_OPERATION_MODE) {
			task_result = xTaskCreate(MQTTOperation_Init,
					(const char * const ) "AppController", TASK_STACK_SIZE_APP_CONTROLLER, NULL,
					TASK_PRIO_APP_CONTROLLER, &AppControllerHandle);
		} else {
			task_result = xTaskCreate(MQTTRegistration_Init,
					(const char * const ) "AppController", TASK_STACK_SIZE_APP_CONTROLLER, NULL,
					TASK_PRIO_APP_CONTROLLER, &AppControllerHandle);
		}

		if (pdPASS
				!= task_result) {
			LOG_AT_ERROR(
					("AppController_Enable: Now calling SoftReset and reboot to recover\r\n"));
			Retcode_RaiseError(retcode);
			BSP_Board_SoftReset();
		}
	}

	Utils_PrintResetCause();
}

static void AppController_SetClientId(const char * clientId) {
	/* Initialize Variables */
	uint8_t _macVal[WIFI_MAC_ADDR_LEN + 1] = { 0 };
	uint8_t _macAddressLen = WIFI_MAC_ADDR_LEN;

	/* Get the MAC Address */
	sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &_macAddressLen,
			(uint8_t *) _macVal);
	sprintf(clientId, "%02X%02X%02X%02X%02X%02X", _macVal[0],
			_macVal[1], _macVal[2], _macVal[3], _macVal[4], _macVal[5]);
}

static void AppController_ToogleLEDCallback(xTimerHandle xTimer) {
	(void) xTimer;

	switch (app_status) {
	case APP_STATUS_STARTED:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_TOGGLE);
		break;
	case APP_STATUS_OPERATING_STARTED:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_TOGGLE);
		break;
	case APP_STATUS_OPERATING_STOPPED:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_ON);
		break;
	case APP_STATUS_ERROR:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_ON);
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_OFF);
		break;
	case APP_STATUS_REBOOT:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_TOGGLE);
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_TOGGLE);
		break;
	case APP_STATUS_REGISTERED:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_ON);
		break;
	case APP_STATUS_REGISTERING:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_TOGGLE);

		break;
	default:
		LOG_AT_WARNING(("AppController: Unknown app status\n"));
		break;
	}
	LOG_AT_TRACE(("STATUS %s\r\n", app_status_text[app_status]));

	switch (cmd_status) {
	case APP_STATUS_COMMAND_RECEIVED:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_TOGGLE);
		cmd_status = APP_STATUS_COMMAND_CONFIRMED;
		break;
	case APP_STATUS_COMMAND_CONFIRMED:
		BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_TOGGLE);
		cmd_status = APP_STATUS_STARTED;
		break;
	case APP_STATUS_STARTED:
		// do nothing
		break;
	default:
		LOG_AT_WARNING(("AppController: Unknown command status\n"));
		break;
	}
}

static void AppController_StartLEDBlinkTimer(int period) {
	xTimerHandle timerHandle = xTimerCreate((const char * const ) "LED Timer", // used only for debugging purposes
			MILLISECONDS(period), // timer period
			pdTRUE, //Autoreload pdTRUE or pdFALSE - should the timer start again after it expired?
			NULL, // optional identifier
			AppController_ToogleLEDCallback // static callback function
			);
	xTimerStart(timerHandle, UINT32_C(0xffff));
}

Retcode_T AppController_SyncTime() {
	uint64_t sntpTimeStampFromServer = 0UL;
	uint16_t sntpAttemps = 0UL;
	Retcode_T retcode = RETCODE_OK;

	/* We Synchronize the node with the SNTP server for time-stamp.
	 * Since there is no point in doing a HTTPS communication without a valid time */
	do {
		retcode = SNTP_GetTimeFromServer(&sntpTimeStampFromServer,
		APP_RESPONSE_FROM_SNTP_SERVER_TIMEOUT);
		if ((RETCODE_OK != retcode) || (0UL == sntpTimeStampFromServer)) {
			LOG_AT_WARNING(
					("AppController: SNTP server time was not synchronized. Retrying...\r\n"));
		}
		sntpAttemps++;
		//vTaskDelay(pdMS_TO_TICKS(500));
	} while (0UL == sntpTimeStampFromServer && sntpAttemps < 3); // only try to sync time 3 times

	if (0UL == sntpTimeStampFromServer) {
		sntpTimeStampFromServer = 1580515200UL; // use default time 1. Feb 2020 00:00:00 UTC
		SNTP_SetTime(sntpTimeStampFromServer);
		LOG_AT_WARNING(
				("AppController: Using fixed timestamp 1. Feb 2020 00:00:00 UTC, SNTP sync not possible\r\n"));
		retcode = RETCODE_OK; // clear return code
	}

	//uint8_t * timeBuffer = (uint8_t *) &sntpTimeStampFromServer;
	//LOG_AT_TRACE(("AppController: SNTP time: %d,%d,%d,%d,%d,%d,%d,%d\r\n", timeBuffer[0], timeBuffer[1], timeBuffer[2], timeBuffer[3],timeBuffer[4],timeBuffer[5],timeBuffer[6],timeBuffer[7]));
	struct tm time;
	char timezoneISO8601format[40];
	TimeStamp_SecsToTm(sntpTimeStampFromServer, &time);
	TimeStamp_TmToIso8601(&time, timezoneISO8601format, 40);
	BCDS_UNUSED(sntpTimeStampFromServer); /* Copy of sntpTimeStampFromServer will be used be HTTPS for TLS handshake */
	return retcode;
}

/* global functions ********************************************************* */

/** Initialize Application
 *
 */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2) {

	/*
	 * Initialialze global variables
	 */

	MqttSetupInfo = (MQTT_Setup_TZ ) { .IsSecure = APP_MQTT_SECURE_ENABLE, };

	MqttConnectInfo = (MQTT_Connect_TZ ) { .ClientId = clientId, .BrokerURL =
					MQTT_BROKER_HOST_NAME, .BrokerPort =  MQTT_BROKER_HOST_PORT,
					.CleanSession = true, .KeepAliveInterval = 100, };

	MqttCredentials = (MQTT_Credentials_TZ ) { .Username = APP_MQTT_USERNAME,
					.Password = APP_MQTT_PASSWORD, .Anonymous = false,};

	StorageSetup =
			(Storage_Setup_T ) { .SDCard = true, .WiFiFileSystem = true, };

	SensorSetup = (Sensor_Setup_T ) { .CmdProcessorHandle = NULL, .Enable = {
					.Accel = false, .Mag = false, .Gyro = false, .Humidity =
							false, .Temp = false, .Pressure = false, .Light =
							false, .Noise = false, }, .Config = { .Accel = {
					.Type = SENSOR_ACCEL_BMA280, .IsRawData = false,
					.IsInteruptEnabled = false, .Callback = NULL, }, .Gyro = {
					.Type = SENSOR_GYRO_BMG160, .IsRawData = false, }, .Mag = {
					.IsRawData = false }, .Light = { .IsInteruptEnabled = false,
					.Callback = NULL, }, .Temp = { .OffsetCorrection =
					APP_TEMPERATURE_OFFSET_CORRECTION, }, }, };

	Retcode_T retcode = RETCODE_OK;
	BCDS_UNUSED(param2);

	vTaskDelay(pdMS_TO_TICKS(1000));
	LOG_AT_INFO(("AppController_Init: XDK Cumulocity Agent startup ...\r\n"));

	// start status LED indicator
	AppController_SetAppStatus(APP_STATUS_STARTED);
	AppController_SetCmdStatus(APP_STATUS_STARTED);
	AppController_StartLEDBlinkTimer(500);

	// init battery monitor
	BatteryMonitor_Init();

	if (cmdProcessorHandle == NULL) {
		retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
	} else if (RETCODE_OK == retcode) {
		AppCmdProcessor = (CmdProcessor_T *) cmdProcessorHandle;
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppController_Setup,
				NULL, UINT32_C(0));
	}

	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

void AppController_SetAppStatus(uint8_t status) {
	if (app_status != APP_STATUS_REBOOT) {
		app_status = status;
	}
}

uint8_t AppController_GetAppStatus(void) {
	return app_status;
}

void AppController_SetCmdStatus(uint8_t status) {
	cmd_status = status;
}

uint8_t AppController_GetCmdStatus(void) {
	return cmd_status;
}
