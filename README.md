# ArduinoHost

Enables a BBC Micro to view an Arduino as a filing system host over a serial connection. 

Arduino connects to wifi, enabling the BBC Micro to mount a .ssd DFS disc image over the Internet.


## Set up

Assumes a BBC Micro with UPURSFS ROM installed, connected to an Arduino via the BBC User Port as specified by UPURS. 

Currently specific to MKR1000 / SAMD21 based Arduino boards. This hardware requires all signals to be level shifted from 5v to 3.3v to the Arduino. On the BBC side, UPURS assumes RX and TX levels are inverted. In this set-up we are doing this in external hardware (with a SN74ACT14N) so the Arduino is presented with standard 115200 baud serial.

The Arduino connects to your wifi AP and loads the Arcadians DFS disc image from bbcmicro.co.uk.


![Screenshot](https://github.com/8bitkick/ArduinoHost/raw/Developer/screenshot.jpg)


## Current status - basic functionality

* BBC successfully boots into HostFS

* ArduinoHost mounts an SSD image hosted at bbcmicro.co.uk (read-only)

* Successfully runs game direct from the Internet :)

* `\*LOAD, \*RUN, \*., \*WIFI implemented

## Usage

Connect to WiFi and the remote disc

`*WIFI`

Show .ssd contents

`*.`

Run game

`CHAIN "ARCADIA"`

## To do

Lots more functions for the TubeHost need to be implemented and refined.

For the web disc, the ability to search / browse bbcmicro.co.uk from the beeb and load any game. This can be done by doing an HTTP GET to the site using the ?search query string, and stripping tags from the response to list games. Ideally there might be a neater way that just returns json...





