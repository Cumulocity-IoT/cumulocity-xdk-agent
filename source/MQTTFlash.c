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
static void MQTTFlash_FLDeleteFile (const uint8_t *fileName);
static APP_RESULT MQTTFlash_SDWrite(const uint8_t *fileName, uint8_t *stringBuffer, BYTE mode);
static APP_RESULT MQTTFlash_SDRead(const uint8_t *fileName, ConfigDataBuffer *ramBufferRead);

APP_RESULT MQTTFlash_Init(void) {
	/* read boot status */
	uint8_t bootstatus[SIZE_XSMALL_BUF] = { 0 };
	if (MQTTFlash_FLReadBootStatus(bootstatus) != APP_RESULT_OPERATION_OK) {
		//probably the first time this XDK is used with C8Y, so we need to initialize the reboot.txt file
		MQTTFlash_FLWriteBootStatus((uint8_t*) NO_BOOT_PENDING);
		if (MQTTFlash_FLReadBootStatus(bootstatus) != APP_RESULT_OPERATION_OK)
		{
			return APP_RESULT_ERROR;
		}

	}
	LOG_AT_ERROR(("MQTTFlash: Reading boot status: [%s]\r\n", bootstatus));

	return APP_RESULT_OPERATION_OK;

}

void MQTTFlash_SDAppendCredentials(char* stringBuffer) {
	LOG_AT_DEBUG(("MQTTFlash: Received credentials:%s\r\n", stringBuffer));
	if (SDCARD_INSERTED == SDCardDriver_GetDetectStatus()) {
		LOG_AT_DEBUG(("MQTTFlash: SD card is inserted in XDK\r\n"));
		/* SDC Disk FAT file system Write/Read functionality */
		if (MQTTFlash_SDWrite((const uint8_t*) CONFIG_FILENAME, stringBuffer, FA_OPEN_ALWAYS | FA_WRITE) == APP_RESULT_OPERATION_OK) {
			LOG_AT_TRACE ( ("MQTTFlash: Write using FAT file system success \r\n") );
		} else {
			LOG_AT_ERROR( ("MQTTFlash: Write using FAT file system failed\r\n") );
			assert(0);
		}
	} else {
		LOG_AT_ERROR( ("MQTTFlash: SD card is not inserted in XDK\r\n") );
		assert(0);
	}
}

APP_RESULT MQTTFlash_FLReadBootStatus(uint8_t* bootstatus) {
	Storage_Read_T readCredentials =
	{
			.FileName = REBOOT_FILENAME,
			.ReadBuffer = bootstatus,
			.BytesToRead = 0UL,
			.ActualBytesRead = 0UL,
			.Offset = 0UL,
	};
	Retcode_T retcode = RETCODE_OK;
	bool status = false;

	/* Validating if WIFI storage medium is available */
	retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
	if ((RETCODE_OK == retcode) && (true == status)) {
		retcode = WifiStorage_GetFileStatus((const uint8_t*) &(REBOOT_FILENAME), &(readCredentials.BytesToRead));
		if (retcode == RETCODE_OK) {
			retcode = Storage_Read(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &readCredentials);
			if (retcode == RETCODE_OK) {
				LOG_AT_TRACE(("MQTTFlash: Read using FAT file system success: [%s] \r\n", readCredentials.ReadBuffer));
				return APP_RESULT_OPERATION_OK;
			}
		}

	}
	LOG_AT_ERROR(("MQTTFlash: Read using flash file system failed!\r\n"));
	Retcode_RaiseError(retcode);
	return APP_RESULT_FILE_MISSING;
}

APP_RESULT MQTTFlash_FLWriteBootStatus(uint8_t* bootstatus) {

	Storage_Write_T writeCredentials =
	{
			.FileName = REBOOT_FILENAME,
			.WriteBuffer = bootstatus,
			.BytesToWrite = strlen(bootstatus) + 1,
			.ActualBytesWritten = 0UL,
			.Offset = 0UL,
	};

	Retcode_T retcode = RETCODE_OK;
	bool status = false;

	/* Validating if WIFI storage medium is available */
	retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
	if ((RETCODE_OK == retcode) && (true == status))
	{
		/* Since WIFI_CONFIG_FILE_NAME is available, copying it to the WIFI file system */
		retcode = Storage_Write(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &writeCredentials);
		if (RETCODE_OK == retcode)
		{
			LOG_AT_DEBUG(("MQTTFlash: Init boot status: [%s]\r\n", bootstatus));

		} else {
			LOG_AT_ERROR(("MQTTFlash: Write boot status failed!\r\n"));
			assert(0);
		}
	}
	return retcode;

}


APP_RESULT MQTTFlash_FLReadConfig(ConfigDataBuffer *configBuffer) {
	Storage_Read_T readCredentials =
	{
			.FileName = CONFIG_FILENAME,
			.ReadBuffer = configBuffer->data,
			.BytesToRead = 0UL,
			.ActualBytesRead = 0UL,
			.Offset = 0UL,
	};
	Retcode_T retcode = RETCODE_OK;
	bool status = false;

	/* Validating if WIFI storage medium is available */
	retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
	if ((RETCODE_OK == retcode) && (true == status)) {
		retcode = WifiStorage_GetFileStatus((const uint8_t*) &(CONFIG_FILENAME), &(readCredentials.BytesToRead));
		if (retcode == RETCODE_OK) {
			retcode = Storage_Read(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &readCredentials);
			if (retcode == RETCODE_OK) {
				configBuffer->length =  strlen(configBuffer->data);
				LOG_AT_DEBUG(("MQTTFlash: Read config from flash success: [%lu] \r\n", configBuffer->length));
				return APP_RESULT_OPERATION_OK;
			}
		}

	}
	LOG_AT_ERROR(("MQTTFlash: Read config from flash not successful, maybe config isn't written to flash yet!\r\n"));
	return APP_RESULT_FILE_MISSING;
}

void MQTTFlash_FLWriteConfig(ConfigDataBuffer *configBuffer) {
	Storage_Write_T writeCredentials =
	{
			.FileName = CONFIG_FILENAME,
			.WriteBuffer = configBuffer->data,
			.BytesToWrite = strlen(configBuffer->data) + 1,
			.ActualBytesWritten = 0UL,
			.Offset = 0UL,
	};

	Retcode_T retcode = RETCODE_OK;
	bool status = false;

	/* Validating if WIFI storage medium is available */
	retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
	if ((RETCODE_OK == retcode) && (true == status))
	{
		/* Since WIFI_CONFIG_FILE_NAME is available, copying it to the WIFI file system */
		retcode = Storage_Write(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &writeCredentials);
		if (RETCODE_OK == retcode)
		{
			LOG_AT_TRACE(("MQTTFlash: Write config to flash memory with length: [%lu]\r\n", configBuffer->length));

		} else {
			LOG_AT_ERROR(("MQTTFlash: Write config failed!\r\n"));
			assert(0);
		}
	}

}

void MQTTFlash_FLDeleteConfig(void) {
	MQTTFlash_FLDeleteFile( (const uint8_t*)CONFIG_FILENAME);
	MQTTFlash_FLDeleteFile( (const uint8_t*)REBOOT_FILENAME);
}

APP_RESULT MQTTFlash_SDReadConfig(ConfigDataBuffer *configBuffer) {
	return MQTTFlash_SDRead ((const uint8_t*) CONFIG_FILENAME, configBuffer);
}

static APP_RESULT MQTTFlash_SDRead(const uint8_t *fileName, ConfigDataBuffer *ramBufferRead) {
	FIL fileObject; /* File objects */
	uint16_t fileSize;
	FILINFO fileInfo;
	uint16_t maxBufferSize = sizeof(ramBufferRead->data);

	if (f_stat((const TCHAR*) fileName, &fileInfo) != FR_Z_OK) {
		LOG_AT_ERROR(("MQTTFlash: Can't find file: [%s]!\r\n",fileName ));
		return (APP_RESULT_SDCARD_ERROR);
	}
	fileSize = fileInfo.fsize;

	if (fileSize > maxBufferSize) {
		LOG_AT_ERROR(("MQTTFlash: Read size: %i to big: %hu\r\n", fileSize, maxBufferSize));
		return (APP_RESULT_SDCARD_BUFFER_OVERFLOW_ERROR);
	} else {
		LOG_AT_TRACE(("MQTTFlash: Read file: [%s] with size: %i\r\n", fileName, fileSize));
	}

	/* Open the file for read */
	if (f_open(&fileObject, (const TCHAR*) fileName, FA_READ) != FR_Z_OK) {
		LOG_AT_ERROR(("MQTTFlash: Opening file failed\r\n"));
		return (APP_RESULT_SDCARD_ERROR);
	}

	/* Set the file read pointer to first location */
	if (f_lseek(&fileObject, 0) != FR_Z_OK) {
		/* Error. Cannot set the file pointer */
		LOG_AT_ERROR(("MQTTFlash: Seeking file failed\r\n"));
		return (APP_RESULT_SDCARD_ERROR);
	}

	/*Read some data from file */
	if ((f_read(&fileObject, ramBufferRead->data, fileSize, &(ramBufferRead->length)) == FR_Z_OK) && (fileSize == ramBufferRead->length)) {
		LOG_AT_TRACE(("MQTTFlash: Reading from succeded: %i %lu\r\n",
					fileSize, ramBufferRead->length));
	} else {
		/* Error. Cannot read the file */
		LOG_AT_ERROR(("MQTTFlash: Read failed with different size : %i %lu\r\n",
				fileSize, ramBufferRead->length));
		return (APP_RESULT_SDCARD_ERROR);
	}
	LOG_AT_TRACE(("MQTTFlash: Read content from status file: %s\r\n", ramBufferRead->data));

	/* Close the file */
	if (f_close(&fileObject) != FR_Z_OK) {
		return (APP_RESULT_SDCARD_ERROR);
	}
	return APP_RESULT_OPERATION_OK;
}

static APP_RESULT MQTTFlash_SDWrite(const uint8_t* fileName, uint8_t* stringBuffer, BYTE mode ) {
	FIL fileObject; /* File objects */
	uint8_t ramBufferWrite[SIZE_LARGE_BUF]; /* Temporary buffer for write file */
	uint16_t fileSize;
	UINT bytesWritten;

	/* Initialization of file buffer write */
	fileSize = strlen(stringBuffer);
	LOG_AT_TRACE(("MQTTFlash: Size of buffer: [%i]\r\n", fileSize));
	memcpy(ramBufferWrite, stringBuffer, fileSize);

	/* Open the file to write */
	if (f_open(&fileObject, (const TCHAR*) fileName, mode) != FR_Z_OK) {
		/* Error. Cannot create the file */
		LOG_AT_ERROR(("MQTTFlash: Error creating status"));
		return (APP_RESULT_SDCARD_ERROR);
	}

	/* Set the file write pointer at th eend to append */
	if (f_lseek(&fileObject, f_size(&fileObject)) != FR_Z_OK) {
		/* Error. Cannot set the file write pointer */
		LOG_AT_ERROR(("MQTTFlash: Error accessing file"));
		return (APP_RESULT_SDCARD_ERROR);
	}

	/* Write a buffer to file*/
	if ((f_write(&fileObject, ramBufferWrite, fileSize, &bytesWritten) != FR_Z_OK) || (fileSize != bytesWritten)) {
		/* Error. Cannot write the file */
		LOG_AT_ERROR(("MQTTFlash: Error writing in file"));
		return (APP_RESULT_SDCARD_ERROR);
	}

	/* Close the file */
	if (f_close(&fileObject) != FR_Z_OK) {
		/* Error. Cannot close the file */
		LOG_AT_ERROR(("MQTTFlash: Error closing file"));
		return (APP_RESULT_SDCARD_ERROR);
	}

	return APP_RESULT_OPERATION_OK;
}

static void MQTTFlash_FLDeleteFile (const uint8_t* fileName) {
	Retcode_T retcode = RETCODE_OK;
	bool status = false;
	uint32_t bytesToRead = 0;
	/* Validating if WIFI storage medium is available */
	retcode = Storage_IsAvailable(STORAGE_MEDIUM_WIFI_FILE_SYSTEM, &status);
	if ((RETCODE_OK == retcode) && (true == status))
	{
		retcode = WifiStorage_GetFileStatus(fileName, &bytesToRead);
		if (retcode == RETCODE_OK) {
			LOG_AT_TRACE(("MQTTFlash: File  [%s] exists, length: [%lu]\r\n", fileName, bytesToRead ));
			int32_t fileHandle = INT32_C(-1);
			retcode = WifiStorage_FileDelete((const uint8_t *) fileName, &fileHandle);
			if (RETCODE_OK == retcode)
			{
				LOG_AT_DEBUG(("MQTTFlash: Deleted file successful!\r\n"));

			} else {
				LOG_AT_ERROR(("MQTTFlash: Deleted file failed:[%lu] \r\n", Retcode_GetCode(retcode)));
				//assert(0);
			}
		} else {
			LOG_AT_ERROR(("MQTTFlash: Something is wrong with: [%s], length: [%lu], error_code [%lu]\r\n", fileName, bytesToRead,  Retcode_GetCode(retcode)) );
		}
	}

}
