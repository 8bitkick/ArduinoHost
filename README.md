# ArduinoHost

Enables a BBC Micro to view an Arduino as a filing system host over a serial connection. 

## Set up

Assumes a BBC Micro with UPURSFS ROM installed, connected to an Arduino via the BBC User Port. 

Currently specific to MKR1000 / SAMD21 based boards. This hardware requires all signals to be level shifted from 5v to 3.3v to the Arduino. On the BBC side, UPURS assumes RX and TX levels are inverted. In this set-up we are doing this in external hardware (with a SN74ACT14N) so the Arduino is presented with standard 115200 baud serial.


## Current status - basic test functionality

* BBC successfully boots into HostFS

* When in `MODE 0`, typing `*LOAD OWL 3000` loads a picture on the BBC from the Arduino

* \*SAVE sends BBC memory region to the Arduino serial montior. 
  * When in `MODE 7` typing `*SAVE A 7c00 8000` will send BBC screen to the Arduino console

* With wifi enabled, the `*WIFI` command returns status


## Issues

There are some possible sync issues with serial communication over User Port serial. Error checking is not performed over the comms, and any lost or corrupted command bytes to the BBC could cause it to hang. Some more rigourous testing of this needs implementing.
