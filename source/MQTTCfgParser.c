/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTCfgParser.c
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/**

 *
 *  This file provides the Implementation of parser of configuration file present on the micro SD card
 *
 *  Configuration parser Processing source file
 * Module that parses the configuration file present on the micro SD card
 * 
 * Configuration file has following constraints:
 * 1) TOKENs should not have any space.
 * 2) Value should not have any trailing spaces.
 * 3) TOKENs are case sensitive.
 * 4) CFG_FILE_READ_BUFFER size is currently 2024 bytes.
 * 5) Default value of TOKENs are not considered from '.c' and '.h' files.
 * \code{.unparsed}
 *
 * Example for config.txt file:
 * Documentation for config.txt
 * The device agent for the XDK runs in two modes:
 *	1) Registration mode: Entries for MQTTUSER and MQTTPASSWORD are missing. This is important.
 *		Upon registration the XDK receives device credentials and writes them to the file.
 *	2) Operation mode: MQTTUSER and MQTTPASSWORD where appended at the end of the config file and
 *		now XDK can connect to C8Y tenant and send measurements.
 *
 * This config file is modified depending which phase the XDK is running in.
 *
 * WIFISSID=<SSID>
 * WIFIPASSWORD=<PASSWORD OF WIFI>
 * MQTTBROKERNAME=mqtt.cumulocity.com
 * MQTTBROKERPORT=<PORT OF MQTT CUMULOCITY BROKER, 8883 FOR TLS, 1883 NO TLS>
 * MQTTSECURE=<TRUE FOR TLS, FALSE OTHERWISE>
 * MQTTANONYMOUS=FALSE #this property is lony meant to test the agent with other mqtt brokers, cumulocity always requires username password
 * STREAMRATE=<RATE TO SEND MEASUREMENTS TO C(Y IN MILISECONDS>
 * ACCELENABLED=<TRUE TO SEND,FALSE OTHERWISE>
 * GYROENABLED=<TRUE TO SEND,FALSE OTHERWISE>
 * MAGENABLED=<TRUE TO SEND,FALSE OTHERWISE>
 * ENVENABLED=<TRUE TO SEND,FALSE OTHERWISE>
 * LIGHTENABLED=<TRUE TO SEND,FALSE OTHERWISE>
 * NOISEENABLED=<TRUE TO SEND,FALSE OTHERWISE>
 * SNTPNAME=<NAME/IP OF SNTP SERVER>
 * SNTPPORT=<PORT SNTP SEVER>
 * MQTTUSER=<USESNAME IN THE FORM TENANT/USER, RECEIVED IN REGISTRATION>
 * MQTTPASSWORD=<PASSWORD, RECEIVED IN REGISTRATION>
 */


/* Suppressing Non const, non volatile static or external variable */
/* own header files */
#include "AppController.h"
#include "MQTTCfgParser.h"
#include "MQTTFlash.h"


/* system header files */
#include "BCDS_Basics.h"
#include "BCDS_Assert.h"
#include "BCDS_Retcode.h"

/* additional interface header files */
#include "ff.h"
#include "fs.h"

/* constant definitions ***************************************************** */
#define CFG_EMPTY                       ""
#define CFG_YES                         "YES"
#define CFG_NO                          "NO"

#define CFG_DEFAULT_STATIC              CFG_NO        /**< Network static IP Address Default*/
#define CFG_DEFAULT_STATICIP            ""            /**< Network static IP Address Default*/
#define CFG_DEFAULT_DNSSERVER           ""            /**< Network DNS server Default*/
#define CFG_DEFAULT_GATEWAY             ""            /**< Network gateway Default*/
#define CFG_DEFAULT_MASK                ""            /**< Network mask Default*/
#define CFG_DEFAULT_LWM2M_BINDING       "U"           /**< LWM2M binding by Default*/
#define CFG_DEFAULT_LWM2M_NOTIFIES      CFG_NO        /**< LWM2M binding by Default*/
#define CFG_TEST_MODE_MIX               "MIX"         /**< BLE is not activated */

#define CFG_DEFAULT_DRIVE               ""              /**< SD Card default Drive location */
#define CFG_FORCE_MOUNT                 UINT8_C(1)      /**< Macro to define force mount */
#define CFG_SEEK_FIRST_LOCATION         UINT8_C(0)      /**< File seek to the first location */
#define CFG_MAX_LINE_SIZE               UINT8_C(65)
#define ATT_IDX_SIZE					UINT8_C(17)

#define CFG_NUMBER_UINT8_ZERO           UINT8_C(0)    /**< Zero value */

#define CFG_WHITESPACE                  "\t\n\r "
#define CFG_SPACE                       "\t "

/* local variables ********************************************************** */

/** Variable containers for configuration values */
static char AttValues[ATT_IDX_SIZE][UINT8_C(50)];
static ConfigDataBuffer fileReadBuffer;
void MQTTCfgParser_List(const char* Title, uint8_t defaultsOnly);
static char *itoa (int value, char *result, int base);

/*
 * Configuration holder structure array
 */
static ConfigLine_T ConfigStructure[ATT_IDX_SIZE] = {
		{ A00Name, WLAN_SSID,CFG_FALSE, CFG_FALSE, AttValues[0] },
		{ A01Name, WLAN_PSK, CFG_FALSE, CFG_FALSE, AttValues[1] },
		{ A02Name, MQTT_BROKER_HOST_NAME, CFG_FALSE, CFG_FALSE, AttValues[2] },
		{ A03Name, STR_MQTT_BROKER_HOST_PORT, CFG_FALSE, CFG_FALSE,	AttValues[3] },
		{ A04Name, BOOL_TO_STR(DEFAULT_MQTTSECURE), CFG_FALSE, CFG_FALSE, AttValues[4] },
		{ A05Name, DEFAULT_MQTTUSERNAME, CFG_FALSE, CFG_FALSE,AttValues[5] },
		{ A06Name, DEFAULT_MQTTPASSWORD, CFG_FALSE, CFG_FALSE,AttValues[6] },
		{ A07Name, BOOL_TO_STR(DEFAULT_MQTTANONYMOUS), CFG_FALSE, CFG_FALSE,AttValues[7] },
		{ A08Name, DEFAULT_STR_STREAMRATE, CFG_FALSE, CFG_FALSE, AttValues[8] },
		{ A09Name, BOOL_TO_STR(DEFAULT_ACCELENABELED), CFG_FALSE, CFG_FALSE, AttValues[9] },
		{ A10Name, BOOL_TO_STR(DEFAULT_GYROENABELED), CFG_FALSE, CFG_FALSE, AttValues[10] },
		{ A11Name, BOOL_TO_STR(DEFAULT_MAGENABELED), CFG_FALSE, CFG_FALSE, AttValues[11] },
		{ A12Name, BOOL_TO_STR(DEFAULT_ENVENABELED), CFG_FALSE, CFG_FALSE, AttValues[12] },
		{ A13Name, BOOL_TO_STR(DEFAULT_LIGHTENABELED), CFG_FALSE, CFG_FALSE, AttValues[13]},
		{ A14Name, BOOL_TO_STR(DEFAULT_NOISEENABELED), CFG_FALSE, CFG_FALSE, AttValues[14]},
		{ A15Name, SNTP_SERVER_URL, CFG_FALSE, CFG_FALSE, AttValues[15]},
		{ A16Name, STR_SNTP_SERVER_PORT, CFG_FALSE, CFG_FALSE, AttValues[16]},
};


/* global variables ********************************************************* */

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
static const char* getAttValue(int index) {
	if (0 <= index && index < ATT_IDX_SIZE) {
		LOG_AT_TRACE(("MQTTCfgParser: Debugging attribute get: %i / %s \r\n",
				ConfigStructure[index].defined, ConfigStructure[index].attValue ));
		if (CFG_TRUE == ConfigStructure[index].defined) {
			return ConfigStructure[index].attValue;
		} else {
			return ConfigStructure[index].defaultValue;
		}
	}
	return "";
}

static void setAttValue(int index, char* value) {
	if (0 <= index && index < ATT_IDX_SIZE) {
		LOG_AT_TRACE(("MQTTCfgParser: Debugging attribute set: %i / %s \r\n",
				ConfigStructure[index].defined, ConfigStructure[index].attValue ));
		strcpy(ConfigStructure[index].attValue, value);
		ConfigStructure[index].defined = CFG_TRUE;
	}
}


/**
 * @brief extracts tokens from the input buffer , copy into the token buffer and returns its
 * size at tokensize
 *
 * @param[in] buffer
 *            The input buffer
 * @param[in] idxAtBuffer
 *            The index from which we have to look after tokens
 * @param[in] bufSize
 *            The size of the input buffer (param one)
 * @param[out] token
 *            The buffer that will contain the token (must be different to NULL )
 * @param[in] expTokenType
 *            expected token type awaited from attribute parser
 *
 * @param[out] tokenSize
 *            The pointer to the variable that receive the size of the buffer
 *
 * @return the TOKEN types found (TOKEN_TYPE_UNKNOWN is EOF)
 */
static TokensType_T GetToken(const char *buffer, uint16_t *idxAtBuffer,
		uint16_t bufSize, char *token, uint16_t *tokenSize,
		States_T expTokenType) {
	TokensType_T Result = TOKEN_TYPE_UNKNOWN;

	/* erase the OUTPUT token buffer*/
	memset(token, 0, *tokenSize);

	uint8_t index = UINT8_C(0);
	/* don't skip line end if next token is STAT_EXP_ATT_VALUE */
	/* support empty attribute values to deleted compiled defaults */
	const char *skipSet =
			(expTokenType == STAT_EXP_ATT_VALUE) ? CFG_SPACE : CFG_WHITESPACE;
	/* Bypass all chars in skip set */
	while ((*idxAtBuffer < bufSize)
			&& (NULL != strchr(skipSet, buffer[*idxAtBuffer]))) {
		*idxAtBuffer = *idxAtBuffer + 1;
	}

	if (expTokenType == STAT_EXP_ATT_NAME) {
		while (buffer[*idxAtBuffer] == '#') // Handling in-line comments for key value pair in the configuration file
		{
			while ((buffer[*idxAtBuffer] != '\n')

					&& (buffer[*idxAtBuffer] != '\r') && (*idxAtBuffer < bufSize)) // skip to EOL if since Start of comment tag '#' was found
			{
				*idxAtBuffer = *idxAtBuffer + 1;
			}
			while ((*idxAtBuffer < bufSize)
					&& (NULL != strchr(CFG_WHITESPACE, buffer[*idxAtBuffer]))) {
				*idxAtBuffer = *idxAtBuffer + 1;
			}
		}
	}

	/* is the next char the EQUAL sign */
	if (buffer[*idxAtBuffer] == '=') {
		/* YES */
		token[index] = '=';
		*idxAtBuffer = *idxAtBuffer + 1;
		index = index + 1;
		Result = TOKEN_EQUAL;
	} else {
		/* No , its a string ...*/
		if (expTokenType == STAT_EXP_ATT_VALUE) {
			while ((buffer[*idxAtBuffer] != '\r')
					&& (buffer[*idxAtBuffer] != '\n')
					&& (buffer[*idxAtBuffer] != '\t')
					&& (*idxAtBuffer < bufSize)) {
				if (index < *tokenSize - 1) {
					token[index] = buffer[*idxAtBuffer];
					*idxAtBuffer = *idxAtBuffer + 1;
					index = index + 1;
				}
			};
		} else {
			while ((buffer[*idxAtBuffer] != '\r')
					&& (buffer[*idxAtBuffer] != '\n')
					&& (buffer[*idxAtBuffer] != '\t')
					&& (buffer[*idxAtBuffer] != ' ')
					&& (buffer[*idxAtBuffer] != '=') && (*idxAtBuffer < bufSize)) {
				if (index < *tokenSize - 1) {
					token[index] = buffer[*idxAtBuffer];
					*idxAtBuffer = *idxAtBuffer + 1;
					index = index + 1;
				}
			};
		}
		/* Is this string is a known string ,i.e. attribute name ?*/
		for (uint8_t ConfigIndex = UINT8_C(0); ConfigIndex < ATT_IDX_SIZE;
				ConfigIndex++) {
			if (0 == strcmp(token, ConfigStructure[ConfigIndex].attName)) {
				Result = TOKEN_ATT_NAME;
				break;
			}
		}
		/* The string is not known one, so it's a value */
		if ((index > 0) && (TOKEN_TYPE_UNKNOWN == Result)) {
			Result = TOKEN_VALUE;
		}
	}
	/* At this point nothing has been found ! */
	if ((index == 0) && (TOKEN_TYPE_UNKNOWN == Result)) /* (index == 0) check was added due to :Warning 438: Last value assigned to variable 'index' (defined at line 218) not used*/
	{
		Result = TOKEN_EOF;
	}
	return (Result);
}

/**
 * @brief Parse the config file configurations to the Buffer
 *
 * @param[in] buffer
 *            The buffer containing the configuration file
 *
 * @param[in] bufSize
 *            The size of the buffer (first param)
 * @return CFG_TRUE if configuration file is correct and contains necessary attribute/values
 *
 */
static uint8_t MQTTCfgParser_Config(const char *buffer, uint16_t bufSize, uint8_t overwrite) {
	uint16_t IndexAtBuffer = UINT16_C(0);
	uint8_t Result = CFG_TRUE;
	States_T State = STAT_EXP_ATT_NAME;
	int8_t CurrentConfigToSet = INT8_C(0);
	char Token[CFG_MAX_LINE_SIZE] = { 0 };
	uint16_t OutputTokenSize = UINT16_C(0);
	while (CFG_TRUE == Result) {
		OutputTokenSize = CFG_MAX_LINE_SIZE;
		TokensType_T GetTokenType = GetToken(buffer, &IndexAtBuffer, bufSize,
				Token, &OutputTokenSize, State);
		if (GetTokenType == TOKEN_EOF)
			break;

		switch (State) {
		case STAT_EXP_ATT_NAME: {
			if (GetTokenType != TOKEN_ATT_NAME) {
				LOG_AT_ERROR(("MQTTCfgParser: Expecting attname at %u\r\n",
						(uint16_t) (IndexAtBuffer - strlen(Token))));
				Result = CFG_FALSE;
				break;
			}
			for (uint8_t i = UINT8_C(0); i < ATT_IDX_SIZE; i++) {
				if (strcmp(Token, ConfigStructure[i].attName) == UINT8_C(0)) {
					CurrentConfigToSet = i;
					State = STAT_EXP_ATT_EQUAL;
					break;
				}
			}

			break;
		}
		case STAT_EXP_ATT_EQUAL: {
			if (GetTokenType != TOKEN_EQUAL) {
				LOG_AT_ERROR(("MQTTCfgParser: Expecting sign '=' at %u\r\n",
						(uint16_t) (IndexAtBuffer - strlen(Token))));
				Result = CFG_FALSE;
				break;
			}
			State = STAT_EXP_ATT_VALUE;
			break;
		}
		case STAT_EXP_ATT_VALUE: {
			if (GetTokenType != TOKEN_VALUE) {
				LOG_AT_ERROR(("MQTTCfgParser: Expecting value string at %u\r\n",
						(uint16_t) (IndexAtBuffer - strlen(Token))));
				Result = CFG_FALSE;
				break;
			}

			strcpy(ConfigStructure[CurrentConfigToSet].attValue, Token);

			if (ConfigStructure[CurrentConfigToSet].defined == 0 || overwrite) {

				ConfigStructure[CurrentConfigToSet].defined = 1;
				State = STAT_EXP_ATT_NAME;

			} else {
				Result = CFG_FALSE;
				LOG_AT_ERROR(("MQTTCfgParser: Twice definition of attribute %s!\r\n",
						ConfigStructure[CurrentConfigToSet].attName));
			}
			break;
		}
		default:
			LOG_AT_WARNING(("MQTTCfgParser: Unexpected state %d \r\n", State));
			break;
		}
	}
	return Result;
}

void MQTTCfgParser_List(const char* Title, uint8_t defaultsOnly) {
	LOG_AT_DEBUG(("%s\r\n", Title));
	for (uint8_t i = UINT8_C(0); i < ATT_IDX_SIZE; i++) {
		if (CFG_FALSE == ConfigStructure[i].ignore) {
			if (CFG_TRUE == ConfigStructure[i].defined) {
				LOG_AT_DEBUG(("[%19s] = [%s]\r\n", ConfigStructure[i].attName,
						ConfigStructure[i].attValue));
			} else {
				if (CFG_TRUE == defaultsOnly) {
					if (0 != *ConfigStructure[i].defaultValue) {
						LOG_AT_DEBUG(("[%19s] * [%s]\r\n", ConfigStructure[i].attName,
								ConfigStructure[i].defaultValue));
					}
				} else {
					LOG_AT_DEBUG(("[%19s] * [%s] (DEFAULT)\r\n",
							ConfigStructure[i].attName,
							ConfigStructure[i].defaultValue));
				}
			}
		} else if (CFG_TRUE == ConfigStructure[i].defined) {
			LOG_AT_WARNING(("[%19s] is deprecated and should be removed\r\n",
					ConfigStructure[i].attName));
		}
	}
}

void MQTTCfgParser_GetConfig(ConfigDataBuffer *configBuffer, uint8_t defaultsOnly) {
	for (uint8_t i = UINT8_C(0); i < ATT_IDX_SIZE; i++) {
		if (CFG_FALSE == ConfigStructure[i].ignore) {
			if (CFG_TRUE == ConfigStructure[i].defined) {
				configBuffer->length += sprintf(configBuffer->data + configBuffer->length,"%s=%s\n", ConfigStructure[i].attName,
						ConfigStructure[i].attValue);
			} else {
				if (CFG_TRUE == defaultsOnly) {
					if (0 != *ConfigStructure[i].defaultValue) {
						configBuffer->length += sprintf(configBuffer->data + configBuffer->length, "%s=%s\n", ConfigStructure[i].attName,
								ConfigStructure[i].defaultValue);

					}
				} else {
					configBuffer->length += sprintf(configBuffer->data + configBuffer->length, "%s=%s\n",
							ConfigStructure[i].attName,
							ConfigStructure[i].defaultValue);
				}
			}
		} else if (CFG_TRUE == ConfigStructure[i].defined) {
			LOG_AT_WARNING(("[%19s] is deprecated and should be removed\r\n",
					ConfigStructure[i].attName));
		}
	}
}


/** @brief For description of the function please refer interface header MQTTCfgParser.h  */
APP_RESULT MQTTCfgParser_ParseConfigFile(void) {
	APP_RESULT RetValFLash = APP_RESULT_ERROR;
	APP_RESULT RetVal = APP_RESULT_ERROR;

	fileReadBuffer.length = NUMBER_UINT32_ZERO;
	memset(fileReadBuffer.data, CFG_NUMBER_UINT8_ZERO, SIZE_XLARGE_BUF);

	RetValFLash = MQTTFlash_FLReadConfig(&fileReadBuffer);
	if (RetValFLash != APP_RESULT_FILE_MISSING) {
		//config on flash exists
		if (CFG_TRUE
				== MQTTCfgParser_Config((const char*) fileReadBuffer.data,
						fileReadBuffer.length, CFG_FALSE)) {
			RetValFLash = APP_RESULT_OPERATION_OK;
		}
		MQTTCfgParser_List("MQTTCfgParser: Parsing content from config file [config.txt] on WIFI chip:", CFG_TRUE);
	}

	// test if config on SDCard exists and overwrite setting from config on flash
	LOG_AT_INFO(("MQTTCfgParser_ParseConfigFile: Trying to read config from SDCard ...\r\n"));
	fileReadBuffer.length = NUMBER_UINT32_ZERO;
	memset(fileReadBuffer.data, CFG_NUMBER_UINT8_ZERO, SIZE_XLARGE_BUF);
	RetVal = MQTTFlash_SDReadConfig(&fileReadBuffer);

	if (RetVal == APP_RESULT_OPERATION_OK ) {
		// append \n , since parser only parses complete lines
		fileReadBuffer.length += snprintf (fileReadBuffer.data + strlen(fileReadBuffer.data), sizeof (fileReadBuffer.data) - strlen(fileReadBuffer.data) , "\n");
		if (CFG_TRUE
				== MQTTCfgParser_Config((const char*)  fileReadBuffer.data,
						fileReadBuffer.length, CFG_TRUE)) {
			RetVal = APP_RESULT_OPERATION_OK;
		}
		MQTTCfgParser_List("MQTTCfgParser: Parsing content from config file [config.txt] on SD card (potentially merged):", CFG_TRUE);
	} else {
		LOG_AT_ERROR(("MQTTCfgParser: Config not read from SD card!\r\n"));
	}

	// if any of the attemps to parse a config: Flash or SD was successful return OK
	if (RetVal == APP_RESULT_OPERATION_OK || RetValFLash == APP_RESULT_OPERATION_OK) {
		RetVal = APP_RESULT_OPERATION_OK;
	}
	return RetVal;
}

/**
 * @brief returns the WLAN SSID defined at the configuration file
 *
 * @return WLAN SSID
 */
const char *MQTTCfgParser_GetWlanSSID(void) {
	return getAttValue(ATT_IDX_WIFISSID);
}
/**
 * @brief returns the PASSWORD defined by the attribute PASSWORD of the configuration file
 *
 * @return WLAN PASSWORD
 */
const char *MQTTCfgParser_GetWlanPassword(void) {
	return getAttValue(ATT_IDX_WIFIPASSWORD);
}

const char *MQTTCfgParser_GetMqttBrokerName(void) {
	return getAttValue(ATT_IDX_MQTTBROKERNAME);
}

int32_t MQTTCfgParser_GetMqttBrokerPort(void) {
	return (int32_t) atol(getAttValue(ATT_IDX_MQTTBROKERPORT));
}

bool MQTTCfgParser_IsMqttSecureEnabled(void) {
	const char* value = getAttValue(ATT_IDX_MQTTSECURE);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}

bool MQTTCfgParser_IsMqttAnonymous(void) {
	const char* value = getAttValue(ATT_IDX_MQTTANONYMOUS);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}


const char *MQTTCfgParser_GetMqttUser(void) {
	return getAttValue(ATT_IDX_MQTTUSER);
}

const char *MQTTCfgParser_GetMqttPassword(void) {
	return getAttValue(ATT_IDX_MQTTPASSWORD);
}

void MQTTCfgParser_SetMqttUser(char * user) {
	setAttValue(ATT_IDX_MQTTUSER, user);
}

void MQTTCfgParser_SetMqttPassword(char * password) {
	setAttValue(ATT_IDX_MQTTPASSWORD, password);
}


int32_t MQTTCfgParser_GetStreamRate(void) {
	int32_t s = (int32_t) atol(getAttValue(ATT_IDX_STREAMRATE));
	return s ;
}

void MQTTCfgParser_SetStreamRate(int32_t rate) {
	char token[CFG_MAX_LINE_SIZE] = { 0 };
	itoa (rate, token , 10);
	setAttValue(ATT_IDX_STREAMRATE, token);
}

void MQTTCfgParser_SetSensor(const char* value, int index) {
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		setAttValue(index, "TRUE");
	else
		setAttValue(index, "FALSE");
}

bool MQTTCfgParser_IsAccelEnabled(void) {
	const char* value = getAttValue(ATT_IDX_ACCELENABLED);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}

bool MQTTCfgParser_IsGyroEnabled(void) {
	const char* value = getAttValue(ATT_IDX_GYROENABLED);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}

bool MQTTCfgParser_IsMagnetEnabled(void) {
	const char* value = getAttValue(ATT_IDX_MAGENABLED);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}

bool MQTTCfgParser_IsEnvEnabled(void) {
	const char* value = getAttValue(ATT_IDX_ENVENABLED);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}

bool MQTTCfgParser_IsLightEnabled(void) {
	const char* value = getAttValue(ATT_IDX_LIGHTENABLED);
	bool res = false;
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		res = true;
	else
		res = false;
	return res;
}

bool MQTTCfgParser_IsNoiseEnabled(void) {
	const char* value = getAttValue(ATT_IDX_NOISEENABLED);
	if (strcmp(value,"TRUE") == 0 || strcmp(value,"1") == 0 )
		return true;
	else
		return false;
}

void MQTTCfgParser_FLWriteConfig(void) {
	// update config in flash memory
	ConfigDataBuffer localbuffer;
	localbuffer.length = NUMBER_UINT32_ZERO;
	memset(localbuffer.data, 0x00, SIZE_XLARGE_BUF);
	MQTTCfgParser_GetConfig(&localbuffer, CFG_FALSE);
	MQTTFlash_FLWriteConfig(&localbuffer);
}

/**
 * @brief returns the Server name for SNTP defined at the configuration file
 *
 * @return Server name SNTP
 */
const char *MQTTCfgParser_GetSntpName(void) {
	return getAttValue(ATT_IDX_SNTPNAME);
}


/**
 * @brief returns the Server port for SNTP defined at the configuration file
 *
 * @return Server port SNTP
 */
int32_t MQTTCfgParser_GetSntpPort(void) {
	return (int32_t) atol(getAttValue(ATT_IDX_SNTPPORT));
}

APP_RESULT MQTTCfgParser_Init(void) {
	/* Initialize the attribute values holders */
	for (uint8_t i = UINT8_C(0); i < ATT_IDX_SIZE; i++) {
		ConfigStructure[i].defined = CFG_FALSE;
		memset(ConfigStructure[i].attValue, CFG_NUMBER_UINT8_ZERO,
				CFG_MAX_LINE_SIZE);
		if (NULL == ConfigStructure[i].defaultValue) {
			ConfigStructure[i].defaultValue = CFG_EMPTY;
		}
	}

	if (APP_RESULT_OPERATION_OK != MQTTCfgParser_ParseConfigFile())
	{
		LOG_AT_ERROR(("MQTTCfgParser: Config file is not correct. Please give a proper config file and reboot the device!\r\n"));
		return APP_RESULT_ERROR;
	}

	if (strcmp(MQTTCfgParser_GetMqttUser(), DEFAULT_MQTTUSERNAME) == UINT8_C(0)) {
		return APP_RESULT_REGISTRATION_MODE;
	} else {
		return APP_RESULT_OPERATION_MODE;
	}
}

static char *itoa (int value, char *result, int base)
{
	// check that the base if valid
	if (base < 2 || base > 36) { *result = '\0'; return result; }

	char* ptr = result, *ptr1 = result, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
	} while ( value );

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}
