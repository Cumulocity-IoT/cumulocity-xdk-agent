/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTFlash.h
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

#define DEFAULT_LOGICAL_DRIVE        ""             /**< Macro to define default logical drive */
#define FORCE_MOUNT                 UINT8_C(1)      /**< Macro to define force mount */
#define WRITEREAD_DELAY 			UINT32_C(5000) 	 /**< Millisecond delay for WriteRead timer task */
#define WRITEREAD_BLOCK_TIME 		UINT32_C(0xffff) /**< Macro used to define block time of a timer*/
#define SINGLE_BLOCK				UINT8_C(1)      /**< SD- Card Single block write or read */
#define DRIVE_ZERO				    UINT8_C(0)      /**< SD Card Drive 0 location */
#define PARTITION_RULE_FDISK	    UINT8_C(0)      /**< SD Card Drive partition rule 0: FDISK, 1: SFD */
#define AUTO_CLUSTER_SIZE		    UINT8_C(0)      /**< zero is given, the cluster size is determined depends on the volume size. */
#define SEEK_FIRST_LOCATION		    UINT8_C(0)      /**< File seek to the first location */
#define SECTOR_VALUE			    UINT8_C(1)      /**< SDC Disk sector value */

#define REBOOT_FILENAME    "reboot.txt"	/**< Filename to open/write/read from SD-card */
#define CONFIG_FILENAME  "config.txt"	/**< Filename to open/write/read from SD-card */

/* Ram buffers
 * BUFFERSIZE should be between 512 and 1024, depending on available ram on efm32
 */
#define FILE_TINY_BUFFER_SIZE          UINT16_C(128)
#define FILE_SMALL_BUFFER_SIZE         UINT16_C(256)
#define FILE_EQUAL 		     UINT8_C(0)
#define FILE_NOT_EQUAL 	     UINT8_C(1)

#define SDCARD_DRIVE_NUMBER  UINT8_C(0)           /**< SD Card Drive 0 location */
/**
 * @brief
 *      The sdCardFatFileSystemWriteRead API uses the FAT file system library calls.
 *      This API will mount and create the file system, then it will open, seek write,
 *      read and close the files. This API will compare the contents which has been
 *      written and read.
 *
 * @retval
 *      ERR_NO_ERROR - All the file operations are success
 *
 * @retval
 *      ERR_ERROR - one of the file operation failed
 *
 * @retval
 *      APP_ERR_INIT_FAILED - disk has not been initialized
 */
APP_RESULT MQTTFlash_Init(void);
APP_RESULT MQTTFlash_FLReadBootStatus(uint8_t* status);
void MQTTFlash_FLWriteBootStatus(uint8_t* status);
void MQTTFlash_FLWriteConfig(ConfigDataBuffer *configBuffer);
APP_RESULT MQTTFlash_FLReadConfig(ConfigDataBuffer* configBuffer);
void MQTTFlash_FLDeleteConfig(void);
void MQTTFlash_SDAppendCredentials(char* stringBuffer);
APP_RESULT MQTTFlash_SDWrite(const uint8_t* fileName, uint8_t* stringBuffer, BYTE mode);
APP_RESULT MQTTFlash_SDRead(const uint8_t* fileName, ConfigDataBuffer * ramBufferRead, uint16_t maxBufferSize);

/* local inline function definitions */


#endif /* MQTTFLASH_H_ */
