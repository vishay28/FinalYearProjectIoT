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
// Cron library for running the schedules
#include "CronAlarms.h"
// Time and sys time libraries for setting the system time
#include <time.h>
#include <sys/time.h>
// Base64 encryption and decryption library
#include "base64.h"
#include <AES.h>
AES aes ;

//Initialise variable to store the current status
bool plugStatus = false;

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

  saveLog("Smart Plug Booting Up");

  // Read in the network name and password from memory if it exisits
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
      timeClient.begin();
      timeClient.update();

      // Getting the current time in epoch
      timeval epoch = {timeClient.getEpochTime(), 0};
      timezone tz = {0, 0};
      // Setting the system time to the current time
      settimeofday(&epoch, &tz);
      saveLog("Set the system time");

      // Calling the function to load the saved schedules
      loadSchedules();
      saveLog("Loaded in saved schedules");

      // Sets up the output pins which set and unset the relay
      pinMode(12, OUTPUT);
      pinMode(13, OUTPUT);

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);
    
      // Toggles the plug on or off
      server.on("/switch", HTTP_PUT, switchStatus);

      // Gets the status of the plug
      server.on("/switch", HTTP_GET, getStatus);

      // Add a new schedule
      server.on("/schedule", HTTP_POST, addSchedule);

      // Get a schedule
      server.on("/schedule", HTTP_GET, getSchedule);

      // Delete a schedule
      server.on("/schedule", HTTP_DELETE, deleteSchedule);

      // Add a new user
      server.on("/user", HTTP_POST, addUser);

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
    WiFi.softAP("smartPlug", "");
  
    saveLog("Starting access point IP: " + (WiFi.softAPIP()).toString());
  
    // Returns the network input form when a request on the root is called
    server.on("/", HTTP_GET, handleRoot);
  
    // Saves the network details submitted by the form
    server.on("/setup", HTTP_POST, postSetup);
  
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
  // Check for any cron tasks
  Cron.delay();
  timeClient.update();
  server.handleClient();
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
    server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartPlug\"}");
    delay(100);
    // Disconnects from the phone
    WiFi.softAPdisconnect(true);
    saveLog("Turning off the access point");
  } else {
    WiFi.softAPdisconnect(true);
    server.send (403);
  }
}

// Function to toggle the plug on and off
void switchStatus(){
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    String statusVal;
    statusVal = server.arg(0);
    if (statusVal == "true") {
      turnOn();
    }
    else {
      turnOff();
    }
    saveLog("Switching plug to: "+statusVal);
    server.send(204);
  } else {
    server.send (403);
  }
}

// Funtion to turn on the plug
void turnOn() {
  plugStatus = true;
  digitalWrite(12, HIGH);
  delay(10);
  digitalWrite(12, LOW);
}

// Function to turn off the plug
void turnOff() {
  plugStatus = false;
  digitalWrite(13, HIGH);
  delay(10);
  digitalWrite(13, LOW);
}

// Function to return the current status of the plug
void getStatus() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    server.send(200, "plain/text", "{\"status\":"+String(plugStatus)+"}");
  } else {
    server.send (403);
  }
}

// Function to add a schedule
void addSchedule() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Initialise a variable to store the id of the cron job being added
    CronId id;
    // Set a variable with the specified timings
    String timings = server.arg(1);
    // Determine if the function is to turn on or off
    if (server.arg(2) == "turnOn") {
      // For specifying the timings in cron you have to give the address of the first character in a string literal
      // Create the cron job and return the id
      id = Cron.create(&timings[0], turnOn, false);
    } else {
      id = Cron.create(&timings[0], turnOff, false);
    }

    // Append the new cron job to the cron file
    File cronFile = SPIFFS.open("/cron.txt", "a");
    // The format that this is saved is "name, sec min hr day mon day, function, id"
    cronFile.print(server.arg(0) + "," + server.arg(1) + "," + server.arg(2) + ",");
    // Making the id two digits before saving it
    cronFile.println(id < 10 ? "0"+String(id) : String(id));
    cronFile.close();
    saveLog("Creating a new schedule: " + timings + " " + server.arg(2));
    server.send(201);
  } else {
    server.send (403);
  }
}

// Function to get the details of a specific cron job
void getSchedule() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Open the cron file
    File cronFile = SPIFFS.open("/cron.txt", "r");

    // Initialise a varaible to store the read row
    String cronRow;
    // Read the cron file until the cron job is found
    while (cronFile.available()) {
      cronRow = cronFile.readStringUntil('\n');
      cronRow.trim();
      // If the cron job is found then break from the while loop
      if (cronRow.startsWith(server.arg(0),0)) {
        break;
      }
    }
    cronFile.close();
    // Send the job details back to the app
    server.send(200, "text/plain", "{\"row\":\""+cronRow+"\"}");
    saveLog("Sending schedule: " + cronRow);
  } else {
    server.send (403);
  }
}

// Function to delete a schedule
void deleteSchedule() {
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Initialising a variable to store a row read from the cron file
    String cronRow;
    saveLog("Deleting schedule: " + server.arg(0));
    // Opening the cron file and creating a new file in which the jobs that aren't being deleted will be copied to
    File cronFile = SPIFFS.open("/cron.txt", "r");
    File cronFileNew = SPIFFS.open("/cronNew.txt", "w");

    // Read every row in the cron file
    while (cronFile.available()) {
      cronRow = cronFile.readStringUntil('\n');
      cronRow.trim();

      // If the row isn't the job that needs to be deleted then copy it to a new file
      if (!cronRow.startsWith(server.arg(0), 0)) {
        cronFileNew.println(cronRow);
      }
    }
    // Delete the actual cron job and free up memory
    Cron.free((cronRow.substring(cronRow.length() - 2)).toInt());

    cronFile.close();
    cronFileNew.close();

    // Delete the old file and replace it with the new file
    SPIFFS.remove("/cron.txt");
    SPIFFS.rename("/cronNew.txt", "/cron.txt");

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

// Function to reset the settings
void resetSettings(){
  saveLog("HTTP " + String(server.uri()));
  if (server.hasHeader("Authorization") && authenticate(server.header("Authorization"))){
    // Format the files saved
    Serial.println("Reset the device");
    SPIFFS.format();
    server.send(200);
    delay(100);
    resetFunc();
  } else {
    server.send (403);
  }
}
/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/

/* SCHEDULE FUNCTION---------------------------------------------------------------------------------------------------------------*/
// Function to load the saved cron jobs saved in memory upon startup
void loadSchedules() {
  // Open the cron file and create a new cron file to store the jobs with the updated IDs
  File cronFile = SPIFFS.open("/cron.txt", "r");
  File cronFileNew = SPIFFS.open("/cronNew.txt", "w");

  // Initialise variables to store the row read from the file and the timings and id
  String cronRow;
  String timings;
  CronId id;

  // For each row in the original cron file, create a new cron job and save the details as well as the new id in the new file
  while (cronFile.available()) {
    cronRow = cronFile.readStringUntil('\n');
    // Extract the timings for the given cron job
    timings = cronRow.substring(cronRow.indexOf(",")+1, cronRow.indexOf(",",cronRow.indexOf(",")+1));
    // Check the function of the given cron job and create the relevant cron job
    if (cronRow.substring(cronRow.indexOf(",",cronRow.indexOf(",")+1)+1, cronRow.indexOf(",", cronRow.indexOf(",",cronRow.indexOf(",")+1)+1)) == "turnOn") {
      id = Cron.create(&timings[0], turnOn, false);
    } else {
      id = Cron.create(&timings[0], turnOff, false);
    }
    // Remove the old ID of the cron job
    cronRow.remove(cronRow.lastIndexOf(",")+1,2);
    // Append the new id
    if (id < 10) {
      cronRow += ("0"+String(id));
    } else {
      cronRow += String("id");
    }
    // Save the details of the cron job in the new file
    cronFileNew.println(cronRow);
    saveLog("Loading in schedule: "+cronRow);
  }
  cronFile.close();
  cronFileNew.close();

  // Replace the old file with the new file
  SPIFFS.remove("/cron.txt");
  SPIFFS.rename("/cronNew.txt", "/cron.txt");
}
/* SCHEDULE FUNCTION---------------------------------------------------------------------------------------------------------------*/

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
