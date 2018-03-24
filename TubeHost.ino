// Arduino Host - 8bitkick
// ------------------------
// 
// Storage medium is BBC Micro DFS disc images (.ssd) served from bbcmicro.co.uk
// 
//
// Reference
//
// HostFS - Sweh
// github.com/sweharris/TubeHost/blob/master/TubeHost
//
// Arduino-UPURS I/O - Myelin
// github.com/google/myelin-acorn-electron-hardware/blob/master/upurs_usb_port/upurs_usb_port.ino
//


#include <Arduino.h>
#include "wiring_private.h"
#include <SPI.h>
#include <WiFi101.h>
#include <FlashStorage.h>

#define DEBUG


// WIFI & WEB

char ssid[] = SECRET_SSID;   
char pass[] = SECRET_PASS;  
int status = WL_IDLE_STATUS;

WiFiClient client;

#define TCPPORT 80
#define MAX_WEBLINKS 10

String webhost = "www.bbcmicro.co.uk";
String webdisc = "/gameimg/discs/79/Disc005-DareDevilDenis.ssd"; // This is effectively our current URL
String weblink[MAX_WEBLINKS]; // Here we cache hyperlinks from searching the given webhost
int    weblinks = 0;


// SERIAL WIRE DEFINES 

#define USE_ARDUINO_MKR // We assume RX & TX signals are inverted externally as we can't use SofwareSerial. 
// NB Must use 5v <> 3.3v level shifter with Arduino MKR

#define RXD_PIN 0   // Serial receive 
#define TXD_PIN 1   // Serial transmit
#define RTS_PIN 3   // active-high output Requesting them To Send. We are always ready so this stays high
#define CTS_PIN 4   // active-high input tells us we're Cleared To Send. (Using 4 as we can attach interrupt)

#define LED_PIN 6


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

// DISC
// http://beebwiki.mdfs.net/Acorn_DFS_disc_format


#define CAT_LENGTH    512

// Boot option :-
// Bit 5	Bit 4	Action
// 0	0	No action
// 0	1	*LOAD $.!BOOT
// 1	0	*RUN $.!BOOT
// 1	1	*EXEC $.!BOOT

struct DFS_Disc
{
  // DFS catalogue information 512 bytes (Sectors 0 & 1)
  char  volumeLo[8];
  char  filename[31][8];
  
  char  volumeHi[4];
  uint8_t cycle;
  uint8_t numberFiles;
  uint8_t bootOption;
  uint8_t sectors;
  
  struct Fileinfo{
    uint16_t loadAddr;
    uint16_t execAddr;
    uint16_t fileLen;
    uint8_t bits;
    uint8_t startSector;
  }fileinfo[31];
};

DFS_Disc disc;


/* TODO - implement defines for bitfield derived DFS values
// from DFS.c - Dominic Beesley
#define DFS_SECTORCOUNT (unsigned int)((dfs->header.sector1.seccount_l \
+ ((dfs->header.sector1.opt4 & 3) << 8)) )

#define DFS_CATCOUNT(dfs) (dfs->header.sector1.cat_count >> 3)

#define DFS_FILE_LOCKED(dfs, index) \
( (dfs->header.sector0.file_names[index].dir & 0x80) != 0)

#define DFS_EX(x) ( ((x & 0x30000) == 0x30000)? x | 0xFC0000:x)

#define DFS_FILE_LOAD(dfs, index) DFS_EX(( dfs->header.sector1.file_addrs[index].load_l \
+   (dfs->header.sector1.file_addrs[index].load_h << 8) \
+ ( (unsigned long int) (dfs->header.sector1.file_addrs[index].morebits & 0x0C) << 14) ))

#define DFS_FILE_LEN(dfs, index) ( dfs->header.sector1.file_addrs[index].len_l \
+ (dfs->header.sector1.file_addrs[index].len_h << 8) \
+ ( (unsigned long int) (dfs->header.sector1.file_addrs[index].morebits & 0x30) << 12) )

#define DFS_FILE_EXEC(dfs, index) DFS_EX(( dfs->header.sector1.file_addrs[index].exec_l \
+ (dfs->header.sector1.file_addrs[index].exec_h << 8) \
+ ( ((unsigned long int) dfs->header.sector1.file_addrs[index].morebits & 0xC0) << 10) ))

#define DFS_FILE_SECTOR(dfs, index) ( dfs->header.sector1.file_addrs[index].secaddr_l \
+ ( (dfs->header.sector1.file_addrs[index].morebits & 0x03 ) << 8) )
*/          


// Teletext colour codes
#define TT_RED     char(129)
#define TT_GREEN   char(130)
#define TT_YELLOW  char(131)
#define TT_BLUE    char(132)
#define TT_MAGENTA char(133)
#define TT_CYAN    char(134)
#define TT_WHITE   char(135)


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
  ){};                                // Otherwise wait for this condition
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
    raw_write(data[n]);
  }
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
  
  while (data[i] != 0x20 && data[i] != 0x00 && i < data.length()) {
    if (data[i] != 34) {filename = filename + data[i];}; // REMOVE " FROM FILENAME
    i++;
  }
  return filename;
}

String extract_parameter(String data)
{
  String ret;
  int i = 0; 
  
  while (data[i] != 0x20 && data[i] != 0x00 && i < data.length()) {
    i++;
  }
  i++;
  while (data[i] != 0x20 && data[i] != 0x00 && i < data.length()) {
    if (data[i] != 34) {ret = ret + data[i];}; // REMOVE " FROM FILENAME
    i++;
  }
  return ret;
}

void error(uint8_t code, const char* message) {
  send_cmd(0);
  send_byte(code);
  send_reply(message);
}

void send_RLE_image(uint32_t loadAddr, uint8_t *data, int length) {
  send_cmd(0xE0);  // Load data response
  send_addr(loadAddr); // Address to load to 
  int bytes = 0;
  for(int n=0; n < length; n=n+2) { // Data
    int value = data[n];
    int runlength = data[n+1];
    for(int x=0; x < runlength; x++) {
      bytes++;
      send_byte(value);
    }
  }
  send_cmd(0xB0);  // End 
}

// BBC LOAD DATA
void send_data_to_memory(uint32_t loadAddr, uint8_t *data, int length) {
  send_cmd(0xE0);               // Load data response
  send_addr(loadAddr);          // Address to load to 
  send_data(data, length);      // Data
  send_cmd(0xB0);               // End 
}

// BBC SAVE DATA
void read_data_from_memory(uint32_t saveAddr, int length) {
  send_cmd(0xF0);
  send_addr(saveAddr);
  for(int n=0; n < length; n=n+1) {
    const byte byteIn = raw_read();
    Serial.write(byteIn);
  }
  send_cmd(0xB0);
  Serial.print(length,DEC);
  Serial.println(" bytes saved\n");
}

// ==========================================================================
// SECTION: OSFSC
// ==========================================================================


void runOSFSC() {
  byte X = raw_read(); byte Y = raw_read(); byte A = raw_read();
  char fname [10];
  switch (A) {
    /*
    case 0: // OPT Handler
    case 1: // Check EOF
    case 2: // *Handler
    case 6: // Shutdown files
    case 7: // Give file handle ranges
    case 8: // *ENABLE checker */
    case 3: // Unrecognised * command handler
    OSFSC_cmd(X,Y);
    break;
    // *RUN handler
    case 4: { 
      send_byte(127); // Request filename
      String filename = extract_filename(read_string());
      int fileID = DFS_find(filename);
      if (fileID != -1) {
        send_cmd(0xE0);   // Start data send
        send_addr(disc.fileinfo[fileID].loadAddr); // Address to load to 
        web_load(fileID); // Send file data
        send_cmd(0xB0);   // End 
        send_cmd(0xc0);   // set execution address
        send_addr(disc.fileinfo[fileID].execAddr);
        send_byte(0x80);  // do it!
      } else {
        error(214,"file not found");
      }
    }
    break;
    // *CAT handler
    case 5: { 
      String response = DFS_cat();
      send_byte(127);
      send_byte(0x40);
      send_string(response);
      send_byte(0);
    }
    break;
    // BOOT (See https://github.com/sweharris/TubeHost )
    case 255: 
    send_byte(255); send_byte(0);send_byte(X);
    setup_wifi(); // We restart wifi on boot
    //  web_mount(); // DEBUG
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
void OSFSC_cmd (uint8_t X, uint8_t Y){
  send_byte(ESCAPE); // tell client to send us the command
  String commandline = read_string();
  String command = extract_filename(commandline);
  String parameter = extract_parameter(commandline);
  
  if (command=="WIFI") {
    setup_wifi();
    if (web_mount()==0) {
      send_byte(0x40); // Reponse start
      send_wifi_status();
      send_byte(0x00); // Reponse end
      return;
    } 
  }
  if (command=="SEARCH") { // Search bbcmicro.co.uk
    send_byte(0x40); // Host FS reponse start
    String match;
    int start;
    int end;
    int num = 0;
    
    http_get(webhost, "/?search=" + parameter, 0, 0);
    send_string(String(TT_RED));
    send_string("Webhost: "+webhost+"\r\n");
    while (client.available() && num < MAX_WEBLINKS){
      String line = client.readStringUntil(0xA);  
      start = line.indexOf("discs/");
      if (start != -1) {
        end = line.indexOf(".ssd",start+6);
        weblink[num] = line.substring(start+6,end);
        start = weblink[num].indexOf("-");
        match = String(TT_CYAN) + num;
        match =  match + String(TT_WHITE) + "-" + String(TT_YELLOW) + weblink[num].substring(start+1) + "\r\n";
        send_string(match); // Host FS send string
        Serial.println(match);
        num++;
      }
    }
    weblinks = num;
    send_byte(0x00); // Host FS reponse end
    return;
  }
  
  if (command=="MOUNT") {
    webdisc = "/gameimg/discs/"+weblink[parameter.toInt()]+".ssd"; // TODO - catch dsd
    web_mount(); // TODO catch error
    send_byte(0x40); // Host FS reponse start
    send_string(String(TT_YELLOW)+webdisc+"\r\n"); // Host FS send string
    send_byte(0x00); // Host FS reponse end
    return;
  }
  error(214,"ArduinoHost: unhandled * command");
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
    case 255:  { // LOAD DATA
      int fileID = DFS_find(filename);
      if (fileID != -1){
        timer = micros();
        if (load==0) {load = disc.fileinfo[fileID].loadAddr;};
        send_cmd(0xE0);  // Start load data 
        send_addr(load); // Address to load to 
        web_load(fileID);// Load data 
        send_cmd(0xB0);  // End 
        timer = micros() - timer;
        Serial.println("*LOAD " + filename);
        Serial.print(timer, DEC);
        Serial.println("us ");
      } else {
        error(214,"file not found");
      }
      break;
    }
    default:
    error(214,"Unhandled OSFILE");
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

// ==========================================================================
// SECTION: Wifi & http get
// ==========================================================================

String send_wifi_status() {
  if (status != WL_CONNECTED) {
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    send_string("Connected to wifi\r\n");
  }  else 
  {
    uint32_t myAddr = WiFi.localIP();
    String first_octet = String((myAddr >> 24) & 0xFF);
    byte second_octet = (myAddr >> 16) & 0xFF;
    byte third_octet = (myAddr >> 8) & 0xFF;
    byte fourth_octet = myAddr & 0xFF;
    String  localIP =  first_octet  + "." + second_octet  + "." + third_octet  + "." + fourth_octet;
    send_string(    "Wifi connected");
    send_string("\r\nLocal IP: ");
    send_string(localIP);
    send_string("\r\nWebhost : "+webhost);
    send_string("\r\n");
  }
}   

void setup_wifi(){  
  // Edit this section for static IP, remove for DHCP
  IPAddress ip(172, 20, 20, 61); 
  IPAddress subnet(255, 255, 255, 0); 
  IPAddress dns(75, 75, 75, 75); 
  IPAddress gateway(172, 20, 20, 1); 
  WiFi.config(ip, dns, gateway, subnet); 
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 1 seconds for connection:
    delay(3000);
  }
  Serial.println("Connected to wifi");
}

// GET
int http_get(String host, String path, int start, int end) {
  client.stop();
  String range = "Range: bytes="; // NB - WE REQUIRE WEBSERVER TO SUPPORT PARTIAL CONTENT
  if (start != 0) {range  = range + start;} else {range = range + "0";}
  range = range + "-";
  range = range + end;
  Serial.println(range);
  // if there's a successful connection:
  if (client.connect(host.c_str(), TCPPORT)) {
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: "+ host);
    if (start != end) {client.println(range);} // Skip range request if start=end
    client.println("Connection: close");
    client.println();
    // Check response 
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 206 Partial Content") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      return 1;
    }
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      Serial.println(F("Invalid response"));
      return 1;
    }
    return 0;
  }
}

char client_read(){
  while (!client.available()){};
  return client.read();
};

char client_empty(){
  while (client.available()){
    Serial.write(client.read());
  } // TODO test for unused data recieved
}

// ==========================================================================
// SECTION: HOST MEDIUM - WEB-BASED BBC MICRO DFS DISC IMAGES (.ssd)
// ==========================================================================
// Take filename and return filehandle (relative pointer to DFS catalogue entry)

String DFS_filename(int fileID) {
  char filename[10]; 
	filename[0] = disc.filename[fileID][7] & 0x7F;
	filename[1] = '.';
	memcpy(filename+2, disc.filename[fileID], 7);
		for (int i=0; i<9; i++) {
		if (filename[i] <= 32)
			filename[i] = 0;
	}
	filename[9] = 0;
	return String(filename);
}

String DFS_volume() {  
  char volume[13];
	memcpy(volume, disc.volumeLo, 8);
	memcpy(volume+8, disc.volumeHi, 4);
		for (int i=0; i<12; i++) {
		if (volume[i] <= 32)
			volume[i] = 0;
	}
	volume[12] = 0;
	return String(volume);
}


int DFS_find(String filename){
  int id = -1;
  if (filename.indexOf('.') != 1) {filename = "$."+filename;}
  for( int a = 0; a < (disc.numberFiles >> 3); a = a + 1 ){
    String thisfile = DFS_filename(a);
    if (filename.equalsIgnoreCase(thisfile)) {id = a;}
  }
  return id;
}

String DFS_cat(){
  // Volume title
  String cat;
  uint8_t files = disc.numberFiles >> 3; // number of files
  cat = " "+String(TT_YELLOW) + "\r\nVolume title: "+DFS_volume()+" \r\n";

  for( int a = 0; a < files; a = a + 1 ){  
    
    cat = cat + DFS_filename(a);
    cat = cat + " ";
    cat = cat + String(disc.fileinfo[a].loadAddr, HEX);
    cat = cat + " ";
    cat = cat + String(disc.fileinfo[a].execAddr, HEX);
    cat = cat + " ";
    cat = cat + String(disc.fileinfo[a].fileLen, HEX);
    cat = cat + " ";
    cat = cat + String(disc.fileinfo[a].startSector, HEX);
    cat = cat + "\r\n";
  }
  Serial.println(cat);
  return cat;
}

String get_string(int len){
  String output = "";
  for( int a = 0; a < len; a = a + 1 ){
    char c = client.read();
    output = output + c;
  }
  return output;
}

int web_mount(){ 
  Serial.println("\nReading "+webhost+webdisc);
  if (http_get(webhost, webdisc, 0, CAT_LENGTH-1)) {Serial.println("*** HTTP GET ERROR ***");return -1;};
  uint8_t buffer [CAT_LENGTH];
  for( int a = 0; a < CAT_LENGTH; a = a + 1 ){
    char c = client_read();
    buffer[a] = c;
  }
  memcpy(&disc,buffer,CAT_LENGTH);
  client_empty();
  Serial.println("*** MOUNTED WEB SSD OK ***");
  return 0;
}

// BBC LOAD DATA
void web_load(int fileID) {
  Serial.print("\n\r FileID ");
  Serial.print(fileID,DEC);
  Serial.print("\n\r ");
  //Serial.print(disc.filename[fileID]); TODO #DEFINE for disc filename
  Serial.print(" ");
  Serial.print(disc.fileinfo[fileID].loadAddr, HEX);
  Serial.print(" ");
  Serial.print(disc.fileinfo[fileID].execAddr, HEX);
  Serial.print(" ");
  Serial.print(disc.fileinfo[fileID].fileLen, HEX);
  Serial.print(" ");
  Serial.print(disc.fileinfo[fileID].startSector, HEX);
  Serial.println(" ");
  http_get(webhost, webdisc, disc.fileinfo[fileID].startSector*0x100, disc.fileinfo[fileID].fileLen+disc.fileinfo[fileID].startSector*0x100);
  while (!client.available()){}; // wait for data to be returned
  for( int a = 0; a < disc.fileinfo[fileID].fileLen; a = a + 1 ){
    char c = client_read();
    send_byte(c); 
     //Serial.println(a,HEX);
    //  Serial.print(" ");
    //  Serial.println(c,HEX);
  } 
  client_empty();
}

// ==========================================================================
// SECTION: MAIN
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
}  
