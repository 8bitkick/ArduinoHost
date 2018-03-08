// 
// OAuth 2.0 client to access Google APIs
// Scope is acces to ussr Google Drive files this app creates
//
// https://developers.google.com/identity/protocols/OAuth2ForDevices
//
// Incomplete / not recommended for use!

#include <ArduinoJson.h>
#include <SPI.h>
#include <WiFi101.h>
#include <FlashStorage.h>


// WiFi
char ssid[] = SECRET_SSID;   
char pass[] = SECRET_PASS;   


// Google API 
String client_id = SECRET_CLIENT_ID;
String client_secret = SECRET_CLIENT_SECRET;

  
// Keep the tokens in flash when granted
// So we can skip user authorization after reset

typedef struct {
  boolean valid;
  char access[130];
  char refresh[130];
} Tokens;


FlashStorage(flash, Tokens);
Tokens googleapi_tokens;
  
int status = WL_IDLE_STATUS;

WiFiClient client;

// JSON ARDUINO 
const size_t bufferSize = JSON_OBJECT_SIZE(5) + 230;
DynamicJsonBuffer jsonBuffer(bufferSize);
  

// ==========================================================================
// SECTION: Wifi
// ==========================================================================


void setup_wifi(){
   // check for the presence of the shield:
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

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}


// ==========================================================================
// SECTION: Google API access
// ==========================================================================

void setup_auth(){
  // Google APIs authentication
  //--------------------------------------------
  // We're an app requesting access to Google Drive for only the files we create
  // scope = https://www.googleapis.com/auth/drive.file
  
  Serial.println("Authentication required"); 
  
  http_post("accounts.google.com","/o/oauth2/device/code", "application/x-www-form-urlencoded", 
  "client_id=" + client_id + "&scope=https%3A%2F%2Fwww.googleapis.com%2Fauth%2Fdrive.file");
  
  // Parse response
  JsonObject& parsedJSON = jsonBuffer.parseObject(client);

  const char* verification_url = parsedJSON["verification_url"]; 
  int expires_in = parsedJSON["expires_in"]; 
  int interval = parsedJSON["interval"]; 
  const char* device_code = parsedJSON["device_code"]; 
  const char* user_code = parsedJSON["user_code"]; 
  
  // Display user code and request user grants this app permission 
  Serial.println("\n\n*********************");
  Serial.println("To activate go to:\n");
  Serial.println(verification_url);
  Serial.print  ("\ncode: ");
  Serial.println(user_code);
  Serial.println("*********************");

  // Poll to see if they have granted permission
  while (http_post("www.googleapis.com","/oauth2/v4/token", "application/x-www-form-urlencoded", 
  "client_id=" + client_id + 
  "&client_secret=" + client_secret + 
  "&code=" + device_code + 
  "&grant_type=http%3A%2F%2Foauth.net%2Fgrant_type%2Fdevice%2F1.0"))
  {delay(5000);};
  
   // Parse response
  JsonObject& parsedJSON2 = jsonBuffer.parseObject(client);

  String access_token = parsedJSON2["access_token"]; 
  int token_expires_in = parsedJSON2["expires_in"]; 
  String refresh_token = parsedJSON2["refresh_token"]; 
  
  Serial.println("\nAccess granted");
  Serial.println("*********************");
  
   // We now have Google API access to files we create on users Google Drive
  Serial.println(access_token);
  
  http_get("www.googleapis.com","/drive/v3/files",access_token);
  
  // Write tokens to flash memory
  access_token.toCharArray(googleapi_tokens.access, 130);
  refresh_token.toCharArray(googleapi_tokens.refresh, 130);

  googleapi_tokens.valid = true;

  flash.write(googleapi_tokens);

}

/* TODO
void token_refresh(const char* refresh_token){ 
  // We're requesting a token refresh
  http_post("www.googleapis.com","/oauth2/v4/token", "application/x-www-form-urlencoded", 
  "client_id=" + client_id + 
  "&client_secret=" + client_secret + 
  "&code=" + refresh_token + 
  "&refresh_token=" + device_code + 
  "&grant_type=refresh_token");  
    
   // Parse response
  JsonObject& parsedJSON3 = jsonBuffer.parseObject(client);
  const char* access_token = parsedJSON3["access_token"]; 
  int token_expires_in = parsedJSON3["expires_in"]; 
  
  // Write tokens to flash memory
  access_token.toCharArray(googleapi_tokens.access, 130);
  refresh_token.toCharArray(googleapi_tokens.refresh, 130);
  googleapi_tokens.valid = true;
  flash.write(googleapi_tokens);
  
  Serial.println("Token refreshed");
}*/


// POST
int http_post(String host, String path, String contentType, String data) {
  // NB host = server in this example
  client.stop();
  // if there's a successful connection:
  if (client.connectSSL(host.c_str(), 443)) {
    client.println("POST " + path + " HTTP/1.1");
    client.println("Host: "+ host);
    //client.println("Connection: close");
    client.println("Content-Type: "+contentType);
    client.print("Content-Length: ");
    client.println(data.length(),DEC);
    client.println("");
    client.println(data);
    
  // Check response 
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    //Serial.print(F("Unexpected response: "));
    //Serial.println(status);
    return 1;
  }

  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return 1;
  }

  client.readBytesUntil('\n', status, sizeof(status));  
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    return 1;
  }
  return 0;
}

// GET
int http_get(String host, String path, String auth) {
  client.stop();

  // if there's a successful connection:
  if (client.connectSSL(host.c_str(), 443)) {
    client.println("GET " + path + " HTTP/1.1");
    client.println("Host: "+ host);
    client.println("Connection: close");
    client.print  ("Authorization: Bearer ");
    client.println(auth);
    client.println();
    
  // Check response 
  char status[32] = {0};
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    //Serial.print(F("Unexpected response: "));
    //Serial.println(status);
    return 1;
  }

  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return 1;
  }
    
    
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed: ");
    return 1;
  }
  return 0;
}




// ==========================================================================
// SECTION: Set up
// ==========================================================================



void setup() {

  // Serial set-up
  //--------------------------------------------
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // WiFi set-up
  //--------------------------------------------
  setup_wifi();
  
  // Google API access
  // -------------------------------------------
  
  // Read tokens from flash 
  googleapi_tokens = flash.read();
  String access_token =  googleapi_tokens.access;

  if (googleapi_tokens.valid == true){
    Serial.println("Tokens exist in flash, great"); 
    // Check access token still works
     if (http_get("www.googleapis.com","/drive/v3/files", access_token)) {
        Serial.println("Tokens needs refreshing TODO"); // TODO track UTC + expiry 
       // token_refresh(refresh_token);
     } return; // TODO check for success here
  } else setup_auth();
  
}

// ==========================================================================
// SECTION: Loop
// ==========================================================================


void loop() {
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
}


