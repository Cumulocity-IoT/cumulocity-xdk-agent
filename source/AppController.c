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
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "Serval_Ip.h"
#include "BatteryMonitor.h"
#include "MQTTFlash.h"
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
		WLAN_GW_ADDR, .DnsAddr = WLAN_DNS_ADDR, .Mask = WLAN_MASK, };/**< WLAN setup parameters */

static SNTP_Setup_T SNTPSetupInfo = { .ServerUrl = SNTP_SERVER_URL,
		.ServerPort = SNTP_SERVER_PORT, };/**< SNTP setup parameters */

static CmdProcessor_T * AppCmdProcessor;/**< Handle to store the main Command processor handle to be used by run-time event driven threads */

/* initalize boot progress */
static APP_RESULT rc_Boot_Mode = APP_RESULT_OPERATION_MODE;

static void AppController_Enable(void *, uint32_t);
static void AppController_Fire(void *);
static void AppController_SetClientId(void);
static void AppController_StartLEDBlinkTimer(int);

static char clientId[] = WIFI_DEFAULT_MAC_CLIENTID;


/* global variables ********************************************************* */
APP_STATUS app_status;
APP_STATUS cmd_status;
char deviceId[] = DEVICE_ID;
uint16_t connectAttemps = 0UL;
SensorDataBuffer sensorStreamBuffer;
AssetDataBuffer assetStreamBuffer;

xTaskHandle AppControllerHandle = NULL;/**< OS thread handle for Application controller to be used by run-time blocking threads */
MQTT_Setup_TZ MqttSetupInfo;
MQTT_Connect_TZ MqttConnectInfo;
MQTT_Credentials_TZ MqttCredentials;
Storage_Setup_T	StorageSetup;
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

	Retcode_T  retcode = Storage_Setup(&StorageSetup);
    retcode = Storage_Enable();

    if ((Retcode_T) RETCODE_STORAGE_SDCARD_NOT_AVAILABLE == Retcode_GetCode((retcode)))
    {
        /* This is only a warning error. So we will raise and proceed */
    	LOG_AT_WARNING(("AppController_Setup: SD card missing trying to use config from WIFI chip!\r\n"));
        //Retcode_RaiseError(retcode);
        retcode = RETCODE_OK; /* SD card was not inserted */
    }
	/* Initialize variables from flash */
	if (MQTTFlash_Init() == APP_RESULT_ERROR) {
		retcode = RETCODE(RETCODE_SEVERITY_ERROR,RETCODE_UNEXPECTED_BEHAVIOR);
	}

	/* Initialize Buttons */
	rc_Boot_Mode = MQTTButton_Init(AppCmdProcessor);
	if (rc_Boot_Mode == APP_RESULT_ERROR) {
		LOG_AT_ERROR(("AppController_Setup: Boot error\r\n"));
		retcode = RETCODE(RETCODE_SEVERITY_ERROR,RETCODE_UNEXPECTED_BEHAVIOR);
	}

	if (BSP_Button_GetState((uint32_t) BSP_XDK_BUTTON_2) == 1) {
		LOG_AT_INFO(("AppController_Setup: Button 2 was pressed at startup and delete config stored on WIFI chip!\r\n"));
		MQTTFlash_FLDeleteConfig();
		BSP_Board_SoftReset();
	}

	rc_Boot_Mode = MQTTCfgParser_Init();
	if (rc_Boot_Mode == APP_RESULT_ERROR) {
		retcode = RETCODE(RETCODE_SEVERITY_ERROR,RETCODE_UNEXPECTED_BEHAVIOR);
	}

    if (RETCODE_OK == retcode) {
		// set cfg parameter for WIFI access
		WLANSetupInfo.SSID = MQTTCfgParser_GetWlanSSID();
		WLANSetupInfo.Username = MQTTCfgParser_GetWlanPassword(); // not realy used
		WLANSetupInfo.Password = MQTTCfgParser_GetWlanPassword();
		MqttSetupInfo.IsSecure = MQTTCfgParser_IsMqttSecureEnabled();
		retcode = WLAN_Setup(&WLANSetupInfo);
		if (RETCODE_OK == retcode) {
				retcode = ServalPAL_Setup(AppCmdProcessor);
		}

	}

	if (MqttSetupInfo.IsSecure == true) {
		if (RETCODE_OK == retcode) {
			SNTPSetupInfo.ServerUrl = MQTTCfgParser_GetSntpName();
			SNTPSetupInfo.ServerPort = MQTTCfgParser_GetSntpPort();
			LOG_AT_INFO(("AppController_Setup: SNTP server: [%s:%d]\r\n", SNTPSetupInfo.ServerUrl, SNTPSetupInfo.ServerPort));
			retcode = SNTP_Setup(&SNTPSetupInfo);
		}
	}
	if (RETCODE_OK == retcode) {
		retcode = MQTT_Setup_Z(&MqttSetupInfo);
	}
	if (RETCODE_OK == retcode && rc_Boot_Mode == APP_RESULT_OPERATION_MODE) {
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
	//delay start
	vTaskDelay(pdMS_TO_TICKS(2000));


	if (RETCODE_OK == retcode) {
		retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppController_Enable, NULL, UINT32_C(0));
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
	if (RETCODE_OK == retcode && rc_Boot_Mode == APP_RESULT_OPERATION_MODE) {
		retcode = Sensor_Enable();
	}
	if (RETCODE_OK == retcode) {
        if (pdPASS != xTaskCreate(AppController_Fire, (const char * const ) "AppController", TASK_STACK_SIZE_APP_CONTROLLER, NULL, TASK_PRIO_APP_CONTROLLER, &AppControllerHandle))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
        }
	}
	if (RETCODE_OK != retcode) {
		LOG_AT_ERROR(("AppController_Enable: Now calling SoftReset and reboot to recover\r\n"));
		Retcode_RaiseError(retcode);
		BSP_Board_SoftReset();
		// assert(0);
	}
	Utils_PrintResetCause();
}





/**
 * @brief Responsible for controlling the send data over MQTT application control flow.
 *
 * - Synchronize SNTP time stamp for the system if MQTT communication is secure
 * - Connect to MQTT broker
 * - Subscribe to MQTT topic
 * - Read environmental sensor data
 * - Publish data periodically for a MQTT topic
 *
 * @param[in] pvParameters
 * Unused
 */

static void AppController_Fire(void* pvParameters)
{
    BCDS_UNUSED(pvParameters);

	AppController_SetClientId();
	LOG_AT_INFO(("AppController_Fire: Device id for registration in Cumulocity %s\r\n",
			deviceId));
	MqttConnectInfo.BrokerURL = MQTTCfgParser_GetMqttBrokerName();
	MqttConnectInfo.BrokerPort = MQTTCfgParser_GetMqttBrokerPort();
	MqttConnectInfo.CleanSession = true;
	MqttConnectInfo.KeepAliveInterval = 100;

	if (rc_Boot_Mode == APP_RESULT_OPERATION_MODE) {
		MqttCredentials = (MQTT_Credentials_TZ) {
			.Username = MQTTCfgParser_GetMqttUser(),
			.Password = MQTTCfgParser_GetMqttPassword(),
			.Anonymous = MQTTCfgParser_IsMqttAnonymous()
		};
		MQTTOperation_Init();
	} else {
		MqttCredentials = (MQTT_Credentials_TZ) {
			.Username = MQTT_REGISTRATION_USERNAME,
			.Password = MQTT_REGISTRATION_PASSWORD,
			.Anonymous = FALSE
		};
		MQTTRegistration_Init();
	}
}

static void AppController_SetClientId(void) {
	/* Initialize Variables */
	uint8_t _macVal[WIFI_MAC_ADDR_LEN];
	uint8_t _macAddressLen = WIFI_MAC_ADDR_LEN;
	/* Get the MAC Address */
	memset(_macVal, NUMBER_UINT8_ZERO, WIFI_MAC_ADDR_LEN);
	sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &_macAddressLen, (uint8_t *) _macVal);

	memset(clientId, NUMBER_UINT8_ZERO, strlen(clientId));
	//changed
	sprintf(clientId, "d:%02X%02X%02X%02X%02X%02X", _macVal[0],
			_macVal[1], _macVal[2], _macVal[3], _macVal[4], _macVal[5]);

	memset(deviceId, NUMBER_UINT8_ZERO, strlen(deviceId));
	//changed
	sprintf(deviceId, "%02X%02X%02X%02X%02X%02X", _macVal[0],
			_macVal[1], _macVal[2], _macVal[3], _macVal[4], _macVal[5]);

	MqttConnectInfo.ClientId = clientId;
	LOG_AT_INFO( ("AppController: Client id of the device: %s \r\n", clientId));
}

static void AppController_ToogleLEDCallback(xTimerHandle xTimer) {
	(void) xTimer;
	//APP_STATUS l_app_status;

	switch(app_status) {
		case APP_STATUS_STARTED:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_TOGGLE);
			//LOG_AT_TRACE(("STATUS APP_STATUS_STARTED\n"));
			break;
		case APP_STATUS_OPERATING_STARTED:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_TOGGLE);
			LOG_AT_TRACE(("STATUS APP_STATUS_RUNNING\n"));
			break;
		case APP_STATUS_OPERATING_STOPPED:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_ON);
			LOG_AT_TRACE(("STATUS APP_STATUS_STOPPED\n"));
			break;
		case APP_STATUS_ERROR:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_ON);
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_OFF);
			LOG_AT_TRACE(("STATUS APP_STATUS_ERROR\n"));
			break;
		case APP_STATUS_REBOOT:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_TOGGLE);
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_O, (uint32_t) BSP_LED_COMMAND_TOGGLE);
			LOG_AT_TRACE(("STATUS APP_STATUS_REBOOT\n"));
			break;
		case APP_STATUS_REGISTERED:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_ON);
			LOG_AT_TRACE(("STATUS APP_STATUS_REGISTERED\n"));
			break;
		case APP_STATUS_REGISTERING:
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_R, (uint32_t) BSP_LED_COMMAND_OFF);
			BSP_LED_Switch((uint32_t) BSP_XDK_LED_Y, (uint32_t) BSP_LED_COMMAND_TOGGLE);
			LOG_AT_TRACE(("STATUS APP_STATUS_REGISTERING\n"));
			break;
		default:
			LOG_AT_WARNING(("AppController: Unknown status\n"));
			break;
	}
	switch(cmd_status) {
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
			LOG_AT_WARNING(("AppController: Unknown status\n"));
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
					("MQTTOperation: SNTP server time was not synchronized. Retrying...\r\n"));
		}
		sntpAttemps++;
		vTaskDelay(pdMS_TO_TICKS(1500));
	} while (0UL == sntpTimeStampFromServer && sntpAttemps < 3); // only try to sync time 3 times
	if (0UL == sntpTimeStampFromServer) {
		//sntpTimeStampFromServer = 1572566400UL; // use default time 1. Nov 2019 00:00:00 UTC
		sntpTimeStampFromServer = 1580515200UL; // use default time 1. Feb 2020 00:00:00 UTC
		SNTP_SetTime(sntpTimeStampFromServer);
		LOG_AT_WARNING(
				("MQTTOperation: Using fixed timestamp 1. Feb 2020 00:00:00 UTC, SNTP sync not possible\r\n"));
		retcode = RETCODE_OK; // clear return code
	}
	//uint8_t * timeBuffer = (uint8_t *) &sntpTimeStampFromServer;
	//LOG_AT_TRACE(("MQTTOperation: SNTP time: %d,%d,%d,%d,%d,%d,%d,%d\r\n", timeBuffer[0], timeBuffer[1], timeBuffer[2], timeBuffer[3],timeBuffer[4],timeBuffer[5],timeBuffer[6],timeBuffer[7]));
	struct tm time;
	char timezoneISO8601format[40];
	TimeStamp_SecsToTm(sntpTimeStampFromServer, &time);
	TimeStamp_TmToIso8601(&time, timezoneISO8601format, 40);
	BCDS_UNUSED(sntpTimeStampFromServer); /* Copy of sntpTimeStampFromServer will be used be HTTPS for TLS handshake */
	return retcode;
}

/* global functions ********************************************************* */

/** Refer interface header for description */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2) {

	/*
	 * Initialialze global variables
	 */

	MqttSetupInfo = (MQTT_Setup_TZ) { .IsSecure = APP_MQTT_SECURE_ENABLE, };/**< MQTT setup parameters */

	MqttConnectInfo =  (MQTT_Connect_TZ) { .ClientId = APP_MQTT_CLIENT_ID,
			.BrokerURL = MQTT_BROKER_HOST_NAME, .BrokerPort =
			MQTT_BROKER_HOST_PORT, .CleanSession = true, .KeepAliveInterval =
					100, };/**< MQTT connect parameters */

	MqttCredentials = (MQTT_Credentials_TZ) { .Username = APP_MQTT_USERNAME,
			.Password = APP_MQTT_PASSWORD, .Anonymous = false,

	};/**< MQTT connect parameters */

	StorageSetup =
	        (Storage_Setup_T) {
	                .SDCard = true,
	                .WiFiFileSystem = true,
	        };/**< Storage setup parameters */

	SensorSetup = (Sensor_Setup_T) {
			.CmdProcessorHandle = NULL,
			.Enable = {
					.Accel = false,
					.Mag = false,
					.Gyro = false,
					.Humidity = false,
					.Temp =	false,
					.Pressure = false,
					.Light = false,
					.Noise = false, },
			.Config = {
					.Accel = {
							.Type = SENSOR_ACCEL_BMA280,
							.IsRawData = false,
							.IsInteruptEnabled = false,
							.Callback = NULL, },
					.Gyro = {
							.Type = SENSOR_GYRO_BMG160,
							.IsRawData = false, },
					.Mag = {
							.IsRawData = false },
					.Light = {
							.IsInteruptEnabled = false,
							.Callback = NULL, },
					.Temp = {
							.OffsetCorrection =	APP_TEMPERATURE_OFFSET_CORRECTION,
							},
					},
			};/**< Sensor setup parameters */

	Retcode_T retcode = RETCODE_OK;
	BCDS_UNUSED(param2);

	vTaskDelay(pdMS_TO_TICKS(1000));
	LOG_AT_INFO(("AppController_Init: XDK System Startup\r\n"));

	// start status LED indicator
	AppController_SetStatus(APP_STATUS_STARTED);
	AppController_SetCmdStatus(APP_STATUS_STARTED);
	AppController_StartLEDBlinkTimer (500);

	// init battery monitor
	BatteryMonitor_Init();

	if (cmdProcessorHandle == NULL) {
		LOG_AT_ERROR(("AppController_Init: Command processor handle is NULL \r\n"));
		retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
	} else if (RETCODE_OK == retcode) {
		AppCmdProcessor = (CmdProcessor_T *) cmdProcessorHandle;
		retcode = CmdProcessor_EnqueueFromIsr(AppCmdProcessor, AppController_Setup,NULL, UINT32_C(0));
	}

	if (RETCODE_OK != retcode) {
		Retcode_RaiseError(retcode);
		assert(0); /* To provide LED indication for the user */
	}
}

void AppController_SetStatus( uint8_t status) {
	if (app_status != APP_STATUS_REBOOT) {
		app_status = status;
	}
}

uint8_t AppController_GetStatus(void) {
	return app_status;
}

void AppController_SetCmdStatus( uint8_t status) {
		cmd_status = status;
}

uint8_t AppController_GetCmdStatus(void) {
	return cmd_status;
}
