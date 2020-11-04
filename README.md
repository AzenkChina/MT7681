# MT7681

	This module act as a tcp server, it listens on three tcp ports: 4059, 4060 and 9998.
	Port 4059 will passthrough all data between UART and tcp connect in DATA mode, and useless in CMD mode.
	Port 4060 will send out the status(messages like S0E0R0 or S1E0R1) of the set pin(gpio1), event pin(gpio0) & reset pin(gpio3), also, it can control(control command is STA0 & STA1) the sta pin(gpio2).
	Port 9998 is to send command when the module is in DATA mode.

	Some useful AT commands:

	AT#Default								--------make all parameter to default
	AT#Reboot								--------reboot the module
	AT#AP									--------switch to AP mode
	AT#STA									--------switch to STA mode
	AT#CMD									--------switch to CMD mode
	AT#DATA									--------switch to DATA mode
	AT#Uart -b9600 -w8 -p0 -s1						--------config the UART

	AT#SoftAPConf -d0							--------clean current AP cfg on the flash
	AT#SoftAPConf -sMT7681New1231 -a7 -p87654321 -c6 [-b112233445566]	--------config the AP [s=SSID a=auth p=password c=channel b=BSSID]
	AT#SoftAPConf -m0							--------store current AP cfg to flash
	-a0=OPEN -a4=WPAPSK -a7=WPA2PSK -a9=WPA/WPA2PSK

	AT#StaConf -d0								--------clean current STA cfg on the flash
	AT#StaConf -sMT7681New1231 -a7 -p87654321 [-b112233445566]		--------config the STA [s=SSID a=auth p=password b=BSSID]
	AT#StaConf -m0								--------store current STA cfg to flash
	-a0=OPEN -a4=WPAPSK -a7=WPA2PSK -a9=WPA/WPA2PSK
