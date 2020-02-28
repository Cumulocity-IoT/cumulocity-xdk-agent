/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	XDK_MQTT_Z.h
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/**
 * @ingroup CONNECTIVITY
 *
 * @defgroup MQTT MQTT
 * @{
 *
 * @brief This module handles the MQTT communication.
 *
 * @file
 */

/* header definition ******************************************************** */
#ifndef XDK_MQTT_Z_H_
#define XDK_MQTT_Z_H_

/* local interface declaration ********************************************** */
#include "BCDS_Retcode.h"
#include "BCDS_CmdProcessor.h"

/**
 * @brief   Typedef to represent the MQTT setup feature.
 */
typedef struct MQTT_Credentials_SZ MQTT_Credentials_TZ;

struct MQTT_Credentials_SZ
{
	/// username for the connection, see MQTT spec 3.1.3.4.
	const char * Username;
	/// password for the connection, see MQTT spec 3.1.3.5
	const char * Password;
	// ignore username/password if anonymous
	bool Anonymous;
};


/**
 * @brief   Structure to represent the MQTT setup features.
 */
struct MQTT_Setup_SZ
{
    CmdProcessor_T * CmdProcessorHandle; /**< Command processor handle to handle MQTT agent events and servicing. */
    bool IsSecure; /**< Boolean representing if we will do a HTTP secure communication. Unused if MqttType is MQTT_TYPE_AWS. */
};

/**
 * @brief   Typedef to represent the MQTT setup feature.
 */
typedef struct MQTT_Setup_SZ MQTT_Setup_TZ;

/**
 * @brief   Structure to represent the MQTT connect features.
 */
struct MQTT_Connect_SZ
{
    const char * ClientId; /**< The client identifier which is a identifier of each MQTT client connecting to a MQTT broker. It needs to be unique for the broker to know the state of the client. */
    const char * BrokerURL; /**< The URL pointing to the MQTT broker */
    uint16_t BrokerPort; /**< The port number of the MQTT broker */
    bool CleanSession; /**< The clean session flag indicates to the broker whether the client wants to establish a clean session or a persistent session where all subscriptions and messages (QoS 1 & 2) are stored for the client. */
    uint32_t KeepAliveInterval; /**< The keep alive interval (in seconds) is the time the client commits to for when sending regular pings to the broker. The broker responds to the pings enabling both sides to determine if the other one is still alive and reachable */
};

/**
 * @brief   Typedef to represent the MQTT connect feature.
 */
typedef struct MQTT_Connect_SZ MQTT_Connect_TZ;

/**
 * @brief   Structure to represent the MQTT publish features.
 */
struct MQTT_Publish_SZ
{
    const char * Topic; /**< The MQTT topic to which the messages are to be published */
    uint32_t QoS; /**< The MQTT Quality of Service level. If 0, the message is send in a fire and forget way and it will arrive at most once. If 1 Message reception is acknowledged by the other side, retransmission could occur. */
    const char * Payload; /**< Pointer to the payload to be published */
    uint32_t PayloadLength; /**< Length of the payload to be published */
};

/**
 * @brief   Typedef to represent the MQTT publish feature.
 */
typedef struct MQTT_Publish_SZ MQTT_Publish_TZ;

/**
 * @brief   Structure to represent the incoming MQTT message informations.
 */
struct MQTT_SubscribeCBParam_SZ
{
    const char * Topic; /**< The incoming MQTT topic pointer */
    uint32_t TopicLength; /**< The incoming MQTT topic length */
    const char * Payload; /**< The incoming MQTT payload pointer */
    uint32_t PayloadLength; /**< The incoming MQTT payload length */
};

/**
 * @brief   Typedef to represent the incoming MQTT message information.
 */
typedef struct MQTT_SubscribeCBParam_SZ MQTT_SubscribeCBParam_TZ;

/**
 * @brief   Typedef to the function to be called upon receiving incoming MQTT messages.
 */
typedef void (*MQTT_SubscribeCB_TZ)(MQTT_SubscribeCBParam_TZ param);

/**
 * @brief   Structure to represent the MQTT subscribe features.
 */
struct MQTT_Subscribe_SZ
{
    const char * Topic; /**< The MQTT topic for which the messages are to be subscribed */
    uint32_t QoS; /**< The MQTT Quality of Service level. If 0, the message is send in a fire and forget way and it will arrive at most once. If 1 Message reception is acknowledged by the other side, retransmission could occur. */
    MQTT_SubscribeCB_TZ IncomingPublishNotificationCB; /**< The function to be called upon receiving incoming MQTT messages. Can be NULL. */
};

/**
 * @brief   Structure to represent the MQTT subscribe feature.
 */
typedef struct MQTT_Subscribe_SZ MQTT_Subscribe_TZ;

/**
 * @brief This will setup the MQTT
 *
 * @param[in] setup
 * Pointer to the MQTT setup feature
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 *
 * @note
 * - This must be the first API to be called if MQTT feature is to be used.
 * - Do not call this API more than once.
 */
Retcode_T MQTT_Setup_Z(MQTT_Setup_TZ * setup);

/**
 * @brief This will enable the MQTT by connecting to the broker
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 *
 * @note
 * - If setup->IsSecure was enabled at the time of #MQTT_Setup,
 *   then we will enable the HTTPs server certificates as well.
 * - #MQTT_Setup must have been successful prior.
 * - #WLAN_Enable must have been successful prior.
 * - Do not call this API more than once.
 */
Retcode_T MQTT_Enable_Z(void);

/**
 * @brief This will disconnect from a MQTT broker
 *
 * @param[in] mqttConnect
 * Pointer to the MQTT connect feature
 *
 * @param[in] timeout
 * Timeout in milli-second to await successful connection
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
*/
Retcode_T Mqtt_DisconnectFromBroker_Z(void);


/**
 * @brief This will connect to a MQTT broker
 *
 * @param[in] mqttConnect
 * Pointer to the MQTT connect feature
 *
 * @param[in] timeout
 * Timeout in milli-second to await successful connection
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
Retcode_T MQTT_ConnectToBroker_Z(MQTT_Connect_TZ * mqttConnect, uint32_t timeout, MQTT_Credentials_TZ * mqttCredentials);

/**
 * @brief This will subscribe to a MQTT topic
 *
 * @param[in] subscribe
 * Pointer to the MQTT subscribe feature
 *
 * @param[in] timeout
 * Timeout in milli-second to await successful subscription
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
Retcode_T MQTT_SubsribeToTopic_Z(MQTT_Subscribe_TZ * subscribe, uint32_t timeout);

/**
 * @brief This will publish to a MQTT topic
 *
 * @param[in] subscribe
 * Pointer to the MQTT publish feature
 *
 * @param[in] timeout
 * Timeout in milli-second to await successful publication
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
Retcode_T MQTT_PublishToTopic_Z(MQTT_Publish_TZ * publish, uint32_t timeout);

/**
 * @brief This will unsubscribe to a MQTT topic
 *
 * @param[in] unsubscribe
 * Pointer to the MQTT subscribe feature
 *
 * @param[in] timeout
 * Timeout in milli-second to await successful subscription
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
Retcode_T MQTT_UnSubsribeFromTopic_Z(MQTT_Subscribe_TZ * subscribe, uint32_t timeout);


/**
 * @brief This function will check the connection status with Mqtt broker.
 * 	      If connection status in false, session must be started again,
 * 	      by calling MQTT_ConnectToBroker followed by topic subscription/publish.
 *
 * @return  RETCODE_OK on success, or RETCODE_MQTT_DISCONNECT otherwise.
 */
Retcode_T MQTT_IsConnected_Z(void);

#endif /* XDK_MQTT_Z_H_ */

/**@} */
