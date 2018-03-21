# ArduinoHost

Enables a BBC Micro to view an Arduino as a filing system host over a serial connection. 

Arduino connects to wifi, enabling the BBC Micro to mount a .ssd DFS disc image over the Internet.


## Set up

Assumes a BBC Micro with UPURSFS ROM installed, connected to an Arduino via the BBC User Port as specified by UPURS. 

### Hardware
Currently specific to MKR1000 / SAMD21 based Arduino boards. This hardware requires all signals to be level shifted from 5v to 3.3v to the Arduino. On the BBC side, UPURS assumes RX and TX levels are inverted. In this set-up we are doing this in external hardware (with a SN74ACT14N) so the Arduino is presented with standard 115200 baud serial.

### Wifi
The Arduino connects to your wifi AP and searches disc images on bbcmicro.co.uk, allowing to you load files (i.e. sections of a given .ssd hosted on the website) to the beeb. You will need to define the SSID and password in a secrets.h file, and either edit the IP address, gateway and DNS defined in the code or remove them to enable DHCP client.


![Screenshot](https://github.com/8bitkick/ArduinoHost/blob/master/screenshot2.jpg?raw=true)

## Usage

1) Connect to WiFi

`*WIFI or <ctrl-break>` 

2) Search bbcmicro.co.uk (max 10 results returned at the moment)

`*SEARCH ARCADI`

3) Select a disc image number to mount from the search results.

`*MOUNT 3`

4) Show remote .ssd contents

`*.`

5) Run game!

`CHAIN"ARCADIA"`

## Save

*SAVE currently sends BBC memory region to the Arduino serial montior.

When in MODE 7 typing *SAVE A 7c00 8000 will send BBC screen to the Arduino console


## To do

This is just a proof of concept. Browsing the site works to an extent, some games load, others don't.

Lots of tidy-up, debugging and optimization still required. And many TubeHost functions still need to be implemented...






