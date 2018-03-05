// Arduino Host
//
//
// HostFS ported from Sweh
// github.com/sweharris/TubeHost/blob/master/TubeHost
//
// Arduino-UPURS I/O from Myelin
// github.com/google/myelin-acorn-electron-hardware/blob/master/upurs_usb_port/upurs_usb_port.ino


// --- Serial comms options ---

// UPURS uses inverted logic because it expects the User Port pins to be wired
// (with a series resistor and a diode from GND) straight to pins at RS232
// levels (+13V = 0, -13V = 1).

// Arduino MRK1000 uses hardware SERCOM instead of software serial for additional UART instances
// so inversion must be done externally in hardware

#define USE_ARDUINO_MKR // We assume RX & TX signals are inverted externally as we can't use SofwareSerial. 
// NB Must use 5v <> 3.3v level shifter with Arduino MKR

//#define WIFI

#include <Arduino.h>
#include "wiring_private.h"

// WiFi setup
#ifdef WIFI

#include <SPI.h>
#include <WiFi101.h>
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;
WiFiClient client;

#endif WIFI


// SERIAL WIRE DEFINES 
#define RXD_PIN 0   // Serial receive 
#define TXD_PIN 1   // Serial transmit
#define RTS_PIN 3   // active-high output Requesting them To Send. We are always ready so this stays high
#define CTS_PIN 4   // active-high input tells us we're Cleared To Send. (Using 4 as we can attach interrupt)


// TUBE HOST COMMAND DEFINES
// mdfs.net/Software/Tube/Serial/Protocol

#define ESCAPE        0x7F
#define OSRDCHIO      0x00 // Cy A
#define OSCLI         0x02 // string 0x0D                   0x7F or 0x80
#define OSBYTELO      0x04 // X A                           X
#define OSBYTEHI      0x06 // X Y A                         Cy Y X
#define OSWORD        0x08 // A in_length block out_length  block
#define OSWORD0       0x0A // block                         0xFF or 0x7F string 0x0D
#define OSARGS        0x0C // Y block A                     A block
#define OSBGET        0x0E // Y                             Cy A
#define OSBPUT        0x10 // Y A                           0x7F
#define OSFIND        0x12 // 0x00 Y                        0x7F
#define OSFIND        0x12 // A string 0x0D                 A
#define OSFILE        0x14 // block string 0x0D A           A block
#define OSGBPB        0x16 // block A                       block Cy A
#define OSFSC         0x18 // X Y A                         0xFF Y X or 0x7F

int long timer = 0;

// FILESYSTEM (fudged for now)
uint8_t owlLogo [4618] = {0,255,0,255,0,255,0,8,7,1,15,1,31,1,0,3,15,1,255,4,0,3,254,1,255,4,0,4,224,1,248,1,254,1,255,1,0,15,3,1,0,3,1,1,31,1,127,1,255,2,0,3,255,5,0,3,224,1,252,1,255,3,0,5,128,1,192,1,224,1,0,12,1,1,7,1,31,1,63,1,0,3,63,1,255,4,0,3,252,1,255,4,0,4,192,1,240,1,252,1,254,1,0,14,1,1,3,1,0,3,1,1,31,1,255,3,0,3,255,5,0,3,192,1,252,1,255,3,0,6,128,1,224,1,0,12,3,1,15,1,63,1,127,1,0,3,63,1,255,4,0,3,248,1,255,4,0,4,128,1,224,1,248,1,252,1,0,13,1,1,7,1,15,1,0,3,7,1,127,1,255,3,0,3,255,5,0,3,128,1,240,1,254,1,255,2,0,7,128,1,0,12,7,1,31,1,127,1,255,1,0,3,255,5,0,3,240,1,255,4,0,5,192,1,240,1,248,1,0,13,3,1,7,1,15,1,0,3,7,1,127,1,255,3,0,3,254,1,255,4,0,4,224,1,252,1,254,1,255,1,0,7,128,1,0,7,1,1,0,4,15,1,63,1,255,2,0,3,255,5,0,3,224,1,254,1,255,3,0,5,128,1,224,1,240,1,0,255,0,1,63,2,127,3,63,1,31,1,15,1,255,24,128,2,192,2,224,1,252,1,255,2,3,1,7,2,15,2,127,1,255,26,240,1,248,4,240,1,224,1,192,1,0,8,127,2,255,2,127,2,63,1,31,1,255,22,254,1,252,1,0,2,128,2,0,4,7,1,15,4,7,1,3,2,255,24,224,1,240,3,248,1,255,3,0,2,1,2,3,1,31,1,255,26,254,2,255,6,0,4,128,1,241,1,255,2,15,1,31,3,63,1,255,27,192,1,224,4,192,1,128,1,0,1,1,2,3,2,1,2,0,2,255,7,127,1,255,16,252,2,254,1,252,3,248,1,240,1,0,8,31,1,63,4,31,1,15,1,7,1,255,24,128,1,192,3,224,1,252,1,255,2,3,3,7,1,15,1,63,1,255,26,248,2,252,2,248,1,240,2,224,1,0,255,0,1,7,1,0,7,255,2,31,1,0,5,255,3,31,1,3,3,7,1,255,30,254,2,255,3,224,1,128,1,0,3,255,1,254,1,224,1,0,5,128,1,0,15,7,1,1,1,0,6,255,2,63,1,0,5,255,2,254,1,0,5,240,1,192,1,0,22,255,1,63,1,3,1,0,5,255,3,3,1,0,4,255,5,127,1,63,2,255,27,248,1,224,1,192,1,224,2,255,3,63,1,15,1,7,2,15,1,255,28,254,1,252,1,248,2,255,3,192,1,0,4,254,1,248,1,128,1,0,21,31,1,7,1,0,6,255,3,0,5,255,2,240,1,0,5,192,1,0,15,3,1,0,7,255,1,127,1,15,1,0,5,255,3,15,1,3,1,1,1,0,2,255,35,240,1,128,1,0,2,128,1,255,1,254,1,224,1,0,5,128,1,0,255,0,10,1,1,7,1,15,1,31,1,63,1,127,1,0,1,63,1,255,6,63,1,255,21,248,1,224,1,255,5,248,1,0,2,252,1,248,1,240,1,224,1,128,1,0,37,3,1,15,1,31,1,63,1,127,2,0,1,127,1,255,6,0,1,254,1,255,6,0,2,192,1,240,1,252,1,254,1,255,2,0,32,63,1,31,1,15,1,3,1,0,4,255,4,127,1,15,1,0,2,255,6,15,1,3,1,255,38,224,1,128,1,255,4,254,1,224,1,0,2,240,2,224,1,128,1,0,34,1,1,3,1,0,2,7,1,63,1,127,1,255,3,0,1,255,7,0,1,248,1,255,6,0,3,224,1,240,1,248,1,252,2,0,32,127,2,63,1,15,1,1,1,0,3,255,5,63,1,0,2,255,6,63,1,7,1,255,8,248,1,255,7,0,1,240,1,254,1,255,5,0,3,192,1,224,1,240,1,248,1,252,1,0,255,0,1,127,3,63,1,31,1,15,1,3,1,0,1,255,7,127,1,255,13,252,1,248,1,192,2,128,3,0,34,3,1,7,1,0,4,31,1,255,3,0,2,3,1,31,1,255,36,128,2,192,1,252,1,255,4,0,4,248,1,255,3,0,5,128,1,192,1,240,1,0,24,1,2,0,6,255,4,127,1,31,1,15,1,1,1,255,18,254,2,252,1,248,1,224,1,0,38,3,1,15,1,31,1,0,4,127,1,255,3,3,2,7,1,127,1,255,28,254,2,255,6,0,3,240,1,255,4,0,4,224,1,252,1,255,2,0,6,128,1,192,1,0,24,7,1,3,2,1,2,0,3,255,5,127,1,63,1,7,1,255,15,252,3,248,1,240,2,192,1,128,1,0,255,0,10,7,1,0,6,63,1,252,1,0,6,255,1,0,7,128,1,0,32,15,1,31,2,63,1,31,2,15,1,7,1,255,25,252,1,224,3,248,1,255,3,31,1,7,1,3,1,7,1,15,1,255,26,248,1,252,5,248,1,240,1,0,40,31,1,0,7,240,1,0,47,63,1,127,6,31,1,255,25,240,1,192,1,128,2,224,1,255,3,63,1,31,1,15,2,63,1,255,26,224,1,240,2,248,1,240,2,224,1,192,1,0,39,3,1,63,1,0,6,255,1,192,1,0,6,248,1,0,255,0,9,1,1,7,1,15,1,31,1,63,2,127,2,255,16,240,1,252,1,255,6,0,4,128,2,192,2,0,24,3,1,0,7,255,2,63,1,3,1,0,4,255,4,3,1,1,1,0,2,255,7,127,1,255,28,224,1,128,2,0,1,255,2,254,1,128,1,0,4,224,1,128,1,0,102,15,1,3,1,0,6,255,3,15,1,0,4,255,4,15,1,7,1,3,1,1,1,255,29,254,2,252,1,255,4,128,1,0,3,255,2,248,1,0,5,128,1,0,34,1,1,3,2,7,2,31,1,127,1,255,22,0,1,192,1,224,1,240,1,248,2,252,1,248,1,0,255,0,1,63,2,31,1,7,1,3,1,0,3,255,5,127,1,3,1,0,1,255,7,7,1,255,8,240,1,255,7,0,1,128,1,254,1,255,5,0,3,192,1,240,1,248,1,252,1,254,1,0,32,127,2,63,1,15,1,3,1,0,3,255,6,1,1,0,1,255,6,224,1,0,1,255,1,254,2,248,1,224,1,0,38,1,1,7,1,15,1,31,1,63,1,0,2,63,1,255,5,0,2,255,6,0,2,128,1,240,1,254,1,255,3,0,6,128,2,0,5,1,2,3,1,0,2,7,1,31,1,127,1,255,3,0,1,63,1,255,6,0,2,248,1,255,5,0,4,192,1,224,1,240,1,248,1,0,24,1,1,0,7,255,3,63,1,15,1,3,1,0,2,255,6,15,1,0,1,255,5,254,1,128,1,0,1,252,1,248,2,224,1,128,1,0,38,7,1,31,1,63,1,127,1,255,1,0,1,3,1,255,6,31,1,255,21,254,1,192,1,255,5,248,1,0,2,248,1,240,1,224,1,192,1,0,255,0,11,3,1,15,1,0,4,1,1,127,1,255,2,3,1,1,1,3,1,15,1,255,28,254,2,255,6,0,3,192,1,255,4,0,5,248,1,255,2,0,7,192,1,0,72,63,4,31,1,15,1,7,1,1,1,255,24,192,2,224,1,240,1,255,4,7,2,15,1,31,1,255,28,248,4,240,1,224,1,192,1,0,79,1,1,7,1,0,4,3,1,63,1,255,2,0,1,1,2,7,1,255,36,0,2,128,1,192,1,255,4,0,4,128,1,252,1,255,2,0,7,192,1,0,255,0,1,31,1,63,2,127,3,63,1,31,1,255,26,224,1,192,3,240,1,255,3,31,1,15,3,63,1,255,25,224,1,240,1,248,3,252,1,255,2,0,7,255,1,0,72,63,1,0,7,255,1,63,1,7,1,1,2,0,3,255,7,127,1,255,21,254,2,252,1,255,1,252,1,192,1,0,5,248,1,0,78,1,1,0,6,1,1,255,1,15,1,31,2,63,2,127,1,255,28,240,1,224,1,192,2,128,2,255,2,15,1,7,1,3,2,1,2,255,24,224,1,240,1,248,4,240,2,0,255,0,1,15,1,7,1,1,1,0,5,255,3,63,1,0,4,255,4,127,1,7,1,3,1,1,1,255,37,128,1,0,2,255,5,127,1,63,2,255,16,240,1,252,1,255,6,0,3,128,1,192,2,224,2,0,72,63,1,31,1,7,1,0,5,255,3,127,1,0,4,255,3,254,1,0,4,248,1,240,1,192,1,0,79,1,1,3,1,7,1,15,3,31,1,127,1,255,27,252,1,248,2,255,4,192,1,0,3,255,1,252,1,240,1,128,1,0,20,255,1,63,1,15,1,0,5,255,4,0,4,255,2,254,1,224,1,0,4,224,1,128,1,0,255,0,11,3,1,15,1,31,1,63,1,0,2,15,1,255,5,3,1,15,1,255,21,254,1,255,16,0,1,225,1,255,6,127,1,255,22,223,1,255,8,240,1,252,1,255,6,0,2,248,1,255,5,0,3,128,1,240,1,252,1,254,2,0,13,1,1,3,1,7,1,0,2,1,1,31,1,127,1,255,3,0,2,255,6,0,2,128,1,248,1,254,1,255,3,0,5,128,1,192,1,224,1,0,11,1,1,15,1,63,1,127,1,255,1,0,2,31,1,255,5,0,2,240,1,255,5,0,4,224,1,240,1,248,1,254,1,0,12,1,1,3,1,15,2,0,2,7,1,63,1,255,4,0,2,255,6,0,2,128,1,240,1,254,1,255,3,0,6,128,1,192,1,0,7,1,1,0,3,7,1,31,1,127,1,255,2,0,2,127,1,255,5,31,1,127,1,255,21,240,1,255,5,252,1,224,1,0,1,240,1,224,2,192,1,128,1,0,34,1,1,0,3,3,1,31,1,127,1,255,2,0,2,127,1,255,5,0,2,192,1,252,1,255,4,0,4,128,1,192,1,240,2,0,255,0,1,127,4,63,1,31,1,15,1,7,1,255,24,224,1,192,2,224,1,248,1,255,3,31,1,15,3,127,1,255,27,254,1,248,2,252,1,255,4,1,1,0,2,1,1,7,1,255,35,0,2,128,1,192,1,240,1,255,3,15,2,31,2,127,1,255,27,240,3,248,1,254,1,255,3,0,1,1,2,3,1,15,1,255,27,254,5,252,1,248,1,224,1,0,8,31,5,15,1,7,1,1,1,255,23,254,1,224,4,192,2,128,1,0,1,1,1,3,2,1,2,0,3,255,6,127,1,31,1,255,17,254,2,252,2,248,1,240,1,192,1,0,48,3,4,1,2,0,2,255,6,127,1,63,1,255,16,248,1,252,2,248,2,240,1,224,1,128,1,0,255,0,1,1,1,0,7,255,1,31,1,0,6,255,2,31,1,3,1,1,2,3,1,31,1,255,34,227,1,128,1,0,2,128,1,225,1,255,3,127,1,63,2,127,1,255,27,252,1,240,1,224,2,240,1,252,1,255,2,63,1,7,1,3,2,7,1,31,1,255,28,254,2,255,4,195,1,0,4,195,1,255,4,63,1,127,1,255,28,248,1,224,1,192,2,224,1,248,1,255,1,252,1,0,6,128,1,0,23,127,1,15,1,0,6,255,2,0,6,248,1,192,1,0,22,7,1,0,7,255,2,0,6,255,1,248,1,0,70,7,1,0,7,255,1,127,1,0,6,254,1,224,1,0,255,0,17,7,1,15,1,31,1,63,2,127,1,31,1,255,28,248,1,224,1,192,1,255,5,127,1,15,2,255,30,252,1,248,1,255,5,135,1,1,1,0,1,255,37,240,1,192,1,128,1,255,5,127,1,31,2,255,29,254,1,248,1,240,1,255,5,15,1,3,1,1,1,255,16,252,1,255,7,0,1,128,1,224,1,248,1,252,1,254,2,255,1,0,140,1,2,3,2,0,1,7,1,63,1,255,5,127,1,255,7,192,1,252,1,255,6,0,2,128,1,192,1,224,1,240,1,248,2,0,255,0,1,127,1,63,2,31,1,15,1,3,1,0,2,255,7,15,1,255,16,192,1,224,1,255,6,15,1,31,1,255,30,252,1,254,1,255,6,0,1,1,1,223,1,255,37,128,1,192,1,251,1,255,5,31,1,63,1,255,30,240,1,248,1,255,6,1,1,3,1,31,1,255,28,248,1,255,1,254,2,252,1,248,1,224,1,128,1,0,137,3,2,1,2,0,4,255,4,127,1,31,1,7,1,0,1,255,7,30,1,255,6,248,1,0,1,248,2,240,1,224,1,192,1,0,255,0,20,15,1,3,1,1,2,0,4,255,6,127,1,63,1,255,24,192,1,0,3,128,1,255,4,127,1,63,2,127,1,255,27,248,1,224,3,240,1,255,3,15,1,7,1,3,2,7,1,255,28,254,1,252,1,254,1,255,4,129,1,0,4,255,4,127,1,63,1,127,1,255,28,248,1,224,1,192,2,240,1,255,3,0,6,254,1,255,1,0,7,192,1,0,150,1,1,15,1,0,6,255,2,0,6,224,1,254,1,0,255,0,33,15,1,3,1,0,6,255,2,31,1,0,5,255,3,63,1,15,2,7,2,255,28,252,1,248,1,252,1,254,1,255,3,3,1,0,3,1,1,255,35,224,1,128,3,192,1,255,3,127,1,31,1,15,1,31,1,63,1,255,27,252,1,248,1,240,2,252,1,255,3,7,1,1,3,3,1,255,24,240,1,248,1,252,1,254,2,255,3,0,7,192,1,0,130,1,1,3,5,63,1,255,23,128,1,192,1,240,2,248,3,240,1,0,255,0,41,3,1,1,1,0,6,255,3,63,1,15,1,0,3,255,5,127,1,0,2,255,7,127,1,255,30,248,1,224,1,255,6,15,1,7,1,255,31,254,1,255,6,1,1,0,1,255,7,127,1,255,30,240,1,224,1,255,6,31,1,15,1,255,16,0,1,248,1,255,6,0,3,192,1,224,1,240,1,248,2,0,112,1,1,0,7,255,2,127,1,31,1,3,1,0,3,255,5,0,3,255,3,254,1,240,1,0,3,240,1,224,1,128,1,0,255,0,70,63,1,31,2,15,1,7,1,3,1,0,2,255,7,63,1,255,16,224,2,248,1,255,5,3,1,7,1,15,1,255,29,252,1,254,1,255,6,0,3,255,5,63,1,127,1,255,30,192,1,224,1,240,1,255,5,7,1,15,1,31,1,255,29,252,2,255,6,0,3,254,1,255,4,0,4,224,1,252,1,255,2,0,7,128,1,0,100,3,1,31,1,127,1,255,1,0,4,255,4,0,4,240,1,254,1,255,2,0,6,128,1,192,1,0,255,0,81,255,1,3,1,0,6,255,4,127,2,63,1,31,1,255,25,224,1,128,2,192,1,224,1,255,3,63,1,31,1,15,1,31,1,127,1,255,27,252,1,240,2,248,1,252,1,255,3,3,1,1,3,7,1,255,35,128,1,0,3,192,1,255,3,127,1,63,1,31,1,63,1,255,27,192,2,224,1,240,2,252,1,255,2,0,6,240,1,255,1,0,7,128,1,0,72,1,1,3,5,1,1,0,1,255,24,224,1,240,1,248,2,240,2,224,1,192,1,0,255,0,89,15,1,3,1,0,6,255,3,1,1,0,4,255,4,15,1,7,1,3,2,255,29,254,1,252,1,254,1,255,4,0,4,255,5,127,1,63,1,127,1,255,28,224,1,192,2,224,1,255,4,31,1,7,2,15,1,255,28,254,1,252,3,255,4,1,1,0,3,255,24,240,1,252,1,254,1,255,5,0,5,128,2,192,1,0,72,63,1,31,1,3,1,0,5,255,3,0,5,255,1,254,1,240,1,0,5,128,1,0,255,0,112,1,1,0,7,255,2,127,1,63,1,15,1,1,1,0,2,255,6,3,1,0,1,255,7,63,1,255,8,129,1,255,22,248,1,255,6,191,1,3,1,255,8,240,1,255,7,31,1,255,22,128,1,255,7,127,1,255,8,3,1,255,22,248,1,255,7,7,1,255,8,240,1,255,7,0,1,224,1,254,1,255,5,0,3,192,1,224,1,248,2,252,1,0,53,1,2,3,1,0,2,7,1,63,1,127,1,255,3,0,1,63,1,255,6,0,2,248,1,255,5,0,4,192,1,224,1,240,2,0,255,0,129,31,1,15,2,7,2,3,1,0,2,255,7,63,1,255,16,240,2,248,1,254,1,255,4,1,2,3,1,15,1,255,30,254,2,252,1,248,1,240,1,192,1,0,8,31,3,15,2,7,1,1,1,0,1,255,7,127,1,255,16,240,3,252,1,255,4,3,2,7,1,31,1,255,28,252,1,254,1,255,6,0,3,192,1,255,4,0,4,128,1,248,1,255,2,0,7,128,1,0,24,3,4,1,1,0,3,255,6,63,1,15,1,255,15,254,1,248,3,240,1,224,1,192,1,128,1,0,255,0,138,7,1,0,6,1,1,255,1,3,1,0,4,3,1,255,4,127,1,63,1,127,1,255,27,252,1,224,2,192,1,128,2,0,1,254,1,0,31,15,1,0,6,7,1,255,1,15,1,1,1,0,3,7,1,255,34,248,1,192,1,128,3,0,2,255,2,31,1,15,2,7,2,3,1,255,24,192,1,224,1,240,2,248,2,255,2,0,7,255,1,0,32,255,1,0,6,255,1,224,1,0,6,192,1,0,255,0,139,1,1,3,1,7,1,15,2,31,2,255,28,248,1,240,2,255,4,0,4,254,1,252,1,224,1,0,38,1,1,7,1,15,2,31,3,127,1,255,27,254,1,248,1,240,2,255,3,254,1,0,4,252,1,248,1,224,1,0,13,1,1,0,7,255,1,127,1,31,1,1,1,0,4,255,4,1,1,0,3,255,5,127,1,63,2,255,16,224,1,252,1,254,1,255,5,0,4,128,2,192,2,0,3,1,1,3,3,7,1,15,1,63,1,255,14,252,1,255,7,0,2,192,1,224,1,240,1,248,3,0,255,0,28,7,1,31,1,127,1,255,2,0,2,255,6,0,2,252,1,255,5,0,3,128,1,224,1,248,1,252,1,254,1,0,12,3,1,7,1,15,1,31,1,0,2,31,1,127,1,255,4,0,2,255,6,0,2,192,1,248,1,254,1,255,3,0,6,128,1,192,1,0,7,1,1,0,3,15,1,63,1,127,1,255,2,0,2,255,6,63,1,255,22,195,1,255,8,248,1,255,7,0,2,255,6,0,3,240,1,252,1,254,1,255,2,0,7,128,1,0,6,1,1,3,1,0,2,3,1,31,1,127,1,255,3,0,1,1,1,255,6,127,1,255,22,135,1,255,8,248,1,254,1,255,6,0,2,255,6,0,3,224,1,248,1,252,1,254,1,255,1,0,32,31,2,15,1,7,1,1,1,0,3,255,5,127,1,3,1,0,1,255,7,7,1,255,8,224,1,248,1,255,6,7,1,63,1,255,21,192,1,255,5,252,1,128,1,0,1,240,2,224,1,192,1,0,255,0,21,1,4,0,4,255,5,127,1,63,1,15,1,255,16,254,4,252,2,248,1,224,1,0,8,31,1,63,2,31,2,15,1,7,1,1,1,255,23,252,1,224,3,192,2,128,1,0,2,3,3,1,2,0,3,255,6,127,1,31,1,255,16,254,2,252,2,248,1,240,1,224,1,192,1,0,8,255,1,127,1,63,2,31,1,15,1,7,1,1,1,255,22,254,1,248,1,128,1,192,2,128,2,0,3,3,1,7,2,3,1,1,2,0,2,255,7,63,1,255,16,254,1,252,1,248,2,240,2,224,1,128,1,1,1,0,7,255,2,127,3,63,1,15,1,7,1,255,21,254,1,252,1,240,1,0,1,128,2,0,45,1,1,0,7,255,3,127,2,63,1,31,1,7,1,255,24,0,3,128,1,240,1,255,3,0,5,224,1,252,1,255,1,0,7,128,1,0,255,0,25,3,1,0,7,255,1,127,1,0,6,255,1,248,1,0,30,63,1,3,1,0,6,255,2,0,6,240,1,0,23,3,1,0,7,255,1,63,1,0,6,254,1,224,1,0,30,127,1,7,1,0,6,255,1,252,1,0,6,192,1,0,23,7,1,0,7,255,1,127,1,0,6,252,1,192,1,0,30,255,1,15,1,0,6,255,1,252,1,0,6,128,1,0,63,1,1,0,7,255,1,15,1,0,6,255,2,31,1,7,1,3,3,1,1,255,24,192,1,224,1,240,2,248,2,240,2,0,255,0,255,0,98,255,1,127,1,31,1,3,1,0,4,255,4,0,4,255,3,248,1,0,4,224,1,192,1,0,255,0,255,0,255};
// ==========================================================================
// SECTION: Arduino Serial port 
// ==========================================================================


// Fast GPIO access for MKR1000 
#ifdef USE_ARDUINO_MKR

inline void digitalWrite_fast(int pin, bool val) { if (val)  
  PORT->Group[g_APinDescription[pin].ulPort].OUTSET.reg = (1ul << g_APinDescription[pin].ulPin);
  else     
  PORT->Group[g_APinDescription[pin].ulPort].OUTCLR.reg = (1ul << g_APinDescription[pin].ulPin);
}

inline int digitalRead_fast(int pin) {
  return !!(PORT->Group[g_APinDescription[pin].ulPort].IN.reg & (1ul << g_APinDescription[pin].ulPin));
}

#define CTS_IS_ACTIVE (digitalRead_fast(CTS_PIN) == HIGH) 

// Using SAMD21 SERCOM for UART instances
Uart SerialBBC (&sercom3, RXD_PIN, TXD_PIN, SERCOM_RX_PAD_1, UART_TX_PAD_0); // Create the new UART instance assigning it to pin 0 and 1
void SERCOM3_Handler() {
  SerialBBC.IrqHandler();
}
#endif USE_ARDUINO_MKR


// Keep track of CTS timing with this interrupt handler (micros() OK if we're quick about it....)
volatile unsigned long CTS_since = 0;
void CTSchanged() {CTS_since = micros();} 

void setup_serial() {
  
  // Using SAMD21 SERCOM for UART instances
  #ifdef USE_ARDUINO_MKR 
  pinPeripheral(RXD_PIN, PIO_SERCOM); //Assign RX function to pin 0
  pinPeripheral(TXD_PIN, PIO_SERCOM); //Assign TX function to pin 1
  #endif USE_ARDUINO_MKR
  
  // SET UP SERIAL
  Serial.begin(115200);
  SerialBBC.begin(115200);
  
  pinMode(RTS_PIN, OUTPUT);
  digitalWrite(RTS_PIN, HIGH);        
  pinMode(CTS_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CTS_PIN), CTSchanged, RISING); 
}


// Implement hardware flow control by monitoring CTS
void raw_write(byte byteOut) {
  while (
    !(
      CTS_IS_ACTIVE &&                // We must be clear to send
      (micros() - CTS_since > 3) &&   // for at least 3us
      (micros() - CTS_since < 20)     // and last successful byte transfer <20us ago
    )
  ){};                               // Otherwise wait for this condition
  SerialBBC.write(byteOut); 
  
  if (CTS_IS_ACTIVE) {
    CTS_since = micros()+3;
  }
}

int long raw_read() {while (SerialBBC.available() == 0){};return SerialBBC.read();}


// ==========================================================================
// SECTION: Serial I/O routines for communication with Client
// ==========================================================================

int long read_addr() {uint32_t addr = 0;for( int a = 0; a < 4; a++ ){addr = addr * 256 + raw_read();}return addr;}

void send_byte(byte byteOut) {raw_write(byteOut);if (byteOut==ESCAPE) {raw_write(ESCAPE);};}

void send_cmd(byte byteOut) {raw_write(ESCAPE);raw_write(byteOut);}

void send_addr(uint32_t addr) {send_byte(addr >> 24);send_byte((addr >> 16) & 0xFF);send_byte((addr >> 8) & 0xFF);send_byte(addr & 0xFF);}

void send_data(uint8_t *data, int length) {
  for(int n=0; n < length; n++) {
    send_byte(data[n]);
  }
}

void send_data_to_memory(uint32_t loadAddr, uint8_t *data, int length) {
  send_cmd(0xE0);  // Load data response
  send_addr(loadAddr); // Address to load to 
  send_data(data, length);      // Data;
  send_cmd(0xB0);  // End 
}

void send_string(String message) {
  unsigned int len = message.length();
  for(int n=0; n < len; n++) {
    send_byte(message[n]);
  }
};

void send_reply(String message) {
  unsigned int len = message.length();
  for(int n=0; n < len; n++) {
    send_byte(message[n]);
  }
  send_byte(0x00);
};

void send_output(String message) {
  send_byte(0x40);
  send_reply(message);
};

void read_data_from_memory(uint32_t saveAddr, int length) {
  send_cmd(0xf0);
  send_addr(saveAddr);
  for(int n=0; n < length; n=n+1) {
    byte byteIn = raw_read();
    Serial.write(byteIn);
  }
  send_cmd(0xb0);
  error(214,"Data sent to Arduino console");
}

String read_string() {
  byte nextByte = 0; int index = 0; char stringbuff[80];
  while (nextByte != 0x0D && index<80){
    if (nextByte != 0) {stringbuff[index++] = nextByte;
    }
    nextByte = raw_read();
  }
  stringbuff[index] = 0;
  
  return stringbuff;
}

String extract_filename(String data)
{
  String filename;
  int i = 0; 
  
  while (data[i] != 0x20 && data[i] != 0x00) {
    filename = filename + data[i];
    i++;
  }
  data[i] = 0;
  
  return filename;
}

void error(uint8_t code, const char* message) {
  send_cmd(0);
  send_byte(code);
  send_reply(message);
}

void send_image(uint32_t loadAddr, uint8_t *data, int length) {
  send_cmd(0xE0);  // Load data response
  send_addr(loadAddr); // Address to load to 
  
  for(int n=0; n < length; n=n+2) { // Data
    int value = data[n];
    int runlength = data[n+1];
    for(int x=0; x < runlength; x++) {
      send_byte(value);
    }
  }
  
  send_cmd(0xB0);  // End 
}

// ==========================================================================
// SECTION: OSFSC
// ==========================================================================


void runOSFSC() {
  byte X = raw_read(); byte Y = raw_read(); byte A = raw_read();
  uint8_t response [60] = 
  "ArduinoHost Fake Dev Disc \r\nDirectory :0.$ \r\n\r\n OWL\r\n\r\n\0";
  
  switch (A)
  {
    /*
    case 0: // OPT Handler
    case 1: // Check EOF
    case 2: // *Handler
    
    case 4: // *RUN handler
    
    case 6: // Shutdown files
    case 7: // Give file handle ranges
    case 8: // *ENABLE checker */
    
    case 3: // Unrecognised * command handler
    OSFSC_cmd(X,Y);
    break;
    
    case 5: // *CAT handler
    send_byte(127);
    //String catNam= read_string();
    send_byte(0x40);
    send_data(response, 60);
    send_byte(0);
    break;
    
    
    case 255: // BOOT (See https://github.com/sweharris/TubeHost )
    send_byte(255); send_byte(0);send_byte(X);
    break;
    
    default:
    error(214,"ArduinoHost: unhandled OSFSC");
    Serial.print("Unhandled OSFSC A=0x");
    Serial.print(A, HEX);
    break;
    
  }  // end of switch
}

// Handle custom * commands here
// -----------------------------
// TODO - just a test
void OSFSC_cmd (uint8_t X, uint8_t Y){
  send_byte(ESCAPE); // tell client to send us the command
  String command = read_string();
  
  Serial.println("*"+command);
  
  #ifdef WIFI  
  if (command=="WIFI") {
    send_byte(0x40); // Reponse start
    send_wifi_status();
    send_byte(0x00); // Reponse end
  } else
  {
    error(214,"ArduinoHost: unhandled * command");
  }
  #else
  error(214,"ArduinoHost: unhandled * command");
  #endif
}

// ==========================================================================
// SECTION: OSFILE
// ==========================================================================


void runOSFILE() {
  uint32_t end  = read_addr();
  uint32_t save = read_addr();
  uint32_t exec = read_addr();
  uint32_t load = read_addr();
  String filename = extract_filename(read_string());
  byte A = raw_read();
  
  switch (A) {
    case 0: // SAVE DATA
    timer = micros();
    read_data_from_memory(save, end-save);
    timer = micros() - timer;
    
    Serial.println("*SAVE " + filename);
    Serial.print(timer, DEC);
    Serial.println("us ");
    
    break;
    
    case 255: // LOAD
    timer = micros();
    Serial.println(filename+"*");
    if (filename=="OWL") {
      send_image(load, owlLogo, 4618); 
    }
    else {
      error(214,"File not found \r\nPlease use *LOAD OWL 3000 in MODE 0");
    }
    
    //
    timer = micros() - timer;
    
    Serial.println("*LOAD " + filename);
    Serial.print(timer, DEC);
    Serial.println("us ");
    break;
    
    default:
    error(214,"ArduinoHost: unhandled OSFILE");
    Serial.print("Unhandled OSFILE A=0x");
    Serial.print(A, HEX);
    break;
    
  }  // end of switch
  
  // Send results
  send_byte(1);    // File found; directories aren't possible
  send_addr(end);
  send_addr(save);
  send_addr(exec);
  send_addr(load);
}

// Replace with jump table
void runCommand (const byte inByte) {
  switch (inByte) {
    case OSFSC: runOSFSC();
    break;
    
    case OSFILE: runOSFILE();
    break;
    
    default: {
      Serial.print("Unhandled command 0x");
      Serial.print(inByte, HEX);
    }
    break;
  }  // end of switch
}


#ifdef WIFI 
// ==========================================================================
// SECTION: WiFi & web client
// ==========================================================================


/*
if (client.connect(server, 80)) {
Serial.println("connected to server");
// Make a HTTP request to Google cache, text only
String URL = "en.wikipedia.org/wiki/BBC_Micro";

client.println("GET /search?q=cache:https://"+URL+"&num=1&strip=1&vwsrc=0 HTTP/1.1");
client.println("Host: webcache.googleusercontent.com");
client.println("Connection: close");
client.println();
}
}
void WiFiloop() {
// if there are incoming bytes available
// from the server, read them and print them:
while (client.available()) {
char c = client.read();
Serial.write(c);
}

// if the server's disconnected, stop the client:
if (!client.connected()) {
Serial.println();
Serial.println("disconnecting from server.");
client.stop();

// do nothing forevermore:
while (true);
}
}
*/

String send_wifi_status() {
  send_string("\r\nSSID:");
  send_string(WiFi.SSID());
  send_string("\r\nLocal IP Address: ");
  
  uint32_t myAddr = WiFi.localIP();
  
  String first_octet = String((myAddr >> 24) & 0xFF);
  byte second_octet = (myAddr >> 16) & 0xFF;
  byte third_octet = (myAddr >> 8) & 0xFF;
  byte fourth_octet = myAddr & 0xFF;
  
  String  localIP =  first_octet  + "." + second_octet  + "." + third_octet  + "." + fourth_octet;
  String  signal = String(WiFi.RSSI());
  send_string(localIP);
  send_string("\r\nSignal strength: ");
  send_string(signal+"dBm\r\n");
}
#endif 

// ==========================================================================
// SECTION: SETUP & MAIN
// ==========================================================================


void setup() {
  setup_serial();
  
}


void loop() {
  // Check for commands from BBC Micro
  if (SerialBBC.available () && (raw_read() == ESCAPE)) {
    int nextByte = raw_read();
    if (nextByte != ESCAPE) {runCommand (nextByte);}; // run command if it's not an escaped escape
  }
  
  #ifdef WIFI  
  // WiFi
  if (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
  }
  #endif 
}  

