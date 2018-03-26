# ArduinoHost

Enables a BBC Micro to view an Arduino as a file system host over a serial connection. 

Arduino connects to wifi, enabling the BBC Micro to mount a .ssd DFS disc image over the Internet.

## Setup
Connect an Arduino MKR1000, running the sketch in this repository, to a BBC Micro running the UPURSFS client ROM.


### Arduino software
The Arduino connects to your wifi AP and searches disc images on bbcmicro.co.uk, allowing to you load files (i.e. sections of a given .ssd hosted on the website) to the beeb. You will need to define the SSID and password in a secrets.h file, and either edit the IP address, gateway and DNS defined in the code or remove them to enable DHCP client. Edit as required, compile and upload to the Arduino. The Arduino will act as a file system host to the BBC Micro.

### UPURSFS ROM 

The required ROM can be downloaded from [UPURSFS](https://sweh.spuddy.org/Beeb/ "UPURSFS"), burnt to [EPROM](http://anachrocomputer.blogspot.com/2014/11/roms-for-bbc-micro.html) and inserted in a spare BBC Micro ROM socket. 

### UPURS connection

The Arduino is then connected to the BBC User Port as specified by [UPURS](https://www.retro-kit.co.uk/UPURS/).  The sketch in this repository is currently specific to MKR1000 / SAMD21 based Arduino boards. This hardware requires all IO signals to be level shifted - 5v at the BBC and 3.3v at the Arduino. On the BBC side, UPURS also assumes RX and TX levels are inverted. In this set-up we are doing this in external hardware (with a SN74ACT14N) so the Arduino is presented with standard 115200 baud serial.

<pre>

BBC Micro UPURS connection

.    .    nRX RTS   .    .    .    .   CTS  nTX
   
5V   .    .    .    .    .   GND   .    .    .
                    =====              
                                      
</pre>
*BBC User Port connection (looking into port with BBC upside down...)*






## Usage

![Screenshot](https://github.com/8bitkick/ArduinoHost/blob/master/screenshot2.jpg?raw=true)

With the BBC Micro powered on

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






