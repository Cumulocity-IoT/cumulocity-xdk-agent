# Cumulocity agent for Bosch XDK rapid prototype device

> **IMPORTANT NOTE: This release has changes that break funcionality in previous releases. You have to delete your device from Cumulocity, change your configuration in `config.txt` and register it again.** 

> **CHANGES**  
  
> **1. The format of the configuration in `config.txt` changed. From now on all parameters don't have `ENABELED` in their name, e.g. `ACCELENABELED` becomes `ACCEL`**  
> **2. The format of the device Id changed from `XDK_7C_7C_7C_7C_7C_7C` to `7C7C7C7C7C7C`. Where `7C7C7C7C7C7C` should be replaced by your MAC WLAN printed on the bottom sticker of your XDK**  
> **Background: Since the memory of the XDK is limited changes were introduced to reduce the length of keys.**
> **3. This version will only work with Workbench version 3.6.1. This version contains an improved error handling in case the MQTT connection is broken

This project is a device agent to connect the [Bosch XDK](https://www.bosch-connectivity.com/de/produkte/cross-domain/cross-domain-developement-kit) to Cumulocity (C8Y Agent for XDK). The XDK is a quick and professional prototyping platform for prototyping IoT use cases.

For this demo a Cumulocity tenant and an XDK device is required. For a free trial tenant you can register [here](https://www.softwareag.cloud/site/product/cumulocity-iot.html#/).

When the XDK is registered in a Cumulocity tenant the environmental sensor readings measured by the XDK are sent to the Cumulocity IoT cloud. Potential use cases are:
- Control & monitor heating -> temperature sensor, acoustic sensor
- Control & monitor lighting in building -> digital light sensor
- Control & monitor machine -> acceleration sensor

# Content
1. [Overview](#overview-of-features-XDK-device-agent)
2. [Register XDK in Cumulocity](#register-xdk-in-c8y-tenant)
3. [Operate XDK](#operate-xdk)  
3.1 [Execute operations on device](#execute-operations-on-device)  
3.2 [View events sent from device](#view-events-sent-from-device)  
3.3 [Detailed configuration](#detailed-configuration)  
3.4 [Buttons](#buttons)  
3.5 [Status indicated by LEDs](#status-indicated-by-leds)  
3.6 [Define Root Certificate for TLS](#define-root-certificate-for-tls)  
4. [Troubleshooting](#troubleshooting)
5. [Sample dashboards](#sample-dashboards)


## Overview of features XDK device agent

The device agent allows to send measurements from the XDK to your Cumulocity tenant. These measurements can be visualised in dashboards.
In the downstream direction  operation commands can be sent to the XDK using the device managment app in Cumulocity: change configuration of sensors, stop/start publishing measurements and restarting the device.  
To get an idea of the currently active configuration the device agent sends its current configuration to the tenant. So in the devicemanagment app you can view always the currently active configuration.

But before running the XDK in OPERATION mode you have to register the device in your Cumulocity tenant. Initially the XDK is in REGISTRATION mode. The registration is achieved automatically through the bootstrap meachnism. This is described in further detail [here](https://cumulocity.com/guides/rest/device-integration/).  

After successful registration - device credentials are received by the XDK - the XDK restarts automatically.
Commands can be send from the Cumulocity App Devicemanagement to change the sensor speed, toogle an LED or switch on/off sensors, see documentation https://www.cumulocity.com/guides/users-guide/device-management/#shell .  

[back to content](#content)

## Register XDK in C8Y tenant

1. Prepare SD card
2. Register XDK in Cumulocity & Upload SMART Rest Template
3. Install XDK Workbench: https://xdk.bosch-connectivity.com/de/software-downloads
4. Prepare project 
5. Flash your C8Y Device Agent on your XDK
6. Procedure when re-registering device in Cumulocity tenant
		
### 1. Prepare SD card
> NOTE: Make sure your SD card is smaller than 32GB, otherwise it can't be formatted in the FAT filesystem format		
1. Format SD in FAT format
2. Adapt settings in `config.txt` and copy to SD card. A template for config.txt exists in the project "cumulocity-xdk-agent/resources". Ideally you only need to change `WIFISSID` and `WIFIPASSWORD`.

### 2. Register XDK in Cumulocity & upload SMART Rest Template

1. Before starting the XDK a C8Y device registration for the XDK has to be created in your C8Y tenant, please see [here](https://www.cumulocity.com/guides/users-guide/device-management for a detailed description. For this resgitration you need the external device ID. This is decribed in the next step.
2. The agent uses the MAC of the WLAN chip as an external device ID. You have to check the sticker on the bottom side of your XDK under `WLAN: 7C_7C_7C_7C_7C_7C` , e.g. `7C7C7C7C7C7C`
> NOTE: remove the `_` form the WLAN MAC adress
3. Upload SMART Rest Template "XDK_Template_Collection.json" from folder resources/XDK_Template_Collection.json to your C8Y tenant. Pls see https://www.cumulocity.com/guides/users-guide/device-management for required steps

### 3. Install XDK Workbench 

Install the XDK Workbench: https://xdk.bosch-connectivity.com/software-downloads
> NOTE: The installation path must not contains blanks. 

> NOTE: The current version of the Workbench 3.6.0 defines a buffer size that is not sufficient for the certificate being used for Cumulocity. Therefore the buffer has to be increased.

In order to avoid a buffer overflow, as seen in the following error message:

```
INFO | XDK DEVICE 1: MQTT_ConnectToBroker_Z: connecting secure
INFO | XDK DEVICE 1:         11 [SSL:1] Sec_receiveCB: HORRIBLE Buffer full state=1 (0x200023fc)
```

Increase MBEDTLS_SSL_MAX_CONTENT_LEN macro value from 4850 to 5950 in Common/config/MbedTLS/ MbedtlsConfigTLS.h in line 2921.
The macro MBEDTLS_SSL_MAX_CONTENT_LEN determines the size of both the incoming and outgoing TLS I/O buffer used by MbedTLS library.


### 4. Prepare project 

1. Clone git repository
```
git clone https://github.com/SoftwareAG/cumulocity-xdk-agent.git
```
2. In XDK Workbench import project into your workspace
3. In XDK Workbench: project clean and project build
4. Connect XDK over USB cable

### 5. Flash your C8Y Device Agent to your XDK

NOTE: connect your XDK using USB cable to get debug messages.
 
1. Flash project to XDK using the XDK Workbench 3.6.1
2. After starting the XDK the agent runs in "Registration Mode" and waits until the registration is accepted in your cumulocity tenant. So you have to accept the registration in your C8Y tenant. See as well step 2.1.
3. After accepting the registration in C8Y the XDK agent receives device credentials and stores these on the SD card.
4. The XDK restarts and runs now in "Operation Mode". After this you should see measurements in C8Y.

### 6. Procedure when re-registering device in Cumulocity tenant

1. Delete entries MQTTUSER und MQTTPASSWORD from the file `config.txt` stored on the SD card
2. Delete XDK from Cumulocity Tenant. Navigate to the device in the Cockpit and delete device
3. Restart XDK and keep the button with 2 dots pressed for deleting the configuration on the WIFI flash, see as well handing of [buttons](#buttons).
4. Register XDK again as before.

[back to content](#content)

## Operate XDK
This section contains all information that is relevant once the XDK is registered in Cumulocity

### Execute operations on device
The XDK can receive operations and messages initiated in your C8Y tenant. Operations to the XDK can either be issued by using:
1. shell: see documentation https://www.cumulocity.com/guides/users-guide/device-management/#shell.
2. message widget: see https://www.cumulocity.com/guides/users-guide/cockpit/#widgets.
3. drop-down menue in app Devicemanagment: from the Cumulocity app `cockpit` and execute "Restart device" from dropdown-menue "More"

The following commands are supported:
* change streaming rate at which XDK publishes measurements, issued by shell (option 1.): 
	* `speed 1000`: to publish measurements every 1000 m, changing the speed is written to the config file on the WIFI chip
* toggle yellow light on/off, initiated by command from shell (option 1.):
	* `toggle`
* enable/disable sensor, initiated by command from shell (option 1.):
	* `sensor NOISE TRUE` or `sensor NOISE TRUE`: to enable/disable the noise sensor. To tkae effect an restart is required. Enabling/disabling the sensor is written to the config file on the WIFI chip
* restart XDK from C8Y, issued by option in drop-down menue (option 3.):
	* select XD: device in C8Y cockpit and execute "Restart device" from dropdown-menue "More"	
* toggle yellow light on/off, initiated from message (any text) widget (option 2.):
	* add "Message Widget" to dashboard, use your connected XDK device as destination and send any text to this XDK  
* stop/start publishing measurements:
	* `stop`
	* `start`

### View events sent from device
You can view the last events transmitted form the XDK by accessing the app `Device management` and follow: Device Management>Devices>All Devices. Then choose your XDK and select the `Events` template  
You can see events like::
1. XDK started!
2. Publish stopped!
3. Publish stated!
	
### Buttons
The buttons have following on the XDK have the following functions:

* Button one dot: stop/start sending measurements to Cumulocity
* Button two dots: 
	* if pressed for longer than 3 seconds resets boot status stored on device flash to "NOT_IN_BOOT_PROCESS"
	* if pressed shortly prints the configuration currently stored in the `config.txt` file on the filesystem of the WIFI chip
	* if pressed during the startup process of the XDK the configuration stored on the WIFI chip is deleted

### Detailed configuration

The C8Y Agent for XDK sends the following sensor measurements to C8Y:
* `ACCEL=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true` # unit g
* `GYRO=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true`
* `MAG=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true`
* `ENV=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true` # unit temp C, pressure hPa
* `LIGHT=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true` # unit Lux
* `NOISE=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value false` #

with the defined streamrate:
* `STREAMRATE=<SPEED TO SEND MEASUREMENTS TO C8Y IN MILLISECONDS> | default-value 5000`

Measurements types can be switched on/off in `config.txt` by setting the value to `TRUE`, `FALSE`. 
> NOTE: Make sure you use Unix line endings instead of Windows line endings. Otherwise the config file cannot be parsed correctly.  
> NOTE: Don't use blanks anywhere in the file. After the last config line a newline is required.  

The configuration for the XDK uses two sources:
* file `config.txt` on the SDCard
* file `config.txt`on the filesystem of the WIFI chip  

When registering the XDK a config on an SD card has to be inserted in the XDK. Upon sucessful registration, i.e. device receives credentials from Cumulocity, the config value including the`MQTTUSER`, `MQTTPASSWORD` are written to the config file on WIFI. From then on the XDK can operate without an SDCard.  
Nevertheless in certain situations it is helpful to only change the WIFI settings and keep all the other settings. Then one can set values for `WIFISSID`,`WIFIPASSWORD` in the `config.txt` on SD and thus overwrite settings stored on the file system of the WIFI chip.
The values defined in the config on the SDCard always take precedence.  

> NOTE: When setting up a hot spot for the WIFI connection, mak esure you use a network band of 2.4GHz, the XDK only supports this band.

In addition to the above listed measurements the battery staus is send ervery minute.

### Status indicated by LEDs
| Red           | Orange   | Yellow   |Mode                      | Status                              | Possible Cause                   |
| :------: | :------: | :------: |------------------------- |------------------------------------ |--------------------------------- |
| Blinking      | Off      | Off      | Operation & Registration | Starting                            |                                  |
| On            | Off      | Off      | Operation & Registration | Error                               | wrong config, no Wifi access, SNTP server not reachable |
| Blinking once | Any      | Any      | Operation                | Received command                    |                                  |
| Off           | Blinking | Any      | Operation                | Running - Publishing                |                                  |
| Off           | On	   | Any      | Operation                | Running - Publishing stopped        |                                  |
| Blinking      | Blinking | Any      | Operation                | Restarting                          |                                  |   
| Off           | Off      | Blinking | Registration             | Running - Waiting for credentials   |                                  |
| Off           | Off      | On       | Registration             | Running - Registration successful   |                                  |

[back to content](#content)

## Define Root Certificate for TLS

For TLS the root certificate of the CA has to be flashed to the XDK.  
This certificate in included in the header file source\ServerCA.h in PEM format. The currently included certificate from "Go Daddy Class 2 Certification Authority" is used for tenants *.cumulcity.com.  
If your CA is different this needs to be changed.  

[back to content](#content)

## Troubleshooting

### Maximal allowed size of flashed binary is exceeded

When you use a XDK with bootloader version 1.1.0 the maximal allowed size of the flashed binary  `cumulocity-xdk-agent.bin` can't exceed 600MB. In this case either:

* flash a new bootloader version 1.2.0 or
* disable the use of the `SENSOR_TOOLBOX` library in  `AppController.h`. This will reduce the size below 600MB
```
#define ENABLE_SENSOR_TOOLBOX				0
```


### When the following error occusrs after building and flashing the project please clean, rebuild and flash project again:

```
 INFO | Transmission successfully completed!
 INFO | Booting application...
 INFO | XDK DEVICE 1:  Performing application CRC validation (this will take a couple of seconds)
 INFO | XDK DEVICE 1: Application Firmware Corrupted
 INFO | XDK DEVICE 1:  Invalid application
```

### The following error indicates, that the workspace path contains spaces. PLEASE remove spaces from the workspace:

```
mingw32-make -j 2 -j8 clean 
C:\XDK-Workbench\XDK\make\mingw32-make.exe -C C:\XDK-Workbench\XDK\SDK/xdk110/Common -f application.mk clean
new_bootloader
mingw32-make[1]: Entering directory 'C:/XDK-Workbench/XDK/SDK/xdk110/Common'
application.mk:368: *** mixed implicit and normal rules. Stop.
mingw32-make[1]: Leaving directory 'C:/XDK-Workbench/XDK/SDK/xdk110/Common'
mingw32-make: *** [clean] Error 2
Makefile:53: recipe for target 'clean' failed
```

### When the following error occurs, please right-click project cumulocity-xdk-projectConfigure -> Add  XDK nature

```
Connection to port 'COM9' established
 INFO | Flashing file 'C:/Users/XXX/cumulocity/cumulocity-xdk-agent/debug/cumulocity-xdk-agent.bin'...
 INFO | XDK DEVICE 1: Ready
 INFO | XDK DEVICE 1: C
 INFO | XDK DEVICE 1: XMODEM Download Success
 INFO | XDK DEVICE 1: c
 INFO | XDK DEVICE 1:  CRC of application area
 INFO | XDK DEVICE 1:  CRC0000B4E4
 INFO | Application checksum 'b4e4' successfully verified.
 INFO | Transmission successfully completed!
 INFO | Booting application...
 INFO | XDK DEVICE 1: b
 INFO | XDK DEVICE 1:  Invalid application
```
If this doesn't help pls. flash a standard click app to the device, e.g `LedsandButtons`. If this works try to flash the device agent again.

### Log in XDK Workbench shows `----- HEAP ISSUE ----`
When you see an exception as below, then you should increase the heap size in `xdk110/Common/config/AmazonFreeRTOS/FreeRTOS/FreeRTOSConfig.h`.

```
INFO | XDK DEVICE 2: MQTTOperation_Init: Reading boot status: [0]
INFO | XDK DEVICE 2: SntpSentCallback : Success
INFO | XDK DEVICE 2: SntpTimeCallback : received
INFO | XDK DEVICE 2: ----- HEAP ISSUE ----
INFO | XDK DEVICE 2: MQTT_ConnectToBroker_Z: Failed since Connect event was not received 
INFO | XDK DEVICE 2: MQTTOperation: MQTT connection to the broker failed  [0] time, try again ... 
```

Increase heap size in `xdk110/Common/config/AmazonFreeRTOS/FreeRTOS/FreeRTOSConfig.h` by changing the following value:

```
#define configTOTAL_HEAP_SIZE                     (( size_t )(72 * 1024 )) # old value is (( size_t )(70 * 1024 ))
```

### Config file cannot be pared
Please verify if you used Linux line endings `\n` 

[back to content](#content)

## Sample dashboards

### Dashboard with SCADA widget

A sample dashboard can be build using the resources/Container_V01.svg.   
Please see: https://www.cumulocity.com/guides/users-guide/optional-services/ for a documentation on how to use the svg in a SCADA widget.

![Sample Dashboard](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/master/resources/Container_V01.jpg)  

Dashboard from above is build using the following standard Cumulocity widgets:
1. SCADA widget on the basis of SVG Container_V01.svg
2. Rotation widget to show the position of he XDK
3. Alarm widget to show recent alarms. This requires to define a smart rule "Container doors open": https://cumulocity.com/guides/users-guide/cockpit/#smart-rules  
The rule "On measurement explicit threshold create alarm" is using the measurement `c8y_Light` with min 50000 and max 100000
4. Data point widget with data points `c8y_acceleration=>accelerationX`, `c8y_acceleration=>accelerationY` and `c8y_acceleration=>accelerationZ`

### Rotation widget

Using the Cumulocity custom widget published on the github: https://github.com/SoftwareAG/cumulocity-collada-3d-widget you can view the rotation of the XDK
![Rotation Widget](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/feature_orientation/resources/XDK_collada.png)  

After installation of the collada widget you will need to upload the 3D model of the XDk. This is available `resources/XDK.dae`. The following screenshots shows the required configuration:
![Configuration_Rotation Widget](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/feature_orientation/resources/XDK_collada_config.png) 

[back to content](#content)
______________________
These tools are provided as-is and without warranty or support. They do not constitute part of the Software AG product suite. Users are free to use, fork and modify them, subject to the license agreement. While Software AG welcomes contributions, we cannot guarantee to include every contribution in the master project.	
______________________
For more information you can Ask a Question in the [TECHcommunity Forums](http://tech.forums.softwareag.com/techjforum/forums/list.page?product=cumulocity).

You can find additional information in the [Software AG TECHcommunity](http://techcommunity.softwareag.com/home/-/product/name/cumulocity).
_________________
Contact us at [TECHcommunity](mailto:technologycommunity@softwareag.com?subject=Github/SoftwareAG) if you have any questions.
