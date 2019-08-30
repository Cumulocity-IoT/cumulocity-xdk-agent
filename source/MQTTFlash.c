/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTFlash.c
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

#include "AppController.h"
#include "MQTTFlash.h"
#include "MQTTCfgParser.h"
#include "XDK_Storage.h"
#include "WifiStorage.h"

#include "BCDS_SDCard_Driver.h"
#include <stdio.h>
#include <ff.h>

/* global variables ********************************************************* */

/* local module global variable declarations */

APP_RESULT MQTTFlash_Init(void) {
	/* read boot status */
	uint8_t bootstatus[FILE_TINY_BUFFER_SIZE] = { 0 };
	if (MQTTFlash_ReadBootStatus(bootstatus) != APP_OPERATION_OK) {
		//probably the first time this XDK is used with C8Y, so we need to initialize the reboot.txt file
		MQTTFlash_WriteBootStatus(NO_BOOT_PENDING);
		if (MQTTFlash_ReadBootStatus(bootstatus) != APP_OPERATION_OK)
		{
			return APP_ERROR;
		}

	}
	printf("MQTTFlash: Reading boot status: [%s]\n\r", bootstatus);

    return APP_OPERATION_OK;

}

void MQTTFlash_AppendCredentials(char* stringBuffer) {
	if (DEBUG_LEVEL <= FINE)
		printf("MQTTFlash: Received credentials:%s\n\r", stringBuffer);
	if (SDCARD_INSERTED == SDCardDriver_GetDetectStatus()) {
		if (DEBUG_LEVEL <= FINE)
			printf("MQTTFlash: SD card is inserted in XDK\n\r");

		/* SDC Disk FAT file system Write/Read functionality */
		if (MQTTFlash_SDCardWrite(stringBuffer, CONFIG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE) == APP_SDCARD_NO_ERROR) {
			if (DEBUG_LEVEL <= FINEST)
				printf("MQTTFlash: Write using FAT file system success \n\r");
		} else {
			if (DEBUG_LEVEL <= EXCEPTION)
				printf("MQTTFlash: Write using FAT file system failed\n\r");
			assert(0);
		}
	} else {
		if (DEBUG_LEVEL <= EXCEPTION)
			printf("MQTTFlash: SD card is not inserted in XDK\n\r");
		assert(0);
	}
}

APP_RESULT MQTTFlash_ReadBootStatus(uint8_t* bootstatus) {
    Storage_Read_T readCredentials =
            {
                    .FileName = BOOT_FILENAME,
                    .ReadBuffer = bootstatus,
					.BytesToRead = 0UL,
                    .ActualBytesRead = 0UL,
                    .Offset = 0UL,
            };
    Retcode_T retcode = RETCODE_OK;
    bool status = false;

    /* Validating if wifi storage medium is available */
    retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
    if ((RETCODE_OK == retcode) && (true == status)) {
    	retcode = WifiStorage_GetFileStatus((const uint8_t*) &(BOOT_FILENAME), &(readCredentials.BytesToRead));
		if (retcode == RETCODE_OK) {
			retcode = Storage_Read(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &readCredentials);
			if (retcode == RETCODE_OK) {
				printf("MQTTFlash: Read using FAT file system success: [%s] \n\r", readCredentials.ReadBuffer);
				 return APP_OPERATION_OK;
			}
		}

    }
	printf("MQTTFlash: Read using FAT file system failed!\n\r");
	Retcode_RaiseError(retcode);
	return APP_INIT_FILE_MISSING;
}

void MQTTFlash_WriteBootStatus(uint8_t* bootstatus) {

    Storage_Write_T writeCredentials =
            {
                    .FileName = BOOT_FILENAME,
                    .WriteBuffer = bootstatus,
                    .BytesToWrite = strlen(bootstatus) + 1,
                    .ActualBytesWritten = 0UL,
                    .Offset = 0UL,
            };

    Retcode_T retcode = RETCODE_OK;
    bool status = false;

    /* Validating if wifi storage medium is available */
    retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
    if ((RETCODE_OK == retcode) && (true == status))
    {
		/* Since WIFI_CONFIG_FILE_NAME is available, copying it to the WiFi file system */
		retcode = Storage_Write(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &writeCredentials);
		if (RETCODE_OK == retcode)
		{
			printf("MQTTFlash: Init boot status: [%s]\n\r", bootstatus);

		} else {
			printf("MQTTFlash: Write boot status failed!\n\r");
			assert(0);
		}
    }

}


APP_RESULT MQTTFlash_SDCardRead(int8_t* fileName, int8_t* ramBufferRead, uint16_t maxBufferSize, UINT* bytesRead) {
	FIL fileObject; /* File objects */
	uint16_t fileSize;
	FILINFO fileInfo;

	if (f_stat(fileName, &fileInfo) != FR_Z_OK) {
		if (DEBUG_LEVEL <= EXCEPTION)
			printf("MQTTFlash: Can't find file: [%s]!\n\r",fileName );
		return (APP_SDCARD_ERROR);
	}
	fileSize = fileInfo.fsize;

	if (fileSize > maxBufferSize) {
		printf("MQTTFlash: Read size: %i to big: %hu\n\r", fileSize, maxBufferSize);
		return (APP_SDCARD_BUFFER_OVERFLOW_ERROR);
	} else {
		if (DEBUG_LEVEL <= FINEST)
			printf("MQTTFlash: Read file: [%s] with size: %i\n\r", fileName, fileSize);
	}

	/* Open the file for read */
	if (f_open(&fileObject, fileName, FA_READ) != FR_Z_OK) {
		if (DEBUG_LEVEL <= EXCEPTION)
			printf("MQTTFlash: Opening file failed\n\r");
		return (APP_SDCARD_ERROR);
	}

	/* Set the file read pointer to first location */
	if (f_lseek(&fileObject, 0) != FR_Z_OK) {
		/* Error. Cannot set the file pointer */
		if (DEBUG_LEVEL <= EXCEPTION)
			printf("MQTTFlash: Seeking file failed\n\r");
		return (APP_SDCARD_ERROR);
	}

	/*Read some data from file */
	if ((f_read(&fileObject, ramBufferRead, fileSize, bytesRead) == FR_Z_OK) && (fileSize == *bytesRead)) {
		if (DEBUG_LEVEL <= FINEST)
			printf("MQTTFlash: Reading from succeded: %i %i\n\r",
					fileSize, *bytesRead);
	} else {
		/* Error. Cannot read the file */
			printf("MQTTFlash: Read failed with different size : %i %i\n\r",
					fileSize, *bytesRead);
		return (APP_SDCARD_ERROR);
	}
	if (DEBUG_LEVEL <= FINEST)
		printf("MQTTFlash: Read content from status file: %s\n\r", ramBufferRead);

	/* Close the file */
	if (f_close(&fileObject) != FR_Z_OK) {
		return (APP_SDCARD_ERROR);
	}
	return APP_SDCARD_NO_ERROR;
}

APP_RESULT MQTTFlash_SDCardWrite(int8_t* stringBuffer, int8_t* fileName, BYTE mode ) {
	FIL fileObject; /* File objects */
	int8_t ramBufferWrite[FILE_SMALL_BUFFER_SIZE]; /* Temporary buffer for write file */
	uint16_t fileSize;
	UINT bytesWritten;

	/* Initialization of file buffer write */
	fileSize = strlen(stringBuffer);
	if (DEBUG_LEVEL <= FINE)
		printf("MQTTFlash: Size of buffer: [%i]\n\r", fileSize);
	memcpy(ramBufferWrite, stringBuffer, fileSize);

	/* Open the file to write */
	if (f_open(&fileObject, fileName, mode) != FR_Z_OK) {
		/* Error. Cannot create the file */
		printf("MQTTFlash: Error creating status");
		return (APP_SDCARD_ERROR);
	}

	/* Set the file write pointer at th eend to append */
	if (f_lseek(&fileObject, f_size(&fileObject)) != FR_Z_OK) {
		/* Error. Cannot set the file write pointer */
		printf("MQTTFlash: Error accessing file");
		return (APP_SDCARD_ERROR);
	}

	/* Write a buffer to file*/
	if ((f_write(&fileObject, ramBufferWrite, fileSize, &bytesWritten) != FR_Z_OK) || (fileSize != bytesWritten)) {
		/* Error. Cannot write the file */
		printf("MQTTFlash: Error writing in file");
		return (APP_SDCARD_ERROR);
	}

	/* Close the file */
	if (f_close(&fileObject) != FR_Z_OK) {
		/* Error. Cannot close the file */
		printf("MQTTFlash: Error closing file");
		return (APP_SDCARD_ERROR);
	}

	return APP_SDCARD_NO_ERROR;
}

