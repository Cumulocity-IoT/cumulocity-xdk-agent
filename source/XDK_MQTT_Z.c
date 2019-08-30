/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTT_Z.c
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
 * This module handles the MQTT communication
 *
 */

/* module includes ********************************************************** */

/* own header files */
#include "XdkCommonInfo.h"

#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_COMMON_ID_MQTT

#if XDK_CONNECTIVITY_MQTT
/* own header files */
#include "XDK_MQTT_Z.h"

/* system header files */
#include <stdio.h>

/* additional interface header files */
//#include "aws_mqtt_agent.h"
//#include "aws_bufferpool.h"
//#include "aws_secure_sockets.h"
#include "MbedTLSAdapter.h"
#include "HTTPRestClientSecurity.h"
#include "BCDS_WlanNetworkConfig.h"
#include "Serval_Msg.h"
#include "Serval_Http.h"
#include "Serval_HttpClient.h"
#include "Serval_Types.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "Serval_Mqtt.h"

/* constant definitions ***************************************************** */

/**<  Macro for the number of topics to subscribe */
#define MQTT_SUBSCRIBE_COUNT                1UL

/**<  Macro for the non secure serval stack expected MQTT URL format */
#define MQTT_URL_FORMAT_NON_SECURE          "mqtt://%s:%d"

/**<  Macro for the secure serval stack expected MQTT URL format */
#define MQTT_URL_FORMAT_SECURE              "mqtts://%s:%d"

/* local variables ********************************************************** */

/**< Handle for MQTT subscribe operation  */
static SemaphoreHandle_t MqttSubscribeHandle_Z;
/**< Handle for MQTT publish operation  */
static SemaphoreHandle_t MqttPublishHandle_Z;
/**< Handle for MQTT send operation  */
static SemaphoreHandle_t MqttSendHandle_Z;
/**< Handle for MQTT send operation  */
static SemaphoreHandle_t MqttConnectHandle_Z;
/**< MQTT setup information */
static MQTT_Setup_TZ MqttSetupInfo_Z;
/**< MQTT incoming publish notification callback for the application */
static MQTT_SubscribeCB_TZ IncomingPublishNotificationCB_Z;
/**< MQTT session instance */
static MqttSession_T MqttSession_Z;
/**< MQTT connection status */
static bool MqttConnectionStatus_Z = false;
/**< MQTT subscription status */
static bool MqttSubscriptionStatus_Z = false;
/**< MQTT publish status */
static bool MqttPublishStatus_Z = false;

/**
 * @brief Event handler for incoming publish MQTT data
 *
 * @param[in] publishData
 * Event Data for publish
 */
static void HandleEventIncomingPublish_Z(MqttPublishData_T publishData)
{
    if (NULL != IncomingPublishNotificationCB_Z)
    {
        MQTT_SubscribeCBParam_TZ param =
                {
                        .Topic = publishData.topic.start,
                        .TopicLength = publishData.topic.length,
                        .Payload = (const char *) publishData.payload,
                        .PayloadLength = publishData.length
                };
        IncomingPublishNotificationCB_Z(param);
    }
}

/**
 * @brief Callback function used by the stack to communicate events to the application.
 * Each event will bring with it specialized data that will contain more information.
 *
 * @param[in] session
 * MQTT session
 *
 * @param[in] event
 * MQTT event
 *
 * @param[in] eventData
 * MQTT data based on the event
 *
 */
static retcode_t MqttEventHandler_Z(MqttSession_T* session, MqttEvent_t event, const MqttEventData_t* eventData)
{
    BCDS_UNUSED(session);
    Retcode_T retcode = RETCODE_OK;
    printf("MqttEventHandler_Z: Event - %d\r\n", (int) event);
    switch (event)
    {
    case MQTT_CONNECTION_ESTABLISHED:
        MqttConnectionStatus_Z = true;
        if (pdTRUE != xSemaphoreGive(MqttConnectHandle_Z))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
        }
        break;
    case MQTT_CONNECTION_ERROR:
        case MQTT_CONNECT_SEND_FAILED:
        case MQTT_CONNECT_TIMEOUT:
        MqttConnectionStatus_Z = false;
        if (pdTRUE != xSemaphoreGive(MqttConnectHandle_Z))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
        }

        break;
    case MQTT_CONNECTION_CLOSED:
        MqttConnectionStatus_Z = false;
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_CONNECTION_CLOSED);
        break;
    case MQTT_SUBSCRIPTION_ACKNOWLEDGED:
        MqttSubscriptionStatus_Z = true;
        if (pdTRUE != xSemaphoreGive(MqttSubscribeHandle_Z))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
        }
        break;
    case MQTT_SUBSCRIBE_SEND_FAILED:
        case MQTT_SUBSCRIBE_TIMEOUT:
        MqttSubscriptionStatus_Z = false;
        if (pdTRUE != xSemaphoreGive(MqttSubscribeHandle_Z))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
        }
        break;
    case MQTT_SUBSCRIPTION_REMOVED:
        // indicate no error
    	// retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_REMOVED);
        break;
    case MQTT_INCOMING_PUBLISH:
        HandleEventIncomingPublish_Z(eventData->publish);
        break;
    case MQTT_PUBLISHED_DATA:
        MqttPublishStatus_Z = true;
        if (pdTRUE != xSemaphoreGive(MqttPublishHandle_Z))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
        }
        break;
    case MQTT_PUBLISH_SEND_FAILED:
        case MQTT_PUBLISH_SEND_ACK_FAILED:
        case MQTT_PUBLISH_TIMEOUT:
        MqttPublishStatus_Z = false;
        if (pdTRUE != xSemaphoreGive(MqttPublishHandle_Z))
        {
            retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_SEMAPHORE_ERROR);
        }
        break;
    default:
        printf("MqttEventHandler_Z: Unhandled MQTT Event\r\n");
        break;
    }

    if (RETCODE_OK != retcode)
    {
        Retcode_RaiseError(retcode);
    }

    return RC_OK;
}

#ifdef mqttconfigENABLE_SUBSCRIPTION_MANAGEMENT

static MQTTBool_t MqttAgentSubscribeCallback_Z(void * pvPublishCallbackContext, const MQTTPublishData_t * const pxPublishData)
{
    if ((NULL != IncomingPublishNotificationCB_Z) &&
            (NULL != pvPublishCallbackContext) &&
            (NULL != pxPublishData))
    {
        MQTT_SubscribeCBParam_TZ param =
        {
            .Topic = (const char *) pxPublishData->pucTopic,
            .TopicLength = pxPublishData->usTopicLength,
            .Payload = (const char *) pxPublishData->pvData,
            .PayloadLength = pxPublishData->ulDataLength
        };
        IncomingPublishNotificationCB_Z(param);
    }

    return eMQTTFalse; /* Returning eMQTTFalse will free the buffer */
}

#endif /* mqttconfigENABLE_SUBSCRIPTION_MANAGEMENT */

/** Refer interface header for description */
Retcode_T MQTT_Setup_Z(MQTT_Setup_TZ * setup)
{
    Retcode_T retcode = RETCODE_OK;
    if (NULL == setup)
    {
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    } else  {
		if (setup->IsSecure)
		{
			retcode = HTTPRestClientSecurity_Setup();
		}
		if (RETCODE_OK == retcode)
		{
			MqttSubscribeHandle_Z = xSemaphoreCreateBinary();
			if (NULL == MqttSubscribeHandle_Z)
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
			}
		}
		if (RETCODE_OK == retcode)
		{
			MqttPublishHandle_Z = xSemaphoreCreateBinary();
			if (NULL == MqttPublishHandle_Z)
			{
				vSemaphoreDelete(MqttSubscribeHandle_Z);
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
			}
		}
		if (RETCODE_OK == retcode)
		{
			MqttSendHandle_Z = xSemaphoreCreateBinary();
			if (NULL == MqttSendHandle_Z)
			{
				vSemaphoreDelete(MqttSubscribeHandle_Z);
				vSemaphoreDelete(MqttPublishHandle_Z);
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
			}
		}
		if (RETCODE_OK == retcode)
		{
			MqttConnectHandle_Z = xSemaphoreCreateBinary();
			if (NULL == MqttConnectHandle_Z)
			{
				vSemaphoreDelete(MqttSubscribeHandle_Z);
				vSemaphoreDelete(MqttPublishHandle_Z);
				vSemaphoreDelete(MqttSendHandle_Z);
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
			}
		}

        if (RETCODE_OK == retcode)
        {
            MqttSetupInfo_Z = *setup;
        }
    }
    return retcode;
}

/** Refer interface header for description */
Retcode_T MQTT_Enable_Z(void)
{
    Retcode_T retcode = RETCODE_OK;

    if (MqttSetupInfo_Z.IsSecure) {
            if(RC_OK != MbedTLSAdapter_Initialize())
            {
                printf("MQTT_Enable_Z: MbedTLSAdapter_Initialize unable to initialize Mbedtls.\r\n");
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_HTTP_INIT_REQUEST_FAILED);
            }

            if ( RETCODE_OK == retcode )
            {
                retcode = HTTPRestClientSecurity_Enable();
            }
    }

    return retcode;
}

Retcode_T Mqtt_DisconnectFromBroker_Z(void){
    Retcode_T retcode = RETCODE_OK;

    printf("Mqtt_DisconnectFromBroker_Z: Disconnect from broker\r\n");
    retcode= Mqtt_disconnect(&MqttSession_Z);

    return retcode;
}

/** Refer interface header for description */
Retcode_T MQTT_ConnectToBroker_Z(MQTT_Connect_TZ * mqttConnect, uint32_t timeout, MQTT_Credentials_TZ * mqttCredentials)
{
    Retcode_T retcode = RETCODE_OK;

    if (NULL == mqttConnect)
    {
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    }  else  {

		Ip_Address_T brokerIpAddress = 0UL;
		StringDescr_T clientID;
		char mqttBrokerURL[30] = { 0 };
		char serverIpStringBuffer[16] = { 0 };

		if (RC_OK != Mqtt_initialize())
		{
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_INIT_FAILED);
		}
		if (RETCODE_OK == retcode)
		{
			if (RC_OK != Mqtt_initializeInternalSession(&MqttSession_Z))
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_INIT_INTERNAL_SESSION_FAILED);
			}
		}

		if (RETCODE_OK == retcode)
		{
			retcode = WlanNetworkConfig_GetIpAddress((uint8_t *) mqttConnect->BrokerURL, &brokerIpAddress);
		}
		if (RETCODE_OK == retcode)
		{
			if (0 > Ip_convertAddrToString(&brokerIpAddress, serverIpStringBuffer))
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_IPCONIG_FAIL);
			}
		}

		if (RETCODE_OK == retcode)
		{
			MqttSession_Z.MQTTVersion = 4;
			MqttSession_Z.keepAliveInterval = mqttConnect->KeepAliveInterval;
			MqttSession_Z.cleanSession = mqttConnect->CleanSession;
			MqttSession_Z.will.haveWill = false;
			MqttSession_Z.onMqttEvent = MqttEventHandler_Z;

			StringDescr_wrap(&clientID, mqttConnect->ClientId);
			MqttSession_Z.clientID = clientID;

			if (!mqttCredentials->Anonymous) {
				// add credentials
				StringDescr_T uname;
				StringDescr_wrap(&uname, mqttCredentials->Username);
				MqttSession_Z.username = uname;

				StringDescr_T passw;
				StringDescr_wrap(&passw, mqttCredentials->Password);
				MqttSession_Z.password = passw;
				printf("MQTT_ConnectToBroker_Z: setting credentials: [%s]\r\n", mqttCredentials->Username );
				printf("MQTT_ConnectToBroker_Z: setting password: [%s]\r\n", mqttCredentials->Password );
			}

			if (MqttSetupInfo_Z.IsSecure)
			{
				printf("MQTT_ConnectToBroker_Z: connecting secure\r\n");
				sprintf(mqttBrokerURL, MQTT_URL_FORMAT_SECURE, serverIpStringBuffer, mqttConnect->BrokerPort);
				MqttSession_Z.target.scheme = SERVAL_SCHEME_MQTTS;
			}
			else
			{
				printf("MQTT_ConnectToBroker_Z: connecting unsecure\r\n");
				sprintf(mqttBrokerURL, MQTT_URL_FORMAT_NON_SECURE, serverIpStringBuffer, mqttConnect->BrokerPort);
				MqttSession_Z.target.scheme = SERVAL_SCHEME_MQTT;
			}

			if (RC_OK == SupportedUrl_fromString((const char *) mqttBrokerURL, (uint16_t) strlen((const char *) mqttBrokerURL), &MqttSession_Z.target))
			{
				MqttConnectionStatus_Z = false;
				/* This is a dummy take. In case of any callback received
				 * after the previous timeout will be cleared here. */
				(void) xSemaphoreTake(MqttConnectHandle_Z, 0UL);
				if (RC_OK != Mqtt_connect(&MqttSession_Z))
				{
					printf("MQTT_ConnectToBroker_Z: Failed to connect MQTT \r\n");
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_CONNECT_FAILED);
				}
			}
			else
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_PARSING_ERROR);
			}
		}
		if (RETCODE_OK == retcode)
		{
			if (pdTRUE != xSemaphoreTake(MqttConnectHandle_Z, pdMS_TO_TICKS(timeout)))
			{
				printf("MQTT_ConnectToBroker_Z: Failed since Connect event was not received \r\n");
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_CONNECT_CB_NOT_RECEIVED);
			}
			else
			{
				if (true != MqttConnectionStatus_Z)
				{
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_CONNECT_STATUS_ERROR);
				}
			}
		}

    }
    return retcode;
}

/** Refer interface header for description */
Retcode_T MQTT_SubsribeToTopic_Z(MQTT_Subscribe_TZ * subscribe, uint32_t timeout)
{
    Retcode_T retcode = RETCODE_OK;

    if (NULL == subscribe)
    {
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    }  else {
		static StringDescr_T subscribeTopicDescription[MQTT_SUBSCRIBE_COUNT];
		Mqtt_qos_t qos[MQTT_SUBSCRIBE_COUNT];

		StringDescr_wrap(&(subscribeTopicDescription[0]), subscribe->Topic);
		qos[0] = (Mqtt_qos_t) subscribe->QoS;

		printf("MQTT_SubsribeToTopic_Z: Subscribing to topic: [%s], Qos: [%d]\r\n", subscribe->Topic, qos[0]);
		IncomingPublishNotificationCB_Z = subscribe->IncomingPublishNotificationCB;
		MqttSubscriptionStatus_Z = false;
		/* This is a dummy take. In case of any callback received
		 * after the previous timeout will be cleared here. */
		(void) xSemaphoreTake(MqttSubscribeHandle_Z, 0UL);
		if (RC_OK != Mqtt_subscribe(&MqttSession_Z, MQTT_SUBSCRIBE_COUNT, subscribeTopicDescription, qos))
		{
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_FAILED);
		}

		if (RETCODE_OK == retcode)
		{
			if (pdTRUE != xSemaphoreTake(MqttSubscribeHandle_Z, pdMS_TO_TICKS(timeout)))
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_CB_NOT_RECEIVED);
			}
			else
			{
				if (true != MqttSubscriptionStatus_Z)
				{
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_STATUS_ERROR);
				}
			}
		}
    }
    return retcode;
}

/** Refer interface header for description */
Retcode_T MQTT_PublishToTopic_Z(MQTT_Publish_TZ * publish, uint32_t timeout)
{
    Retcode_T retcode = RETCODE_OK;

    if (NULL == publish)
    {
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    }
    else
    {
		static StringDescr_T publishTopicDescription;
		StringDescr_wrap(&publishTopicDescription, publish->Topic);

		MqttPublishStatus_Z = false;
		/* This is a dummy take. In case of any callback received
		 * after the previous timeout will be cleared here. */
		(void) xSemaphoreTake(MqttPublishHandle_Z, 0UL);
		if (RC_OK != Mqtt_publish(&MqttSession_Z, publishTopicDescription, publish->Payload, publish->PayloadLength, (uint8_t) publish->QoS, false))
		{
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_PUBLISH_FAILED);
		}
		if (RETCODE_OK == retcode)
		{
			if (pdTRUE != xSemaphoreTake(MqttPublishHandle_Z, pdMS_TO_TICKS(timeout)))
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_PUBLISH_CB_NOT_RECEIVED);
			}
			else
			{
				if (true != MqttPublishStatus_Z)
				{
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_PUBLISH_STATUS_ERROR);
				}
			}
		}
    }

    return retcode;
}
/** Refer interface header for description */
Retcode_T MQTT_UnSubsribeFromTopic_Z(MQTT_Subscribe_TZ * subscribe, uint32_t timeout)
{
    Retcode_T retcode = RETCODE_OK;

    if (NULL == subscribe)
    {
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    }
    else {
		static StringDescr_T subscribeTopicDescription[MQTT_SUBSCRIBE_COUNT];
		Mqtt_qos_t qos[MQTT_SUBSCRIBE_COUNT];

		StringDescr_wrap(&(subscribeTopicDescription[0]), subscribe->Topic);
		qos[0] = (Mqtt_qos_t) subscribe->QoS;

		printf("MQTT_UnSubsribeFromTopic_Z: Unsubscribing from topic: [%s], Qos: [%d]\r\n", subscribe->Topic, qos[0]);
		IncomingPublishNotificationCB_Z = subscribe->IncomingPublishNotificationCB;
		MqttSubscriptionStatus_Z = false;
		/* This is a dummy take. In case of any callback received
		 * after the previous timeout will be cleared here. */
		(void) xSemaphoreTake(MqttSubscribeHandle_Z, 0UL);
		if (RC_OK != Mqtt_unsubscribe(&MqttSession_Z, MQTT_SUBSCRIBE_COUNT, subscribeTopicDescription))
		{
			retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_FAILED);
		}

		if (RETCODE_OK == retcode)
		{
			if (pdTRUE != xSemaphoreTake(MqttSubscribeHandle_Z, pdMS_TO_TICKS(timeout)))
			{
				retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_CB_NOT_RECEIVED);
			}
			else
			{
				if (true != MqttSubscriptionStatus_Z)
				{
					retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_MQTT_SUBSCRIBE_STATUS_ERROR);
				}
			}
		}

    }
    return retcode;
}

#endif /* XDK_CONNECTIVITY_MQTT */
