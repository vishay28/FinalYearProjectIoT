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

// Initialise a variavble to store the current time
long currentTime;
// Initialise a variable to check if any connected device exists
String device;
// Initialise a variable to store the latching time
int latchingTime;

// Initialising a list to store user mac addresses
String userList[11];

// Initialising the UDP server and setting the NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

bool notification = false;

// Creating a secure client for emails
WiFiClientSecure client;

// Creating a webserver on port 80
ESP8266WebServer server(80);

byte key[] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
unsigned long long int myIv = 00000000;

void setup() {

  pinMode(12, INPUT);
  digitalWrite(12, HIGH);
  delay(200);
  pinMode(12, OUTPUT);

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
  if (!SPIFFS.begin()) {
    while(true);
  }
  SPIFFS.remove("/log.txt");
  delay(10);
  Serial.println('\n');

  saveLog("Motion Sensor Booting Up!");

  // Read in the network name, password and connected device from memory if it exisits
  File fileObj = SPIFFS.open("/network.txt", "r");
  String networkName = fileObj.readStringUntil('\n');
  String networkPassword = fileObj.readStringUntil('\n');

  fileObj.close();
  networkName.trim();
  networkPassword.trim();

  // Read in the latching time if saved in the settings file
  fileObj = SPIFFS.open("/settings.txt", "r");
  latchingTime = fileObj.readStringUntil('\n').toInt();
  fileObj.close();
  // If there was no latching time saved then set it to the default of 5 minutes
  if (latchingTime == 0) {
    latchingTime = 300000;
  }
  saveLog("Loaded saved latching time: " + String(latchingTime));
  delay(1000);

  // Determining whether to run in setup mode or connect to an existing network
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/
  if (networkName != "") {
    // Attempt to connect to the home network
    saveLog("Connecting to " + networkName + " with password: " + networkPassword);

    networkPassword = decrypt(networkPassword);
    // Connect to the network
    WiFi.begin(networkName.c_str(), networkPassword.c_str());

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

      // Reading in the first connected device if it exists
      fileObj = SPIFFS.open("/devices.txt", "r");
      device = fileObj.readStringUntil('\n');
      fileObj.close();
      device.trim();

      // Read in the users MAC address which is stored in a file
      fileObj = SPIFFS.open("/users.txt", "r");
      String tempUser;
      // User address for internal communication
      userList[0] = "ZXNwODI2NjphZG1pbg==";
      for (int i = 1; i < 10; i++) {
        tempUser = fileObj.readStringUntil('\n');
        tempUser.trim();
        userList[i] = tempUser;
      }
      fileObj.close();
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

      fileObj = SPIFFS.open("/email.txt", "r");
      for (int i = 0; i<2; i++) {
        fileObj.readStringUntil('\n');
      }
      String fileRead = fileObj.readStringUntil('\n');
      fileRead.trim();
      notification = fileRead == "on" ? true : false;
      fileObj.close();
      saveLog("Notifications are: " + fileRead);

      // Setup the input pin for reading the motion sensor status
      pinMode(14, INPUT);

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);

      // Gets the file where all the IP addresses of connected devices are stored
      server.on("/devices", HTTP_GET, getDevices);

      // Adds a connected device
      server.on("/devices", HTTP_POST, addDevice);

      // Deletes a connected device
      server.on("/devices", HTTP_DELETE, deleteDevice);

      // Gets the latching time
      server.on("/time", HTTP_GET, getTime);

      // Updates the latching time
      server.on("/time", HTTP_PUT, updateTime);

      // Set the email username, password and notification
      server.on("/email", HTTP_PUT, setEmail);

      // Get current notification status
      server.on("/email", HTTP_GET, getNotification);

      // Add a new user
      server.on("/user", HTTP_POST, addUser);

      // Resets all the settings
      server.on("/reset", HTTP_DELETE, resetSettings);

      // Returns the logs file
      server.on("/logs", HTTP_GET, getLogs);

      // Serves a 404 not found for a URI that doesn't exist
      server.onNotFound(handleNotFound);

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
    WiFi.softAP("smartMotionSensor", "");
    saveLog("Starting access point IP: " + (WiFi.softAPIP()).toString());

    // Returns the network input form when a request on the root is called
    server.on("/", HTTP_GET, networkForm);

    // Saves the network details submitted by the form
    server.on("/setup", HTTP_POST, postSetup);

    // Returns the logs file
    server.on("/logs", HTTP_GET, getLogs);

    // Serves a 404 not found for a URI that doesn't exist
    server.onNotFound(handleNotFound);

    // Starts the server
    server.begin();
    saveLog("Started the server");
  }
  delay(5000);
}
/* SETUP MODE-------------------------------------------------------------------------------------------------------------------------*/


/* RESET FUNCTION---------------------------------------------------------------------------------------------------------------------*/
void(* resetFunc) (void) = 0;
/* RESET FUNCTION---------------------------------------------------------------------------------------------------------------------*/


/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/
void loop(void){
  if (digitalRead(14) && notification) {
    saveLog("Motion Detected & Sending Email");
    sendEmail();
    delay(2000);
  }
  // Checks if a device is connected
  if (device != "Null" && device != "") {
    // Checks the current state
    if (digitalRead(14)) {
      saveLog("Motion Detected");
      // Switch the connected device
      sendSwitch(digitalRead(14));
      // Save the current time
      currentTime = millis();
      // Start a timer for 5 minutes
      while ((millis()-currentTime) < latchingTime) {
        // If motion is detected reset the timer
        if (digitalRead(14)) {
          currentTime = millis();
        }
        server.handleClient();
      }
    } else {
      sendSwitch(digitalRead(14));
      currentTime = millis();
      // Start the timer but end it if motion is detected
      while (((millis()-currentTime) < latchingTime) && !digitalRead(14)) {
        server.handleClient();
      }
    }
  }
  // Listen for requests
  server.handleClient();
  timeClient.update();
}
/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/
// Returns the form for the user to set up the device through a browser
void networkForm(){
  saveLog("HTTP " + String(server.uri()));
  server.send(200, "text/html", "<form action=\"/setup\" method=\"POST\" enctype=\"multipart/form-data\"> SSID: <input type=\"text\" name=\"ssid\"><br> Password: <input type=\"text\" name=\"password\"><br><input type=\"submit\" value=\"Submit\">");
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

  digitalWrite(0, HIGH);
  digitalWrite(2, HIGH);
  delay(5000);
  digitalWrite(12, LOW);
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
    server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartMotion\"}");
    delay(100);
    // Disconnects from the phone
    WiFi.softAPdisconnect(true);
    saveLog("Turning off the access point");
  } else {
    WiFi.softAPdisconnect(true);
    server.send (403);
  }
}

// Function to return the file containing the IP addresses of the connected devices
void getDevices() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Opens the file to read it
    File deviceFile = SPIFFS.open("/devices.txt", "r");
    // Sends the file
    server.streamFile(deviceFile, "text/plain");
    deviceFile.close();
  } else {
    server.send (403);
  }
}

// Function to add a connected device
void addDevice(){
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Opens the device file in append mode
    File deviceFile = SPIFFS.open("/devices.txt", "a");
    // Save the added device
    deviceFile.println(server.arg(0));
    deviceFile.close();
    device = server.arg(0);

    saveLog("Device added: " + server.arg(0));
    server.send(204);
  } else {
    server.send (403);
  }
}

//Function to delete a connected device
void deleteDevice() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Initialise a variable to store a row from the file
    String deviceRow;
    // Open the current device file and create a new one
    File deviceFile = SPIFFS.open("/devices.txt", "r");
    File deviceFileNew = SPIFFS.open("/devicesNew.txt", "w");

    saveLog("Deleting device: " + server.arg(0));
    // Loop through all the rows in the current device file
    while (deviceFile.available()) {
      // Read the row
      deviceRow = deviceFile.readStringUntil('\n');
      deviceRow.trim();
      // If the row is not the same as the device to be deleted then copy it to the new file
      if (deviceRow != server.arg(0)) {
        deviceFileNew.println(deviceRow);
      }
    }
    // Close the files
    deviceFile.close();
    deviceFileNew.close();
    // Remove the old file and rename the new file
    SPIFFS.remove("/devices.txt");
    SPIFFS.rename("/devicesNew.txt", "/devices.txt");
    // Read in the first connected device to check whether there are still any connected devices
    deviceFile = SPIFFS.open("/devices.txt", "r");
    device = deviceFile.readStringUntil('\n');
    deviceFile.close();
    server.send(204);
  } else {
    server.send (403);
  }
}

// Function to update the latching time
void updateTime(){
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Open the setting file
    File settingsFile = SPIFFS.open("/settings.txt", "w");
    // Save the new latching time in milliseconds
    settingsFile.println(server.arg(0).toInt() * 1000);
    settingsFile.close();
    // Update the global latching time value
    latchingTime = server.arg(0).toInt() * 1000;
    saveLog("Latching time updated to: " + String(latchingTime));
    server.send(204);
  } else {
    server.send (403);
  }
}

// Function to return the latching time
void getTime() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Sends the latching time in seconds
    server.send(200, "text/plain", "{\"time\":"+String(latchingTime/1000)+"}");
  } else {
    server.send (403);
  }
}

void getNotification() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    server.send(200, "text/plain", "{\"notification\":" + String(notification) + "}");
  } else {
    server.send (403);
  }
}

void setEmail() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    File emailFile = SPIFFS.open("/email.txt", "w");
    
    emailFile.println(encrypt(server.arg(0)));
    emailFile.println(encrypt(server.arg(1)));

    emailFile.println(server.arg(2) == "true" ? "on" : "off");
    emailFile.close();

    notification = server.arg(2) == "true" ? true : false;
    saveLog("Notifications set to: "+server.arg(2));
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
    // Format the files saved
    saveLog("Resetting all settings");
    SPIFFS.format();
    server.send(200);
    delay(100);
    digitalWrite(12, LOW);
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
    value = "false";
  }
  else {
    value = "true";
  }
  
  File deviceFile = SPIFFS.open("/devices.txt", "r");
  String deviceRow;
  while (deviceFile.available()) {
    deviceRow = deviceFile.readStringUntil('\n');
    deviceRow.trim();
    saveLog("Sending Status: "+value + " to device: " + deviceRow);
    // Setup the HTTP request
    HTTPClient http;
    // Sets the URL
    http.begin("http://" + deviceRow + "/switch");
    // Sets the content type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", "Basic " + userList[0]);
    // Sends the PUT request
    int responseCode = http.PUT("Status="+value);
    http.end();
    saveLog("Received response: "+String(responseCode));
  }
}
/* SWITCH FUNCTIONS----------------------------------------------------------------------------------------------------------------*/


/* AUTHENTICATION FUNCTIONS--------------------------------------------------------------------------------------------------------*/
// Function to check whether the user submitting the request is registered
bool authenticate(String userClient) {
  // Gets just the value of the hashed MAC address from the authentication header
  userClient = userClient.substring(userClient.indexOf(" ")+1);
  // Checks the MAC address against the allowed users
  for (int i = 0; i < 10; i++) {
    if (userClient == userList[i]) {
      return true;
    }
  }
  saveLog("Authentication failed");
  return false;
}
/* AUTHENTICATION FUNCTIONS--------------------------------------------------------------------------------------------------------*/


/* EMAIL FUNCTIONS-----------------------------------------------------------------------------------------------------------------*/
int sendEmail() {
  saveLog("Attempting to connect to gmail server");
  if (client.connect("smtp.gmail.com", 465) == 1) {
    saveLog("Connected");
  } else {
    saveLog("Connection failed:");
    return 0;
  }
  if (!response()) {
    return 0;
  }

  saveLog("Sending Extended Hello");
  client.println("EHLO gmail.com");
  if (!response()) {
    return 0;
  }

  saveLog("Sending auth login");
  client.println("auth login");
  if (!response()) {
    return 0;
  }

  saveLog("Sending User");
  File emailFile = SPIFFS.open("/email.txt", "r");
  String email = emailFile.readStringUntil('\n');
  email.trim();
  email = decrypt(email);
  client.println(email);
  if (!response())
    return 0;

  email = b64decode(email);

  saveLog("Sending Password");
  String password = emailFile.readStringUntil('\n');
  password.trim();
  emailFile.close();
  client.println(decrypt(password));
  if (!response())
    return 0;

  saveLog("Sending From");
  // your email address (sender) - MUST include angle brackets
  client.println("MAIL FROM: <" + email + ">");
  if (!response())
    return 0;

  // change to recipient address - MUST include angle brackets
  saveLog("Sending To");
  client.println("RCPT To: <" + email + ">");
  // Repeat above line for EACH recipient
  if (!response())
    return 0;

  saveLog("Sending DATA");
  client.println("DATA");
  if (!response())
    return 0;

  saveLog("Sending email");
  // recipient address (include option display name if you want)
  client.println("To: <" + email + ">");

  // change to your address
  String date = timeClient.getFormattedDate();
  client.println("From: " + email);
  client.println("Subject: Motion Alert\r\n");
  client.println("Motion was detected at: " + date.substring(0, date.indexOf("T")) + " " + timeClient.getFormattedTime());
  client.println(".");

  if (!response())
    return 0;
    client.println("QUIT");
    if (!response())
      return 0;

    client.stop();
    saveLog("Disconnected");
    return 1;
}

// Check response from SMTP server
int response()
{
  // Wait for a response for up to X seconds
  int loopCount = 0;
  while (!client.available()) {
    delay(1);
    loopCount++;
    // if nothing received for 10 seconds, timeout
    if (loopCount > 10000) {
      client.stop();
      Serial.println("\r\nTimeout");
      return 0;
    }
  }

  // Take a snapshot of the response code
  int respCode = client.peek();
  while (client.available())
  {
    Serial.write(client.read());
  }

  if (respCode >= '4')
  {
    Serial.print("Failed in eRcv with response: ");
    Serial.print(respCode);
    return 0;
  }
  return 1;
}
/* EMAIL FUNCTIONS-----------------------------------------------------------------------------------------------------------------*/

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
