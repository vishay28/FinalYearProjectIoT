// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// File access library
#include <FS.h>
// HTTP Client library
#include <ESP8266HTTPClient.h>

// Initialise a variavble to store the current time
long currentTime;
// Initialise a variable to check if any connected device exists
String device;
// Initialise a variable to store the latching time
int latchingTime;

// Creating a webserver on port 80
ESP8266WebServer server(80);

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
  if (!SPIFFS.begin()) {
    while(true);
  }
  delay(10);
  Serial.println('\n');

  // Read in the network name, password and connected device from memory if it exisits
  File networkFile = SPIFFS.open("/network.txt", "r");
  String networkName = networkFile.readStringUntil('\n');
  String networkPassword = networkFile.readStringUntil('\n');
  networkFile.close();
  networkName.trim();
  networkPassword.trim();
  File settingsFile = SPIFFS.open("/settings.txt", "r");
  latchingTime = settingsFile.readStringUntil('\n').toInt();
  settingsFile.close();
  if (latchingTime == 0) {
    latchingTime = 300000;
  }
  Serial.println("Starting latching time: " + String(latchingTime));
  delay(1000);

  // Determining whether to run in setup mode or connect to an existing network
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/
  if (networkName != "") {
    // Attempt to connect to the home network
    Serial.print("Connecting to: ");
    Serial.print(networkName);
    Serial.print("\nWith password: ");
    Serial.print(networkPassword);
    Serial.print("\n");
    WiFi.begin(networkName, networkPassword);

    // Waits to connect. If no connection is determined within 20 seconds a timeout occurs
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 20) {
      digitalWrite(0, LOW);
      delay(500);
      digitalWrite(0, HIGH);
      delay(500);
      Serial.print(++i); Serial.print(' ');
    }
    // Determining if the WiFi connection was successfull
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(0, LOW);
      Serial.println("Connecting to WiFi timed out!");
      Serial.println("Entering setup mode");
      //Setting the network name to null so that the setup mode runs
      networkName = "";
    }
    else {
      digitalWrite(2, LOW);
      Serial.println("Connection established!");  
      Serial.print("IP address: ");
      Serial.print(WiFi.localIP());
      Serial.print("\n");

      // Setup the input pin for reading the switch status
      pinMode(14, INPUT);

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);

      // Gets the IP address of the device that the switch is connected to
      server.on("/devices", HTTP_GET, getDevices);
    
      // Updates the device that the switch is connected to
      server.on("/devices", HTTP_PUT, addDevice);

      // Deletes a connected device
      server.on("/deleteDevice", HTTP_PUT, deleteDevice);

      // Gets the latching time
      server.on("/time", HTTP_GET, getTime);

      // Updates the latching time
      server.on("/time", HTTP_PUT, updateTime);

      // Resets all the settings
      server.on("/reset", HTTP_DELETE, resetSettings);
    
      // Starts the server
      server.begin();
    }
  }
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP MODE-------------------------------------------------------------------------------------------------------------------------*/
  if (networkName == "") {

    digitalWrite(0, LOW);
    
    // Starting up the access point with the provided ssid and password
    WiFi.softAP("smartMotionSensor", "");
  
    // Print out the access point details
    Serial.print("Access Point Started");
    Serial.print("\n");
  
    Serial.print("IP address: ");
    // Gets the IP address of the board
    Serial.print(WiFi.softAPIP());
    Serial.print("\n");
  
    // Returns the network input form when a request on the root is called
    server.on("/", HTTP_GET, networkForm);
  
    // Saves the network details submitted by the form
    server.on("/setup", HTTP_POST, postSetup);
  
    // Serves a 404 not found for a URI that doesn't exist
    server.onNotFound(handleNotFound);
  
    // Starts the server
    server.begin();
    Serial.println("HTTP server started");
  }
}
/* SETUP MODE-------------------------------------------------------------------------------------------------------------------------*/


/* RESET FUNCTION---------------------------------------------------------------------------------------------------------------------*/
void(* resetFunc) (void) = 0;
/* RESET FUNCTION---------------------------------------------------------------------------------------------------------------------*/


/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/
void loop(void){

  File deviceFile = SPIFFS.open("/devices.txt", "r");
  device = deviceFile.readStringUntil('\n');
  deviceFile.close();
  device.trim();
  if (device != "Null" && device != "") {
    // Checks the current state
    if (digitalRead(14)) {
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
  server.handleClient();
}
/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/
// Returns the form for the user to set up the device through a browser
void networkForm(){
  server.send(200, "text/html", "<form action=\"/setup\" method=\"POST\" enctype=\"multipart/form-data\"> SSID: <input type=\"text\" name=\"ssid\"><br> Password: <input type=\"text\" name=\"password\"><br><input type=\"submit\" value=\"Submit\">");
}

// Saves the network details that were submitted by the user
void postSetup(){
  // Opens the network file in write mode
  File networkFile = SPIFFS.open("/network.txt", "w");

  // Saves the SSID and password
  networkFile.println(server.arg(0));
  networkFile.println(server.arg(1));

  // Close the file
  networkFile.close();

  // Retruns a 201 once the details have been saved
  server.send(201, "text/plain", "201: Saved WiFi details, please connect to your main network");

  delay(5000);
  resetFunc();
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
}
/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/

/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/
// Function to return the IP address of the arduino on the home network
void sendIp() {
  // Sends the IP address along with the device type
  server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartMotion\"}");
  delay(100);
  // Disconnects from the phone
  WiFi.softAPdisconnect(true);
}

// Function to return the IP address of the device that is connected to the switch
void getDevices() {
  File deviceFile = SPIFFS.open("/devices.txt", "r");
  server.streamFile(deviceFile, "text/plain");
  deviceFile.close();
}

// Function to update the device connected to the switch
void addDevice(){
  File deviceFile = SPIFFS.open("/devices.txt", "a");
  deviceFile.println(server.arg(0));

  deviceFile.close();
  
  Serial.println("Device Saved: " + server.arg(0));
  server.send(204);
}

//Function to delete a device connected to the switch
void deleteDevice() {
  String device;
  File deviceFile = SPIFFS.open("/devices.txt", "r");
  File deviceFileNew = SPIFFS.open("/devicesNew.txt", "w");

  Serial.println("Deleting: " + server.arg(0));

  while (deviceFile.available()) {
    device = deviceFile.readStringUntil('\n');
    device.trim();
    
    if (device != server.arg(0)) {
      Serial.println("Copying: " + device);
      deviceFileNew.println(device);
    }
  }
  
  deviceFile.close();
  deviceFileNew.close();

  SPIFFS.remove("/devices.txt");
  SPIFFS.rename("/devicesNew.txt", "/devices.txt");

  server.send(204);
}

// Function to update the latching time
void updateTime(){
  File settingsFile = SPIFFS.open("/settings.txt", "w");
  settingsFile.println(server.arg(0).toInt() * 1000);
  settingsFile.close();
  latchingTime = server.arg(0).toInt() * 1000;
  Serial.println("Latching time updated: " + String(latchingTime));
  server.send(204);
}

// Function to return the latching time
void getTime() {
  if (latchingTime > 0) {
    Serial.println("Sending latching time: " + String(latchingTime/1000));
   server.send(200, "text/plain", "{\"time\":"+String(latchingTime/1000)+"}");
  } else {
    server.send(200, "text/plain", "{\"time\": \"0\"}");
  }
}

// Function to reset the device
void resetSettings(){
  // Format the files saved
  SPIFFS.format();
  server.send(200);
  delay(100);
  resetFunc();
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
  String device;
  while (deviceFile.available()) {
    device = deviceFile.readStringUntil('\n');
    device.trim();
    Serial.println("Sending Status: "+value + " to device: " + device);
    // Setup the HTTP request
    HTTPClient http;
    // Sets the URL
    http.begin("http://" + device + "/switch");
    // Sets the content type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Sends the PUT request
    int responseCode = http.PUT("Status="+value);
    http.end();
  }
}
/* SWITCH FUNCTIONS----------------------------------------------------------------------------------------------------------------*/
