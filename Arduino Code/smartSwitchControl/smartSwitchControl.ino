// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// File access library
#include <FS.h>
// HTTP Client library
#include <ESP8266HTTPClient.h>
// NTP client for getting the time
#include <NTPClient.h>
// UDP library for connecting to the NTP Client
#include <WiFiUdp.h>
// Time and sys time libraries for setting the system time
#include <time.h>
#include <sys/time.h>
// Base64 encryption and decryption library
#include "base64.h"
#include <AES.h>
AES aes ;

// Initialise the switch value variable
int inputValue;

String userList[11];

// Initialising the UDP server and setting the NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

// Creating a webserver on port 80
ESP8266WebServer server(80);

byte key[] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
unsigned long long int myIv = 00000000;

void setup() {
  
  // Setup LED Indicators
  // Pin 0 is the Red LED and pin 2 is the Blue LED
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);

  // Set both LEDs to off (HIGH means off)
  digitalWrite(0, HIGH);
  digitalWrite(2, HIGH);
  
  // Starting the serial connection
  Serial.begin(115200);
  // Mount the file system
  SPIFFS.begin();
  SPIFFS.remove("/log.txt");
  delay(10);
  Serial.println('\n');

  saveLog("Smart Switch Booting Up!");
  
  // Read in the network name, password and connected device from memory if it exisits
  File networkFile = SPIFFS.open("/network.txt", "r");
  String networkName = networkFile.readStringUntil('\n');
  String networkPassword = networkFile.readStringUntil('\n');
  networkName.trim();
  networkPassword.trim();
  delay(1000);

  networkFile.close();

  // Determining whether to run in setup mode or connect to an existing network
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/
  if (networkName != "") {
    
    saveLog("Connecting to " + networkName + " with password: " + networkPassword);

    networkPassword = decrypt(networkPassword);
    // Connect to the network
    WiFi.begin(networkName, networkPassword);

    // Waits to connect. If no connection is determined within 20 seconds a timeout occurs
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 20) {
      digitalWrite(0, LOW);
      delay(500);
      digitalWrite(0, HIGH);
      delay(500);
      i++;
    }
    // Determining if the WiFi connection was successfull
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(0, LOW);
      saveLog("Connecting to WiFi timed out!");
      saveLog("Entering setup mode");
      //Setting the network name to null so that the setup mode runs
      networkName = "";
    }
    else {
      digitalWrite(2, LOW);
      saveLog("Connection established!");
      saveLog("IP address: " + (WiFi.localIP()).toString());

      File usersFile = SPIFFS.open("/users.txt", "r");
      String tempUser;
      userList[0] = "ZXNwODI2NjphZG1pbg==";
      for (int i = 1; i < 10; i++) {
        tempUser = usersFile.readStringUntil('\n');
        tempUser.trim();
        userList[i] = tempUser;
      }
      usersFile.close();
      saveLog("Loaded registered users");

      //Starting the time client and getting the current time
      saveLog("Starting time client");
      timeClient.begin();
      timeClient.update();

      // Getting the current time in epoch
      timeval epoch = {timeClient.getEpochTime(), 0};
      timezone tz = {0, 0};
      // Setting the system time to the current time
      settimeofday(&epoch, &tz);
      saveLog("Set the system time");

      // Setup the input pin for reading the switch status
      pinMode(14, INPUT_PULLUP);
      inputValue = digitalRead(14);

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);
    
      // Adds a connected device
      server.on("/devices", HTTP_POST, addDevice);

      // Gets the IP address of the device that the switch is connected to
      server.on("/devices", HTTP_GET, getDevices);

      // Deletes a connected device
      server.on("/devices", HTTP_DELETE, deleteDevice);

      // Returns the logs file
      server.on("/logs", HTTP_GET, getLogs);

      // Resets all the settings
      server.on("/reset", HTTP_DELETE, resetSettings);
    
      // Starts the server
      server.begin();
      saveLog("Started the server");
    }
  }
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP MODE-------------------------------------------------------------------------------------------------------------------------*/
  if (networkName == "") {

    digitalWrite(0, LOW);
    
    // Starting up the access point with the provided ssid and password
    WiFi.softAP("smartSwitch", "");
    saveLog("Starting access point IP: " + (WiFi.softAPIP()).toString());
  
    // Returns the network input form when a request on the root is called
    server.on("/", HTTP_GET, handleRoot);
  
    // Saves the network details submitted by the form
    server.on("/setup", HTTP_POST, postSetup);

    // Add a new user
    server.on("/user", HTTP_POST, addUser);

    // Returns the logs file
    server.on("/logs", HTTP_GET, getLogs);
  
    // Serves a 404 not found for a URI that doesn't exist
    server.onNotFound(handleNotFound);
  
    // Starts the server
    server.begin();
    saveLog("Started the server");
  }
}
/* SETUP MODE-------------------------------------------------------------------------------------------------------------------------*/


/* RESET FUNCTION---------------------------------------------------------------------------------------------------------------------*/
void(* resetFunc) (void) = 0;
/* RESET FUNCTION---------------------------------------------------------------------------------------------------------------------*/


/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/
void loop(void){
  server.handleClient();
  timeClient.update();
  // Checks if the switch has been toggled
  if (digitalRead(14) != inputValue) {
    // Saves the new state of the switch
    inputValue = digitalRead(14);
    saveLog("Switch status updated to: " + String(!inputValue));
    // Calls the function to toggle the connected device
    sendSwitch(inputValue);
  }
}
/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/
// Returns the form for the user to set up the device through a browser
void handleRoot(){
  saveLog("HTTP " + String(server.uri()));
  server.send(200);
}

// Saves the network details that were submitted by the user
void postSetup(){
  saveLog("HTTP " + String(server.uri()));
  // Opens the network file in write mode
  File networkFile = SPIFFS.open("/network.txt", "w");
  File usersFile = SPIFFS.open("/users.txt", "a");

  // Saves the SSID and password
  networkFile.println(server.arg(0));
  networkFile.println(encrypt(b64decode(server.arg(1))));

  // Gets the hashed version of the mac address of the device
  String user = server.arg(2);
  user.trim();
  // Saves the hashed version of the MAC address of the device
  usersFile.println(user.substring(user.indexOf(" ")+1));

  // Close the files
  networkFile.close();
  usersFile.close();
  saveLog("Saved the WiFi details and user");

  // Retruns a 201 once the details have been saved
  server.send(201);

  delay(5000);
  resetFunc();
}

void handleNotFound(){
  saveLog("HTTP " + String(server.uri()));
  server.send(404, "text/plain", "404: Not found");
}
/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/

/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/
// Function to return the IP address of the arduino on the home network
void sendIp() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Sends the IP address along with the device type
    server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartSwitch\"}");
    delay(100);
    // Disconnects from the phone
    WiFi.softAPdisconnect(true);
    saveLog("Turning off the access point");
  } else {
    WiFi.softAPdisconnect(true);
    server.send (403);
  }
}

// Function to return the IP addresses of the devices that is connected to the switch
void getDevices() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    File deviceFile = SPIFFS.open("/devices.txt", "r");
    server.streamFile(deviceFile, "text/plain");
    deviceFile.close();
  } else {
    server.send (403);
  }
}

// Function to update the device connected to the switch
void addDevice(){
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    File deviceFile = SPIFFS.open("/devices.txt", "a");
    deviceFile.println(server.arg(0));
  
    deviceFile.close();
    
    saveLog("Adding device: " + server.arg(0));
    server.send(204);
  } else {
    server.send (403);
  }
}

//Function to delete a device connected to the switch
void deleteDevice() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    String device;
    File deviceFile = SPIFFS.open("/devices.txt", "r");
    File deviceFileNew = SPIFFS.open("/devicesNew.txt", "w");
  
    saveLog("Deleting device: " + server.arg(0));
  
    while (deviceFile.available()) {
      device = deviceFile.readStringUntil('\n');
      device.trim();
      
      if (device != server.arg(0)) {
        deviceFileNew.println(device);
      }
    }
    
    deviceFile.close();
    deviceFileNew.close();
  
    SPIFFS.remove("/devices.txt");
    SPIFFS.rename("/devicesNew.txt", "/devices.txt");
  
    server.send(204);
  } else {
    server.send (403);
  }
}

void addUser() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    int i = 0;
    while (i < sizeof(userList)) {
      if (userList[i] == "") {
        break;
      }
      i++;
    }
    userList[i] = server.arg(0);
    File userFile = SPIFFS.open("/users.txt", "a");
    userFile.println(server.arg(0));
    userFile.close();
    server.send(201);
  } else {
    server.send (403);
  }
}

// Function to reset the device
void resetSettings(){
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    saveLog("Resetting all settings");
    // Format the files saved
    SPIFFS.format();
    server.send(200);
    delay(100);
    resetFunc();
  } else {
    server.send (403);
  }
}
/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/

/* SWITCH FUNCTIONS----------------------------------------------------------------------------------------------------------------*/
// Function to toggle the connected device
void sendSwitch(int input) {
  String value;
  // Converts the integer value to a boolean value
  if (input == 0){
    value = "true";
  }
  else {
    value = "false";
  }
  File deviceFile = SPIFFS.open("/devices.txt", "r");
  String device;
  while (deviceFile.available()) {
    device = deviceFile.readStringUntil('\n');
    device.trim();
    saveLog("Sending Status: "+value + " to device: " + device);
  
    // Setup the HTTP request
    HTTPClient http;
    // Sets the URL
    http.begin("http://" + device + "/switch");
    // Sets the content type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", "Basic " + userList[0]);
    // Sends the PUT request
    int responseCode = http.PUT("Status="+value);
    http.end();
    saveLog("Received response: " + String(responseCode));
  }
}
/* SWITCH FUNCTIONS----------------------------------------------------------------------------------------------------------------*/

/* AUTHENTICATION FUNCTION---------------------------------------------------------------------------------------------------------*/
bool authenticate(String userClient) {
  userClient = userClient.substring(userClient.indexOf(" ")+1);
  for (int i = 0; i < 10; i++) {
    if (userClient == userList[i]) {
      return true;
    }
  }
  saveLog("Authentication failed");
  return false;
}
/* AUTHENTICATION FUNCTION---------------------------------------------------------------------------------------------------------*/

/* ENCRYPTION FUNCTIONS------------------------------------------------------------------------------------------------------------*/
String encrypt(String input) {

  byte iv [N_BLOCK];
  byte encrypted[50];
  char b64Output[50];
  String output;

  aes.set_IV(myIv);
  aes.get_IV(iv);

  input.trim();
  aes.do_aes_encrypt((byte *)input.c_str(), input.length(), encrypted, key, 128, iv);
  
  int b64len = b64_encode((char *)b64Output, (char *)encrypted, aes.get_size());
  for (int i = 0; i < b64len; i++) {
    output.concat(b64Output[i]);
  }
  return output;
}

String decrypt(String input) {

  byte iv [N_BLOCK];
  byte decrypted[50];
  byte b64Output[50];
  int b64len;
  String output;

  b64len = b64_decode((char *)b64Output, (char *)input.c_str(), input.length());

  aes.set_IV(myIv);
  aes.get_IV(iv);

  input.trim();
  aes.do_aes_decrypt(b64Output, b64len, decrypted, key, 128, iv);
  int i = 0;
  while (decrypted[i] != 4 && decrypted[i] != 16 && decrypted[i] != 8) {
    output.concat(char(decrypted[i]));
    i++;
  }
  return output;
}

String b64encode(String input) {

  byte b64Output[50];
  String output;
  
  int b64len = b64_encode((char *)b64Output, (char *)input.c_str(), input.length());
  for (int i = 0; i < b64len; i++) {
    output.concat(char(b64Output[i]));
  }
  return output;
}

String b64decode(String input) {

  byte b64Output[50];
  String output;

  int b64len = b64_decode((char *)b64Output, (char *)input.c_str(), input.length());
  for (int i = 0; i < b64len; i++) {
    output.concat(char(b64Output[i]));
  }
  return output;
}
/* ENCRYPTION FUNCTIONS------------------------------------------------------------------------------------------------------------*/

/* LOGS FUNCTION----------------------------------------------------------------------------------------------------------------*/
void saveLog(String message) {
  File logFile = SPIFFS.open("/log.txt", "a");
  String date = timeClient.getFormattedDate();
  Serial.print(date.substring(0, date.indexOf("T")));
  logFile.print(date.substring(0, date.indexOf("T")));
  Serial.print(" ");
  logFile.print(" ");
  Serial.print(timeClient.getFormattedTime());
  logFile.print(timeClient.getFormattedTime());
  Serial.print(" | " + message);
  logFile.print(" | " + message);
  Serial.println();
  logFile.println();
  logFile.close();
}

void getLogs() {
  File logFile = SPIFFS.open("/log.txt", "r");
  server.streamFile(logFile, "text/plain");
  logFile.close();
}
/* LOGS FUNCTION----------------------------------------------------------------------------------------------------------------*/
