# Cumulocity agent for Bosch XDK rapid prototype device

This project is a device agent to connect the Bosch XDK https://www.bosch-connectivity.com/de/produkte/cross-domain/cross-domain-developement-kit/ to Cumulocity (C8Y Agent for XDK).

For this demo a Cumulocity tenant: https://cumulocity.com/try-for-free/ and an XDK device is required.  
The XDK is a quick and professional prototyping platform for prototyping IoT use cases.

When the XDK is registered to the Cumulocity tenant environmental sensor readings are sent to the Cumulocity IoT cloud. Potential use cases are:
- Control & monitor heating -> tempmerature sensor, acoustic sensor
- Control & monitor lighting in building -> digital light sensor
- Control & monitor machine -> acceleration sensor


## Overview of features XDK device agent

The agent runs in either of two modes: REGISTRATION or OPERATION mode.
When the device agent starts the first time it is in REGISTRATION mode (this mode is recognized when no `MQTTUSER`, `MQTTPASSWORD` is defined in `config.txt`).

The XDK agents supports device registration as described in the Cumulocity documentation: https://cumulocity.com/guides/rest/device-integration/

After successful registration - vales for `MQTTUSER`, `MQTTPASSWORD` are received and saved in `config.txt` - the XDK restarts automatically.
After restarting the agent uses values for `MQTTUSER`, `MQTTPASSWORD` to connect to Cumulocity and runs in OPERATION mode. In this mode configured sensor measurements are sent to C8Y.

Commands can be send from the Cumulocity App Devicemanagement to change the sensor speed, toogle an LED or switch on/off sensors, see documentation https://www.cumulocity.com/guides/users-guide/device-management/#shell .

### Configuration

The C8Y Agent for XDK sends the following sensor measurements to C8Y:
* `ACCELENABLED=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true` # unit g
* `GYROENABLED=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true`
* `MAGENATBLED=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true`
* `ENVENABLED=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true` # unit temp C, pressure hPa
* `LIGHTENABLED=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value true` # unit Lux
* `NOISEENABLED=<TRUE TO SEND MEASUREMENTS, FALSE OTHERWISE> | default-value false` #

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

### Operations
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
	* `sensor NOISEENABELED TRUE` or `sensor NOISEENABELED TRUE`: to enable/disable the noise sensor. To tkae effect an restart is required. Enabling/disabling the sensor is written to the config file on the WIFI chip
* restart XDK from C8Y, issued by option in drop-down menue (option 3.):
	* select XD: device in C8Y cockpit and execute "Restart device" from dropdown-menue "More"	
* toggle yellow light on/off, initiated from message (any text) widget (option 2.):
	* add "Message Widget" to dashboard, use your connected XDK device as destination and send any text to this XDK  
* stop/start publishing measurements:
	* `stop`
	* `start`

### Events
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

### Status indicated by LEDs
| Red      | Orange   | Yellow   |Mode                      | Status                              | Possible Cause                   |
| :------: | :------: | :------: |------------------------- |------------------------------------ |--------------------------------- |
| Blinking | Off      | Off      | Operation & Registration | Starting                            |                                  |
| On       | Off      | Off      | Operation & Registration | Error                               | wrong config, no Wifi access, SNTP server not reachable |
| Off      | Blinking | Off      | Operation                | Running - Publishing                |                                  |
| Off      | On	      | Off      | Operation                | Running - Publishing stopped        |                                  |
| Blinking | Blinking | Off      | Operation                | Restarting                          |                                  |   
| Off      | Off      | Blinking | Registration             | Running - Waiting for credentials   |                                  |
| Off      | Off      | On       | Registration             | Running - Registration successful   |                                  |

		
## Steps required to register and operate XDK device in C8Y tenant

1. Prepare SD card
2. Register XDK in Cumulocity & Upload SMART Rest Template
3. Install XDK Workbench: https://xdk.bosch-connectivity.com/de/software-downloads
4. Prepare project 
5. Flash your C8Y Device Agent on your XDK		
		
## 1. Prepare SD card
> NOTE: Make sure your SD card is smaller 32GB, otherwise it can't be formatted in the FAT filesystem format		
1. Format SD in FAT format
2. Adapt settings in `config.txt` and copy to SD card. A template for config.txt exists in the project "cumulocity-xdk-agent/resources"

## 2. Register XDK in Cumulocity & upload SMART Rest Template

1. Before starting the XDK a C8Y device registration for the XDK has to be created in your C8Y tenant. Pls. see https://www.cumulocity.com/guides/users-guide/device-management.
2. The agent uses the MAC of the WLAN chip. Pls. check on sticker on the bottom side of your XDK under `WLAN: 7C_7C_7C_7C_7C_7C` , e.g. `XDK_7C_7C_7C_7C_7C_7C`
> NOTE: prepend `XDK_` before WLAN MAC adress
3. Upload SMART Rest Template "XDK_Template_Collection.json" from folder resources/XDK_Template_Collection.json to your C8Y tenant. Pls see https://www.cumulocity.com/guides/users-guide/device-management for required steps

## 3. Install XDK Workbench 

Install  XDK Workbench: https://xdk.bosch-connectivity.com/software-downloads
> NOTE: Installation path must not contains blanks 

> NOTE: The current version of the Workbench 3.6.0 defines a buffer size that is not sufficient for the certificate being used for Cumulocity.

In order to avoid a buffer overflow, as seen in the following error message:

```
INFO | XDK DEVICE 1: MQTT_ConnectToBroker_Z: connecting secure
INFO | XDK DEVICE 1:         11 [SSL:1] Sec_receiveCB: HORRIBLE Buffer full state=1 (0x200023fc)
```

Increase MBEDTLS_SSL_MAX_CONTENT_LEN macro value from 4850 to 5950 in Common/config/MbedTLS/ MbedtlsConfigTLS.h in line 2921.
The macro MBEDTLS_SSL_MAX_CONTENT_LEN determines the size of both the incoming and outgoing TLS I/O buffer used by MbedTLS library.

> NOTE: You have to increase the heap size in `xdk110/Common/config/AmazonFreeRTOS/FreeRTOS/FreeRTOSConfig.h`.

In order to avoid a heap issue, as seen in the following error message:

```
INFO | XDK DEVICE 2: MQTTOperation_Init: Reading boot status: [0]
INFO | XDK DEVICE 2: SntpSentCallback : Success
INFO | XDK DEVICE 2: SntpTimeCallback : received
INFO | XDK DEVICE 2: ----- HEAP ISSUE ----
INFO | XDK DEVICE 2: MQTT_ConnectToBroker_Z: Failed since Connect event was not received 
INFO | XDK DEVICE 2: MQTTOperation: MQTT connection to the broker failed  [0] time, try again ... 
```

Increase heap size in `xdk110/Common/config/AmazonFreeRTOS/FreeRTOS/FreeRTOSConfig.h`:

```
#define configTOTAL_HEAP_SIZE                     (( size_t )(72 * 1024 )) # old value is (( size_t )(70 * 1024 ))
```


## 4. Prepare project 

1. Clone git repository
```
git clone https://github.com/SoftwareAG/cumulocity-xdk-agent.git
```
2. In XDK Workbench import project into your workspace
3. In XDK Workbench: project clean and project build
4. Connect XDK over USB cable

## 5. Flash your C8Y Device Agent to your XDK

NOTE: connect your XDK using USB cable to get debug messages.
 
1. Flash project to XDK using the XDK Workbench 3.6.0
2. After starting the XDK the agent runs in "Registration Mode" and waits until the registration is accepted in your cumulocity tenant. So you have to accept the registration in your C8Y tenant. See as well step 2.1.
3. After accepting the registration in C8Y the XDK agent receives device credentials and stores these on the SD card.
4. The XDK restarts and runs now in "Operation Mode". After this you should see measurements in C8Y.

## Procedure when re-registering device in Cumulocity tenant

1. Delete entries MQTTUSER und MQTTPASSWORD from the file `config.txt` stored on the SD card
2. Delete XDK from Cumulocity Tenant. Navigate to the device in the Cockpit and delete device
3. Restart XDK and register XDKs again as before

## Troubleshooting

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

### Config file cannot be pared
Please verify if you used Linux line endings `\n`  

## Sample dashboard

A sample dashboard can be build using the resources/Container_V01.svg.   
Please see: https://www.cumulocity.com/guides/users-guide/optional-services/ for a documentation on how to use the svg in a SCADA widget.

![Sample Dashboard](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/master/resources/Container_V01.jpg)  

Dashboard from above is build using the following standard Cumulocity widgets:
1. SCADA widget on the basis of SVG Container_V01.svg
2. Rotation widget to show the position of he XDK
3. Alarm widget to show recent alarms. This requires to define a smart rule "Container doors open": https://cumulocity.com/guides/users-guide/cockpit/#smart-rules  
The rule "On measurement explicit threshold create alarm" is using the measurement `c8y_Light` with min 50000 and max 100000
4. Data point widget with data points `c8y_acceleration=>accelerationX`, `c8y_acceleration=>accelerationY` and `c8y_acceleration=>accelerationZ`

## TLS Certificate

For TLS the certificate of the CA has to be flashed to the XDK.  
This certificate in included in the header file source\ServerCA.h in PEM format. The currently included certificate from "Go Daddy Class 2 Certification Authority" is used for tenants *.cumulcity.com.  
If your CA is different this needs to be changed.

## Rotation widget

Using the Cumulocity custom widget published on the github: https://github.com/SoftwareAG/cumulocity-collada-3d-widget you can view the rotation of the XDK
![Rotation Widget](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/feature_orientation/resources/XDK_collada.png)  

After installation of the collada widget you will need to upload the 3D model of the XDk. This is available `resources/XDK.dae`. The following screenshots shows the required configuration:
![Configuration_Rotation Widget](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/feature_orientation/resources/XDK_collada_config.png)  

______________________
These tools are provided as-is and without warranty or support. They do not constitute part of the Software AG product suite. Users are free to use, fork and modify them, subject to the license agreement. While Software AG welcomes contributions, we cannot guarantee to include every contribution in the master project.	
______________________
For more information you can Ask a Question in the [TECHcommunity Forums](http://tech.forums.softwareag.com/techjforum/forums/list.page?product=cumulocity).

You can find additional information in the [Software AG TECHcommunity](http://techcommunity.softwareag.com/home/-/product/name/cumulocity).
_________________
Contact us at [TECHcommunity](mailto:technologycommunity@softwareag.com?subject=Github/SoftwareAG) if you have any questions.
