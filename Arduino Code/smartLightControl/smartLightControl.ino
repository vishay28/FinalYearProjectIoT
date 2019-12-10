// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// Flash memory library
#include <EEPROM.h>

#include <FastLED.h>

#define LED_PIN 14
#define NUM_LEDS 15
CRGB leds[NUM_LEDS];

// Initialise the address counter
int writeAddress = 0;
// Initialise the array to store the network details
String readValues[2];

int rCurrent = 0;
int gCurrent = 0;
int bCurrent = 0;
int rOld = 0;
int gOld = 0;
int bOld = 0;
bool christmas = false;
bool rave = false;

// Creating a webserver on port 80
ESP8266WebServer server(80);


void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  for (int i=1; i<NUM_LEDS; i++) {
    leds[i-1] = CRGB(0,0,0);
    FastLED.show();
    delay(1);
  }
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

  // Read in the network name and password from memory if it exisits
  readFromMem();
  delay(1000);
  String networkName = readValues[0];
  String networkPassword = readValues[1];

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

      // Sets up the output pins which set and unset the relay
      pinMode(12, OUTPUT);
      pinMode(13, OUTPUT);

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);
    
      // Get the current RGB values
      server.on("/colour", HTTP_GET, getColour);

      // Update the LED colour
      server.on("/colour", HTTP_POST, updateColour);

      // Switch the LEDs on or off from a switch
      server.on("/switch", HTTP_PUT, switchControl);

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
    WiFi.softAP("smartLight", "");
  
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
  if (christmas == true) {
    for (int i=1; i<=NUM_LEDS; i=i+2) {
      leds[i-1] = CRGB(255, 0, 0);
    }
    for (int i=2; i<=NUM_LEDS; i=i+2) {
      leds[i-1] = CRGB(0, 255, 0);
    }
    FastLED.show();
    delay(500);
    for (int i=1; i<=NUM_LEDS; i=i+2) {
      leds[i-1] = CRGB(0, 255, 0);
    }
    for (int i=2; i<=NUM_LEDS; i=i+2) {
      leds[i-1] = CRGB(255, 0, 0);
    }
    FastLED.show();
    delay(500);
  }
  else {
    for (int i=1; i<=NUM_LEDS; i++) {
      leds[i-1] = CRGB(rCurrent, gCurrent, bCurrent);
    }
    FastLED.show();
    delay(1);
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
  String ssid, password;

  //Gets the network details from the response
  ssid = server.arg(0);
  password = server.arg(1);

  // Clears the exisiting memory
  cleanMem();

  // Writes the network details to memory
  saveToMem(ssid);
  saveToMem(password);

  // Retruns a 201 once the details have been saved
  server.send(201, "text/plain", "201: Saved WiFi details, please connect to your main network");

  delay(100);
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
  server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartLight\"}");
  delay(100);
  // Disconnects from the phone
  WiFi.softAPdisconnect(true);
}

void getColour() {
  server.send(200, "text/plain", "{\"r\":"+String(rCurrent)+", \"g\":"+String(gCurrent)+", \"b\":"+String(bCurrent)+"}");
}

void updateColour() {
  if (server.arg(3) == "true") {
    christmas = true;
  }
  else if (server.arg(3) == "false") {
    christmas = false;
  }
  else {
   rCurrent = server.arg(0).toInt();
   gCurrent = server.arg(1).toInt();
   bCurrent = server.arg(2).toInt(); 
  }
  server.send(204);
}

void switchControl() {
  if (server.arg(0) == "false") {
    rOld = rCurrent;
    gOld = gCurrent;
    bOld = bCurrent;
    rCurrent = 0;
    gCurrent = 0;
    bCurrent = 0;
  }
  else {
    if ((rOld == 0) and (gOld == 0) and (bOld == 0)) {
      rCurrent = 255;
      gCurrent = 255;
      bCurrent = 255;
    }
    else {
     rCurrent = rOld;
     gCurrent = gOld;
     bCurrent = bOld;
    }
  }
  server.send(204);
}

// Function to reset the settings
void resetSettings(){
  cleanMem();
  server.send(200);
  delay(100);
  resetFunc();
}
/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/

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


void saveToMem(String valueToSave) {
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
  
  for (int i = 0; i < 2; i++)
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
    }
  }
}
/* MEMORY FUNCTIONS-------------------------------------------------------------------------------------------------------------------*/
