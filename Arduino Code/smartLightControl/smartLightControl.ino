// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// File access library
#include <FS.h>
// LED strip library
#include <FastLED.h>

// Defining the data pin for the LEDs
#define LED_PIN 14
// Defining how many LEDS are on the strip
#define NUM_LEDS 15
// Creating an LED arrray
CRGB leds[NUM_LEDS];

// Initialise the RGB values
int R = 0;
int G = 0;
int B = 0;
bool christmas = false;

// Creating a webserver on port 80
ESP8266WebServer server(80);

void setup() {
  // Initialising the LEDS in the strip
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  // Setting all the LEDs to off
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
  // Mount the file system
  SPIFFS.begin();
  delay(10);
  Serial.println('\n');

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
  // If christmas mode is on then do the christmas animation
  while (christmas) {
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
    server.handleClient();
    delay(500);
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

  Serial.println("Saved the SSID: "+server.arg(0));
  Serial.println("Saved the Password: "+server.arg(1));

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

/* WIFI REQUEST SUPPORT FUNCTIONS--------------------------------------------------------------------------------------------------*/
// Function to update the LEDs to new values
void updateLeds(int rNew, int gNew, int bNew) {
  // Loops through all the LEDs and sets the new values
  for (int i=1; i<=NUM_LEDS; i++) {
    leds[i-1] = CRGB(rNew, gNew, bNew);
  }
  // Shows the updated values on the strip
  FastLED.show();
}

/* WIFI REQUEST SUPPORT FUNCTIONS--------------------------------------------------------------------------------------------------*/

/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/
// Function to return the IP address of the arduino on the home network
void sendIp() {
  // Sends the IP address along with the device type
  server.send(200, "text/plain", "{"+WiFi.localIP().toString()+":\"smartLight\"}");
  delay(100);
  // Disconnects from the phone
  WiFi.softAPdisconnect(true);
}

// Function that returns the status of the strip
void getColour() {
  // Checks if the LEDs are on
  if (leds[0]) {
    server.send(200, "text/plain", "{\"status\":true, \"r\":"+String(R)+", \"g\":"+String(G)+", \"b\":"+String(B)+", \"christmas\":"+String(christmas)+"}");
  } else {
    server.send(200, "text/plain", "{\"status\":false, \"r\":"+String(R)+", \"g\":"+String(G)+", \"b\":"+String(B)+", \"christmas\":"+String(christmas)+"}");
  }
}

// Function to handle a request to update the colours
void updateColour() {
  // Checks if the request was to turn on christmas mode
  if (server.arg(3) == "true") {
    christmas = true;
  }
  else if (server.arg(3) == "false") {
    christmas = false;
    updateLeds(0,0,0);
  }
  else if (!christmas) {
   // Set the new values of R, G, B
   R = server.arg(0).toInt();
   G = server.arg(1).toInt();
   B = server.arg(2).toInt();
   // Call the update LEDs function
   updateLeds(R, G, B);
  }
  server.send(204);
}

// Function to handle requests from a switch
void switchControl() {
  // Checks if the switch was turned off
  if (server.arg(0) == "false") {
    christmas = false;
    // Turn the LEDs off
    updateLeds(0,0,0);
  }
  else {
    // Checks to see any RGB values have been set
    if ((R == 0) and (G == 0) and (B == 0)) {
      // If not then set the RGB to white
      R = 255;
      G = 255;
      B = 255;
    }
    // If christmas mode is off and the switch was turned on, set the LEDs to the last RGB values
    if (!christmas) {
     updateLeds(R, G, B); 
    }
  }
  server.send(204);
}

// Function to reset the settings
void resetSettings(){
  // Format the files saved
  SPIFFS.format();
  Serial.println("Reset the device");
  server.send(200);
  delay(100);
  resetFunc();
}
/* WIFI REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/
