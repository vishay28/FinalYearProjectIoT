// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// Flash memory library
#include <EEPROM.h>
// HTTP Client library
#include <ESP8266HTTPClient.h>

// Initialise the address counter
int writeAddress = 0;
// Initialise the array to store the values stored in memory
String readValues[3];
// Initialise string to store the IP address of the device
String device;

// Initialise the switch value variable
int inputValue;

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
  // Allocate 512 bytes in flash storage
  EEPROM.begin(512);
  delay(10);
  Serial.println('\n');

  // Read in the network name, password and connected device from memory if it exisits
  readFromMem();
  delay(1000);
  String networkName = readValues[0];
  String networkPassword = readValues[1];
  device = readValues[2];

  // Determining whether to run in setup mode or connect to an existing network
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/
  if (networkName != "") {
    // Attempt to connect to the home network
    Serial.print("Connecting to: ");
    Serial.print(networkName);
    Serial.print(" With password: ");
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
      pinMode(14, INPUT_PULLUP);
      inputValue = digitalRead(14);

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);
    
      // Updates the device that the switch is connected to
      server.on("/devices", HTTP_PUT, updateDevices);

      // Gets the IP address of the device that the switch is connected to
      server.on("/devices", HTTP_GET, getDevices);

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
    WiFi.softAP("smartSwitch", "");
  
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
  server.handleClient();
  // Checks if the switch has been toggled
  if (digitalRead(14) != inputValue) {
    // Saves the new state of the switch
    inputValue = digitalRead(14);
    // Calls the function to toggle the connected device
    sendSwitch(inputValue);
  }
}
/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/
// Returns the form for the user to set up the device through a browser
void networkForm(){
  server.send(200, "text/html", "<form action=\"/setup\" method=\"POST\" enctype=\"multipart/form-data\"> SSID: <input type=\"text\" name=\"ssid\"><br> Password: <input type=\"text\" name=\"password\"><br><input type=\"submit\" value=\"Submit\">");
}

// Saves the network details that were submitted by the user
void postSetup(){
  String ssid, password;

  //Gets the network details from the response
  ssid = server.arg(0);
  password = server.arg(1);

  // Clears the exisiting memory
  cleanMem();

  // Writes the network details to memory
  saveToMem(ssid, false);
  saveToMem(password, false);

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
  server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartSwitch\"}");
  delay(100);
  // Disconnects from the phone
  WiFi.softAPdisconnect(true);
}

// Function to update the device connected to the switch
void updateDevices(){
  // Gets the IP address of the device from the parameters
  device = server.arg(0);
  saveToMem(device, true);
  Serial.println("Device Saved: " + device);
  server.send(204);
}

// Function to update the connected device
void resetSettings(){
  cleanMem();
  server.send(200);
  delay(100);
  resetFunc();
}

// Function to return the IP address of the device that is connected to the switch
void getDevices() {
  server.send(200, "text/plain", "{\"deviceIp\":"+String(device)+"}");
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

/* SWITCH FUNCTIONS----------------------------------------------------------------------------------------------------------------*/

/* MEMORY FUNCTIONS-------------------------------------------------------------------------------------------------------------------*/
void cleanMem() {
  for (int i = 0; i < 512; i++) {
    if (EEPROM.read(i) != 255)
    {
      EEPROM.write(i, 255);
      EEPROM.commit();
    }
  }
  Serial.println("Cleaned Memory");
}


void saveToMem(String valueToSave, bool saveDevice) {
  
  int valueLength = valueToSave.length()+1; 
  char charArray[valueLength];
  
  valueToSave.toCharArray(charArray, valueLength);
  
  EEPROM.write(writeAddress, sizeof(charArray)-1);
  EEPROM.commit();
  writeAddress++;
  for (int i = 0; i < sizeof(charArray)-1; i++)
  {
    EEPROM.write(writeAddress, charArray[i]);
    EEPROM.commit();
    writeAddress++;
  }
  if (saveDevice) {
    writeAddress = writeAddress - sizeof(charArray);
  }
  Serial.print("\nAdded ");
  Serial.print(valueToSave);
  Serial.print(" To Memory");
  Serial.print("\n");
}


void readFromMem() {
  int readAddress = 0;
  int lengthOfValue;

  byte rawChar;
  char convertedChar;
  String value;
  
  for (int i = 0; i < 3; i++)
  {
    value = "";
    lengthOfValue = EEPROM.read(readAddress);
    if (lengthOfValue != 255) {
     readAddress++;
      for (int a = 0; a < lengthOfValue; a++)
      {
        rawChar = EEPROM.read(readAddress);
        convertedChar = rawChar;
        value += convertedChar;
       readAddress++;
      }
      readValues[i] = value;
      Serial.println("Read value: "+readValues[i]); 
    }
  }
  if (lengthOfValue != 255) {
    writeAddress = readAddress - (lengthOfValue+1);
  }
  else {
    writeAddress = readAddress;
  }
}
/* MEMORY FUNCTIONS-------------------------------------------------------------------------------------------------------------------*/
