# ArduinoHost

Enables a BBC Micro to view an Arduino as a filing system host over a serial connection. 

## Set up

Assumes a BBC Micro with UPURSFS ROM installed, connected to an Arduino running this firmware. 

Currently specific to MKR1000 / SAMD21 based boards. This hardware requires all signals to be level shifted to 3.3v to the Arduino, and assumes RX and TX are inverted in external hardware (e.g. using an SN74ACT14N).


## Currently some (very) basic capabilities

* BBC successfully boots into HostFS

* When in `MODE 0`, typing `*LOAD OWL 3000` loads a picture on the BBC from the Arduino

* \*SAVE sends BBC memory region to the Arduino serial montior. 
  * When in `MODE 7` typing `*SAVE A 7c00 8000` will send BBC screen to the Arduino console

* With wifi enabled, the `*WIFI` command returns status

