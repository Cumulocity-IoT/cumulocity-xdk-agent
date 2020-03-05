/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTStorage.h
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

#ifndef MQTTFLASH_H_
#define MQTTFLASH_H_

#include <stdint.h>
#include "integer.h"


/* local interface declaration ********************************************** */

/* local type and macro definitions */
/**
 *  @brief Error code returned by APIs of SD card application
 */

typedef enum {
	FR_Z_OK = 0,				/* (0) Succeeded */
	FR_Z_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_Z_INT_ERR,				/* (2) Assertion failed */
	FR_Z_NOT_READY,			/* (3) The physical drive cannot work */
	FR_Z_NO_FILE,				/* (4) Could not find the file */
	FR_Z_NO_PATH,				/* (5) Could not find the path */
	FR_Z_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_Z_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_Z_EXIST,				/* (8) Access denied due to prohibited access */
	FR_Z_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_Z_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_Z_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_Z_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_Z_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_Z_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any parameter error */
	FR_Z_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_Z_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_Z_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_Z_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > _FS_LOCK */
	FR_Z_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} F_Z_RESULT;

#define REBOOT_FILENAME		"reboot.txt"	/**< Filename to open/write/read from SD-card */
#define CONFIG_FILENAME  	"config.txt"	/**< Filename to open/write/read from SD-card */

Retcode_T MQTTFlash_Init(void);
Retcode_T MQTTFlash_FLReadBootStatus(uint8_t* status);
Retcode_T MQTTFlash_FLWriteBootStatus(uint8_t* status);
void MQTTFlash_FLWriteConfig(ConfigDataBuffer *configBuffer);
Retcode_T MQTTFlash_FLReadConfig(ConfigDataBuffer* configBuffer);
Retcode_T MQTTFlash_SDReadConfig(ConfigDataBuffer* configBuffer);
void MQTTFlash_FLDeleteConfig(void);
void MQTTFlash_SDAppendCredentials(char* stringBuffer);

/* local inline function definitions */


#endif /* MQTTFLASH_H_ */
