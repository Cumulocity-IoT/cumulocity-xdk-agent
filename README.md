# cumulocity_xdk_agent

Device agent for Bosch XDK to connect to Cumulocity (C8Y Agent for XDK).
In order to connect the XDK to Cumuloxity a tenant is required: https://cumulocity.com/try-for-free/

## Features

The C8Y Agent for XDK sends following sensor measurements to C8Y :

- ACCELENABLED=<TRUE TO SEND,FALSE OTHERWISE>
- GYROENABLED=<TRUE TO SEND,FALSE OTHERWISE>
- MAGENABLED=<TRUE TO SEND,FALSE OTHERWISE>
- ENVENABLED=<TRUE TO SEND,FALSE OTHERWISE>
- LIGHTENABLED=<TRUE TO SEND,FALSE OTHERWISE>

If a measurement is sent can be configured in config.txt. In addition the rate  (every XX ms) at which measurementes are sent can be configured.

The XDK can receive operations and messages initiated in your C8Y tenant. So you can :
- change streaming rate: 
-- send shell command from C8Y (publish any 1000 ms): "speed 1000" 
- toggle yellow light upon receiving any message:
-- for this define device dashboard and use "message" widget
- restart XDK from C8Y:
-- select XDk device in C8Y cockpit and execute "Restart device" from dropdown-menue "More"
- toggle yellow light:
-- send shell command: "toggle"

The XDK agents supports registration as described: https://cumulocity.com/guides/rest/device-integration/

The buttons have following on the XDK have the following functions:

 	* Button one dot: stop/start sending measurements to Cumulocity
	* Button two dots: reset boot status stored on device flash to "NOT_IN_BOOT_PROCESS"
		
## Overview

1. Prepare SD card
2. Register XDK in Cumulocity & Upload SMART Rest Template
3. Install XDK Workbench: https://xdk.bosch-connectivity.com/de/software-downloads
4. Prepare project 
5. Flash your C8Y Device Agent on your XDK		
		
## 1. Prepare SD card
		
1. Format SD in FAT format
2. Adapt settings in config.txt and copy to SD card. A template for config.txt exists in the project "sag-d-iot.xdk_v4_mqtt_serval/resources"

## 2. Register XDK in Cumulocity & Upload SMART Rest Template

1. Before starting the XDK a C8Y device registration for the XDK has to be created in your C8Y tenant. Pls. see https://www.cumulocity.com/guides/users-guide/device-management.
2. The agent uses the MAC of the WLAN chip. Pls. check on sticker on the bottom side of your XDK under "WLAN: 7C_7C_7C_7C_7C_7C" , e.g. XDK_7C_7C_7C_7C_7C_7C
NOTE: prepend XDK_ before WLAN MAC adress
3. Upload SMART Rest Template "XDK_Template_Collection.json" from folder resources/XDK_Template_Collection.json to your C8Y tenant. Pls see https://www.cumulocity.com/guides/users-guide/device-management for required steps

## 3. Install XDK Workbench 

Install  XDK Workbench: https://xdk.bosch-connectivity.com/software-downloads
> NOTE: Installationpath must not not contains blanks 
> NOTE: The current version of the Workbench 3.6.0 hat eine Buffer Beschränkung. Diese muss wie folgt erhöht werden:

In order to avoid a buffer overflow, as seen in the following error message:

INFO | XDK DEVICE 1: MQTT_ConnectToBroker_Z: connecting secure
INFO | XDK DEVICE 1:         11 [SSL:1] Sec_receiveCB: HORRIBLE Buffer full state=1 (0x200023fc)

Increase MBEDTLS_SSL_MAX_CONTENT_LEN macro value from 4850 to 5950 in Common/config/MbedTLS/ MbedtlsConfigTLS.h in line 2923.
The macro MBEDTLS_SSL_MAX_CONTENT_LEN determines the size of both the incoming and outgoing TLS I/O buffer used by MbedTLS library.

## 4. Prepare project 

1. Clone git repository
```
git clone https://github.com/SoftwareAG/cumulocity-xdk-agent.git
```
2. In XDK Workbench import project into workspace
3. In XDK Workbench: project clean and project build
4. Connect XDK over USB cable

## 5. Flash your C8Y Device Agent to your XDK

NOTE: connect your XDK using USB cable to get debug messages.
 
1. Flash project to XDK using the XDK Workbench 3.6.0
2. After starting the XDK the agent runs in "Registration Mode" and waits until the registration is accepted in your cumulocity tenant. So you have to accept the registration in your C8Y tenant. See as well step 2.1.
3. After accepting the registration in C8Y the XDK agent receives device credentials and stores these on the SD card.
4. The XDK restarts and runs now in "Operation Mode". After this you should see measurements in C8Y.

## Procedure when re-registering device in Cumulocity tenant

1. Delete entries MQTTUSER und MQTTPASSWORD löschen from the file "config.txt"
2. Delete XDK from Cumulocity Tenant
3. Restart XDK and register XDKs again

## Troubleshooting

When the following error occusrs after building and flashing the project please clean,rebuild and flash project again:

```
 INFO | Transmission successfully completed!
 INFO | Booting application...
 INFO | XDK DEVICE 1:  Performing application CRC validation (this will take a couple of seconds)
 INFO | XDK DEVICE 1: Application Firmware Corrupted
 INFO | XDK DEVICE 1:  Invalid application
```

## Sample dashboard

A sample dashboard can be build using the resources/Container_V01.svg. Please see: https://www.cumulocity.com/guides/users-guide/optional-services/ for a documentation on how to use the svg in a SCADA widget.

![Sample Dashboard](https://github.com/SoftwareAG/cumulocity-xdk-agent/blob/master/resources/Container_V01.jpg)
