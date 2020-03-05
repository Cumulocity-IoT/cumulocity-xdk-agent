/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	CfgParser.h
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

/* header definition ******************************************************** */
#ifndef MQTTCFGPARSER_H_
#define MQTTCFGPARSER_H_

/* public interface declaration ********************************************* */
/* system header files */
#include <BCDS_Retcode.h>
#include <BCDS_Basics.h>

/* public type and macro definitions */

#define CFG_NUMBER_UINT8_ZERO            UINT8_C(0)     /**< Zero value */
#define CFG_POINTER_NULL                 NULL          /**< ZERO value for pointers */

#define CFG_TRUE                         UINT8_C(1)    /**< One value  */
#define CFG_FALSE                        UINT8_C(0)    /**< Zero value */

#define CFG_TESTMODE_OFF                 UINT8_C(0)
#define CFG_TESTMODE_ON                  UINT8_C(1)
#define CFG_TESTMODE_MIX                 UINT8_C(2)

#define ATT_IDX_SIZE					UINT8_C(20)
#define ATT_KEY_LENGTH					UINT8_C(20)

#define BOOL_TO_STR(x) ((x) ? "TRUE" : "FALSE")
/**
 * Configuration array cell element
 */
struct ConfigLine_S
{
    const char *attName; /**< Attribute name at the configuration file */
    const char *defaultValue; /**< Attribute default value */
    uint8_t ignore; /**< To specify if the attribute is ignored/deprecated */
    uint8_t defined; /**< To specify if the attribute has been read from the configuration file */
    char *attValue; /**< Attribute value  at the configuration file */
};

typedef struct ConfigLine_S ConfigLine_T;

/**
 * INDEX at the configuration array
 */

static const char ATT_KEY_NAME[ATT_IDX_SIZE][ATT_KEY_LENGTH] = {
		"WIFISSID","WIFIPASSWORD","MQTTBROKERNAME","MQTTBROKERPORT",
		"MQTTSECURE","MQTTUSER","MQTTPASSWORD","MQTTANONYMOUS",
		"STREAMRATE","ACCEL","GYRO","MAG",
		"ENV", "LIGHT","NOISE","SNTPNAME",
		"SNTPPORT","FIRMWARENAME","FIRMWAREVERSION","FIRMWAREURL"};


enum AttributesIndex_E
{
    ATT_IDX_WIFISSID,
    ATT_IDX_WIFIPASSWORD,
    ATT_IDX_MQTTBROKERNAME,
    ATT_IDX_MQTTBROKERPORT,
    ATT_IDX_MQTTSECURE,
    ATT_IDX_MQTTUSER,
    ATT_IDX_MQTTPASSWORD,
    ATT_IDX_MQTTANONYMOUS,
    ATT_IDX_STREAMRATE,
    ATT_IDX_ACCEL,
    ATT_IDX_GYRO,
    ATT_IDX_MAG,
    ATT_IDX_ENV,
    ATT_IDX_LIGHT,
    ATT_IDX_NOISE,
	ATT_IDX_SNTPNAME,
	ATT_IDX_SNTPPORT,
	ATT_IDX_FIRMWARENAME,
	ATT_IDX_FIRMWAREVERSION,
	ATT_IDX_FIRMWAREURL
};

typedef enum AttributesIndex_E AttributesIndex_T;
/**
 *  The possible token types
 */

enum TokensType_E
{
    TOKEN_TYPE_UNKNOWN,
    TOKEN_EOF,
    TOKEN_ATT_NAME,
    TOKEN_EQUAL,
    TOKEN_VALUE,
};

typedef enum TokensType_E TokensType_T;
/**
 *  The states on the configuration file parser states machine
 */
enum States_E
{
    STAT_EXP_ATT_NONE,
    STAT_EXP_ATT_NAME,
    STAT_EXP_ATT_EQUAL,
    STAT_EXP_ATT_VALUE,
};

typedef enum States_E States_T;

/**
 * To represent possible conditional values of the token in configuration file
 */
enum CfgParser_ConditionalValues_E
{
    CGF_CONDITIONAL_VALUE_NOT_DEFINED = 0,
    CGF_CONDITIONAL_VALUE_YES,
    CGF_CONDITIONAL_VALUE_NO,
    CGF_CONDITIONAL_VALUE_OUT_OF_CHOICE,
};

typedef enum CfgParser_ConditionalValues_E CfgParser_ConditionalValues_T;

enum Retcode_CfgParser_E
{
    RETCODE_CFG_PARSER_SD_CARD_MOUNT_ERROR = RETCODE_FIRST_CUSTOM_CODE,
    RETCODE_CFG_PARSER_SD_CARD_FILE_NOT_EXIST,
    RETCODE_CFG_PARSER_SD_CARD_CONFIG_DATA_WRONG,
    RETCODE_CFG_PARSER_SD_CARD_FILE_CLOSE_ERROR,
};

/* local function prototype declarations */

/* public function prototype declarations */



/**
 * @brief initialize configuration parser.
 */
Retcode_T MQTTCfgParser_Init(void);

/**
 * @brief return boot status fo device
 */
APP_STATUS MQTTCfgParser_GetMode(void);

/**
 * @brief   Print list of configured values.
 * @param[in] Title title for list
 * @param[in] defaultsOnly list only default values
 */
void MQTTCfgParser_List(const char* Title, uint8_t defaultsOnly);


/**
 * @brief   Print list of configured values.
 * @param[in] config buffer where current values are added
 * @param[in] defaultsOnly list only default values
 */
void MQTTCfgParser_GetConfig(ConfigDataBuffer *config, uint8_t defaultsOnly);


/**
 * @brief   Function parses the Configuration file if present Typedef to represent the Conditional Value of the token
 * @return RETCODE_OK - if the parsing is completed successfully
 *
 * @note: Precondition before calling this function user has to ensure that SD card is inserted
 * and config.txt file is available
 */

Retcode_T MQTTCfgParser_ParseConfigFile(void);

/**
 * @brief returns the attribute value for the token SSID as defined at the configuration file
 *
 * If attribute is not defined in configuration file it returns empty string
 */
const char *MQTTCfgParser_GetWlanSSID(void);

/**
 * @brief returns attribute value for the token PASSWORD as defined at the configuration file
 *
 *   If attribute is not defined in configuration file it returns empty string
 */
const char *MQTTCfgParser_GetWlanPassword(void);

const char *MQTTCfgParser_GetMqttBrokerName(void);

int32_t MQTTCfgParser_GetMqttBrokerPort(void);

bool MQTTCfgParser_IsMqttSecureEnabled(void);

bool MQTTCfgParser_IsMqttAnonymous(void);

const char *MQTTCfgParser_GetMqttUser(void);

const char *MQTTCfgParser_GetMqttPassword(void);

void MQTTCfgParser_SetMqttUser(char * user);

void MQTTCfgParser_SetMqttPassword(char * password);

int32_t MQTTCfgParser_GetStreamRate(void);

void MQTTCfgParser_SetStreamRate(int32_t rate);

void MQTTCfgParser_SetSensor(const char* value, int index);

bool MQTTCfgParser_IsAccelEnabled(void);

bool MQTTCfgParser_IsGyroEnabled(void);

bool MQTTCfgParser_IsMagnetEnabled(void);

bool MQTTCfgParser_IsEnvEnabled(void);

bool MQTTCfgParser_IsLightEnabled(void);

bool MQTTCfgParser_IsNoiseEnabled(void);

void MQTTCfgParser_FLWriteConfig(void);

const char *MQTTCfgParser_GetSntpName(void);

void MQTTCfgParser_SetFirmwareName(char * name);

void MQTTCfgParser_SetFirmwareVersion(char * name);

void MQTTCfgParser_SetFirmwareURL(char * name);

void MQTTCfgParser_SetConfig(const char * value, int index);

const char *MQTTCfgParser_GetFirmwareName(void);

const char *MQTTCfgParser_GetFirmwareVersion(void);

const char *MQTTCfgParser_GetFirmwareURL(void);

int32_t MQTTCfgParser_GetSntpPort(void);

/* inline function definitions */

#endif /* MQTTCFGPARSER_H_ */


/** ************************************************************************* */
