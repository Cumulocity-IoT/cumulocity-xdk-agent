/******************************************************************************
 **	COPYRIGHT (c) 2019		Software AG
 **
 **	The use of this software is subject to the XDK SDK EULA
 **
 **
 *******************************************************************************
 **
 **	OBJECT NAME:	MQTTStorage.c
 **
 **	DESCRIPTION:	Source Code for the Cumulocity MQTT Client for the Bosch XDK
 **
 **	AUTHOR(S):		Christof Strack, Software AG
 **
 **
 *******************************************************************************/

#include "AppController.h"
#include "MQTTStorage.h"
#include "MQTTCfgParser.h"
#include "XDK_Storage.h"
#include "WifiStorage.h"

#include "BCDS_SDCard_Driver.h"
#include <stdio.h>
#include <ff.h>

/* global variables ********************************************************* */

/* local module global variable declarations */
static void MQTTFlash_FLDeleteFile (const uint8_t *fileName);
static Retcode_T MQTTFlash_SDWrite(const uint8_t *fileName, uint8_t *stringBuffer, BYTE mode);
static Retcode_T MQTTFlash_SDRead(const uint8_t *fileName, ConfigDataBuffer *ramBufferRead);

Retcode_T MQTTFlash_Init(void) {
	/* read boot status */
	uint8_t bootstatus[SIZE_XSMALL_BUF] = { 0 };
	if (MQTTFlash_FLReadBootStatus(bootstatus) != RETCODE_OK) {
		//probably the first time this XDK is used with C8Y, so we need to initialize the reboot.txt file
		MQTTFlash_FLWriteBootStatus((uint8_t*) NO_BOOT_PENDING);
		if (MQTTFlash_FLReadBootStatus(bootstatus) != RETCODE_OK)
		{
			LOG_AT_ERROR(("MQTTStorage: Could not initialize boot status!\r\n"));
			return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_UNINITIALIZED);
		}

	}
	LOG_AT_DEBUG(("MQTTStorage: Reading boot status: [%s]\r\n", bootstatus));

	return RETCODE_OK;

}

void MQTTFlash_SDAppendCredentials(char* stringBuffer) {
	LOG_AT_DEBUG(("MQTTStorage: Received credentials:%s\r\n", stringBuffer));
	if (SDCARD_INSERTED == SDCardDriver_GetDetectStatus()) {
		LOG_AT_DEBUG(("MQTTStorage: SD card is inserted in XDK\r\n"));
		/* SDC Disk FAT file system Write/Read functionality */
		if (MQTTFlash_SDWrite((const uint8_t*) CONFIG_FILENAME, stringBuffer, FA_OPEN_ALWAYS | FA_WRITE) == RETCODE_OK) {
			LOG_AT_TRACE ( ("MQTTStorage: Write using FAT file system success \r\n") );
		} else {
			LOG_AT_ERROR( ("MQTTStorage: Write using FAT file system failed\r\n") );
			assert(0);
		}
	} else {
		LOG_AT_ERROR( ("MQTTStorage: SD card is not inserted in XDK\r\n") );
		assert(0);
	}
}

Retcode_T MQTTFlash_FLReadBootStatus(uint8_t* bootstatus) {
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
				LOG_AT_TRACE(("MQTTStorage: Read using FAT file system success: [%s] \r\n", readCredentials.ReadBuffer));
				return RETCODE_OK;
			}
		}

	}
	LOG_AT_ERROR(("MQTTStorage: Read using flash file system failed!\r\n"));
	Retcode_RaiseError(retcode);
	return RETCODE(RETCODE_SEVERITY_ERROR, FR_NO_FILE);
}

Retcode_T MQTTFlash_FLWriteBootStatus(uint8_t* bootstatus) {

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
			LOG_AT_DEBUG(("MQTTStorage: Init boot status: [%s]\r\n", bootstatus));

		} else {
			LOG_AT_ERROR(("MQTTStorage: Write boot status failed!\r\n"));
			assert(0);
		}
	}
	return retcode;

}


Retcode_T MQTTFlash_FLReadConfig(ConfigDataBuffer *configBuffer) {
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
				LOG_AT_DEBUG(("MQTTStorage: Read config from flash: [%lu] \r\n", configBuffer->length));
				return RETCODE_OK;
			}
		}

	}
	LOG_AT_ERROR(("MQTTStorage: Read config from flash not successful, maybe config isn't written to flash yet!\r\n"));
	return RETCODE(RETCODE_SEVERITY_ERROR, FR_NO_FILE);
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
			LOG_AT_TRACE(("MQTTStorage: Write config to flash memory with length: [%lu]\r\n", configBuffer->length));

		} else {
			LOG_AT_ERROR(("MQTTStorage: Write config failed!\r\n"));
			assert(0);
		}
	}

}

void MQTTFlash_FLDeleteConfig(void) {
	MQTTFlash_FLDeleteFile( (const uint8_t*)CONFIG_FILENAME);
	MQTTFlash_FLDeleteFile( (const uint8_t*)REBOOT_FILENAME);
}

Retcode_T MQTTFlash_SDReadConfig(ConfigDataBuffer *configBuffer) {
	return MQTTFlash_SDRead ((const uint8_t*) CONFIG_FILENAME, configBuffer);
}

static Retcode_T MQTTFlash_SDRead(const uint8_t *fileName, ConfigDataBuffer *ramBufferRead) {
	FIL fileObject; /* File objects */
	uint16_t fileSize;
	FILINFO fileInfo;
	uint16_t maxBufferSize = sizeof(ramBufferRead->data);

	if (f_stat((const TCHAR*) fileName, &fileInfo) != FR_Z_OK) {
		LOG_AT_ERROR(("MQTTStorage: Can't find file: [%s]!\r\n",fileName ));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}
	fileSize = fileInfo.fsize;

	if (fileSize > maxBufferSize) {
		LOG_AT_ERROR(("MQTTStorage: Read size: %i to big: %hu\r\n", fileSize, maxBufferSize));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
	} else {
		LOG_AT_TRACE(("MQTTStorage: Read file: [%s] with size: %i\r\n", fileName, fileSize));
	}

	/* Open the file for read */
	if (f_open(&fileObject, (const TCHAR*) fileName, FA_READ) != FR_Z_OK) {
		LOG_AT_ERROR(("MQTTStorage: Opening file failed\r\n"));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}

	/* Set the file read pointer to first location */
	if (f_lseek(&fileObject, 0) != FR_Z_OK) {
		/* Error. Cannot set the file pointer */
		LOG_AT_ERROR(("MQTTStorage: Seeking file failed\r\n"));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}

	/*Read some data from file */
	if ((f_read(&fileObject, ramBufferRead->data, fileSize, &(ramBufferRead->length)) == FR_Z_OK) && (fileSize == ramBufferRead->length)) {
		LOG_AT_TRACE(("MQTTStorage: Reading from succeded: %i %lu\r\n",
					fileSize, ramBufferRead->length));
	} else {
		/* Error. Cannot read the file */
		LOG_AT_ERROR(("MQTTStorage: Read failed with different size : %i %lu\r\n",
				fileSize, ramBufferRead->length));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}
	LOG_AT_TRACE(("MQTTStorage: Read content from status file: %s\r\n", ramBufferRead->data));

	/* Close the file */
	if (f_close(&fileObject) != FR_Z_OK) {
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}
	return RETCODE_OK;
}

static Retcode_T MQTTFlash_SDWrite(const uint8_t* fileName, uint8_t* stringBuffer, BYTE mode ) {
	FIL fileObject; /* File objects */
	uint8_t ramBufferWrite[SIZE_LARGE_BUF]; /* Temporary buffer for write file */
	uint16_t fileSize;
	UINT bytesWritten;

	/* Initialization of file buffer write */
	fileSize = strlen(stringBuffer);
	LOG_AT_TRACE(("MQTTStorage: Size of buffer: [%i]\r\n", fileSize));
	memcpy(ramBufferWrite, stringBuffer, fileSize);

	/* Open the file to write */
	if (f_open(&fileObject, (const TCHAR*) fileName, mode) != FR_Z_OK) {
		/* Error. Cannot create the file */
		LOG_AT_ERROR(("MQTTStorage: Error creating status"));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}

	/* Set the file write pointer at th eend to append */
	if (f_lseek(&fileObject, f_size(&fileObject)) != FR_Z_OK) {
		/* Error. Cannot set the file write pointer */
		LOG_AT_ERROR(("MQTTStorage: Error accessing file"));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}

	/* Write a buffer to file*/
	if ((f_write(&fileObject, ramBufferWrite, fileSize, &bytesWritten) != FR_Z_OK) || (fileSize != bytesWritten)) {
		/* Error. Cannot write the file */
		LOG_AT_ERROR(("MQTTStorage: Error writing in file"));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}

	/* Close the file */
	if (f_close(&fileObject) != FR_Z_OK) {
		/* Error. Cannot close the file */
		LOG_AT_ERROR(("MQTTStorage: Error closing file"));
		return RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_FAILURE);
	}

	return RETCODE_OK;
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
			LOG_AT_TRACE(("MQTTStorage: File  [%s] exists, length: [%lu]\r\n", fileName, bytesToRead ));
			int32_t fileHandle = INT32_C(-1);
			retcode = WifiStorage_FileDelete((const uint8_t *) fileName, &fileHandle);
			if (RETCODE_OK == retcode)
			{
				LOG_AT_DEBUG(("MQTTStorage: Deleted file successful!\r\n"));

			} else {
				LOG_AT_ERROR(("MQTTStorage: Deleted file failed:[%lu] \r\n", Retcode_GetCode(retcode)));
				//assert(0);
			}
		} else {
			LOG_AT_ERROR(("MQTTStorage: Something is wrong with: [%s], length: [%lu], error_code [%lu]\r\n", fileName, bytesToRead,  Retcode_GetCode(retcode)) );
		}
	}

}
