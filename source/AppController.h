/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	AppController.h
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/**
 * @file
 *
 * @brief Configuration header for the AppController.c file.
 *
 */

/* header definition ******************************************************** */
#ifndef APPCONTROLLER_H_
#define APPCONTROLLER_H_

/* local interface declaration ********************************************** */
#include "XDK_Utils.h"

/* local type and macro definitions */


/* WLAN configurations ****************************************************** */

/**
 * WLAN_SSID is the WIFI network name where user wants connect the XDK device.
 * Make sure to update the WLAN_PSK constant according to your required WIFI network.
 */
#define WLAN_SSID                           "TBD"


/**
 * WLAN_PSK is the WIFI router WPA/WPA2 password used at the Wifi network connection.
 * Make sure to update the WLAN_PSK constant according to your router password.
 */
#define WLAN_PSK                            "TBD"


/**
 * WLAN_STATIC_IP is a boolean. If "true" then static IP will be assigned and if "false" then DHCP is used.
 */
#define WLAN_STATIC_IP                      false

/**
 * WLAN_IP_ADDR is the WIFI router WPA/WPA2 static IPv4 IP address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_IP_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_IP_ADDR                        XDK_NETWORK_IPV4(0, 0, 0, 0)

/**
 * WLAN_GW_ADDR is the WIFI router WPA/WPA2 static IPv4 gateway address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_GW_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_GW_ADDR                        XDK_NETWORK_IPV4(0, 0, 0, 0)

/**
 * WLAN_DNS_ADDR is the WIFI router WPA/WPA2 static IPv4 DNS address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_DNS_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_DNS_ADDR                       XDK_NETWORK_IPV4(0, 0, 0, 0)

/**
 * WLAN_MASK is the WIFI router WPA/WPA2 static IPv4 mask address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_MASK constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_MASK                           XDK_NETWORK_IPV4(0, 0, 0, 0)

/* SNTP configurations ****************************************************** */

/**
 * SNTP_SERVER_URL is the SNTP server URL.
 */
#define SNTP_SERVER_URL                     "0.de.pool.ntp.org"

/**
 * SNTP_SERVER_PORT is the SNTP server port number.
 */
#define SNTP_SERVER_PORT                    123
#define STR_SNTP_SERVER_PORT                "123"

/* MQTT server configurations *********************************************** */

/**
 * APP_MQTT_BROKER_HOST_URL is the MQTT broker host address URL.
 */
#define MQTT_BROKER_HOST_NAME            "mqtt.cumulocity.com"

/**
 * APP_MQTT_BROKER_HOST_PORT is the MQTT broker host port.
 */
#define MQTT_BROKER_HOST_PORT           UINT16_C(8883)
#define STR_MQTT_BROKER_HOST_PORT           "8883"

/**
 * APP_MQTT_CLIENT_ID is the device name
 */
#define APP_MQTT_CLIENT_ID                  "TBD"

/**
 * APP_MQTT_USERNAME is the device name
 */
#define APP_MQTT_USERNAME                  "TBD"

/**
 * APP_MQTT_USERNAME is the device name
 */
#define APP_MQTT_PASSWORD                  "TBD"


/**
 * APP_MQTT_TOPIC is the topic to subscribe and publish
 */
#define APP_MQTT_TOPIC                      "s/uc/XDK"
/**
 * APP_MQTT_SECURE_ENABLE is a macro to enable MQTT with security
 */
#define APP_MQTT_SECURE_ENABLE              1

/**
 * APP_MQTT_DATA_PUBLISH_PERIODICITY is time for MQTT to publish the sensor data
 */
#define APP_MQTT_DATA_PUBLISH_PERIODICITY   UINT32_C(1000)

/* public type and macro definitions */
#define WIFI_DEFAULT_MAC			"00:00:00:00:00:00"         /**< Macro used to specify the Default MAC Address*/
#define WIFI_DEFAULT_MAC_CLIENTID	"d:XDK_00_00_00_00_00_00"   /**< Macro used to specify the Default MAC Address as input for clientId, cumulocity doesn't allow : n clientId*/
#define DEVICE_ID					"XDK_00_00_00_00_00_00"   /**< Macro used to specify the Default MAC Address as input for clientId, cumulocity doesn't allow : n clientId*/

#define WIFI_MAC_ADDR_LEN 	   		UINT8_C(6)                  /**< Macro used to specify MAC address length*/


#define MQTT_CONNECT_TIMEOUT_IN_MS                  UINT32_C(20000)/**< Macro for MQTT connection timeout in milli-second */
#define MQTT_SUBSCRIBE_TIMEOUT_IN_MS                UINT32_C(20000)/**< Macro for MQTT subscription timeout in milli-second */
#define MQTT_UNSUBSCRIBE_TIMEOUT_IN_MS              UINT32_C(1000)/**< Macro for MQTT subscription timeout in milli-second */
#define MQTT_PUBLISH_TIMEOUT_IN_MS                  UINT32_C(20000)/**< Macro for MQTT publication timeout in milli-second */

#define MQTT_DELAY_IN_MS        					UINT32_C(222)/**< Macro for MQTT publication timeout in milli-second */

#define APP_TEMPERATURE_OFFSET_CORRECTION           (-1403)/**< Macro for static temperature offset correction. Self heating, temperature correction factor */

#define APP_RESPONSE_FROM_SNTP_SERVER_TIMEOUT       UINT32_C(10000)/**< Timeout for SNTP server time sync */

/* public type and macro definitions */

/* local function prototype declarations */

/* local module global variable declarations */
//
#define NUMBER_UINT8_ZERO		     UINT8_C(0)     /**< Zero value */
#define NUMBER_UINT32_ZERO 		     UINT32_C(0)    /**< Zero value */
#define NUMBER_UINT16_ZERO 		     UINT16_C(0)    /**< Zero value */
#define NUMBER_INT16_ZERO 		     INT16_C(0)     /**< Zero value */
//
#define POINTER_NULL 			     NULL          /**< ZERO value for pointers */

#define FINEST			1
#define FINE			2
#define	DEBUG			3
#define	INFO			4
#define	EXCEPTION		5
#define DEBUG_LEVEL 	FINE



#define BOOT_PENDING   		"1"
#define NO_BOOT_PENDING   	"0"

typedef enum
{
	APP_INIT_FILE_MISSING = INT8_C(-4),
	APP_SDCARD_BUFFER_OVERFLOW_ERROR= INT8_C(-3),
	APP_SDCARD_ERROR = INT8_C(-2),
	APP_ERROR= INT8_C(-1),
	APP_OPERATION_MODE = INT8_C(0),
	APP_REGISTRATION_MODE = INT8_C(1),
	APP_OK = INT8_C(2),
	APP_SDCARD_NO_ERROR = INT8_C(3),
	APP_OPERATION_OK = INT8_C(4),
} APP_RESULT;

typedef enum
{
	APP_STATUS_ERROR= INT8_C(-2),
	APP_STATUS_STARTED = INT8_C(1),
	APP_STATUS_OPERATEING= INT8_C(2),
	APP_STATUS_REBOOT= INT8_C(3),
	APP_STATUS_STOPPED = INT8_C(4),
	APP_STATUS_REGISTERING = INT8_C(5),
	APP_STATUS_REGISTERED = INT8_C(6),

} APP_STATUS;


/* MQTT C8Y Configuration */
#define MQTT_SECURE         true 						/**use MQTT over TLS */
#define MQTT_USERNAME		"REGISTRATION"
#define MQTT_PASSWORD		"REGISTRATION"
#define MQTT_ANONYMOUS		false


#define STR_STREAM_RATE     "5000"			   /**< Stream Data Rate in MS */
#define REBOOT_DELAY 		5000			   /**< Delay reboot so that device can send back "reboot is in progress" */
#define ACCEL_EN            true               /**< Accelerometer Data Enable */
#define GYRO_EN             true               /**< Gyroscope Data Enable */
#define MAG_EN              true               /**< Magnetometer Data Enable */
#define ENV_EN              true               /**< Environmental Data Enable */
#define LIGHT_EN            true               /**< Ambient Light Data Enable */
#define NOISE_EN            false               /**< Noise Data Enable */


/* Sensor type and macro definitions */
#define SENSOR_LARGE_BUF_SIZE    256
/* Sensor type and macro definitions */
#define SENSOR_SMALL_BUF_SIZE    128

typedef struct {
	uint32_t length;
	char data[SENSOR_LARGE_BUF_SIZE];
} SensorDataBuffer;

typedef struct {
	uint32_t length;
	char data[SENSOR_LARGE_BUF_SIZE];
} AssetDataBuffer;

/* local inline function definitions */

/**
 * @brief Gives control to the Application controller.
 *
 * @param[in] cmdProcessorHandle
 * Handle of the main command processor which shall be used based on the application needs
 *
 * @param[in] param2
 * Unused
 */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2);
void AppController_SetStatus( uint8_t status);

#endif /* APPCONTROLLER_H_ */

/** ************************************************************************* */
