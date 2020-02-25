// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// File access library
#include <FS.h>
// LED strip library
#include <FastLED.h>
// NTP client for getting the time
#include <NTPClient.h>
// UDP library for connecting to the NTP Client
#include <WiFiUdp.h>
// Cron library for running the schedules
#include "CronAlarms.h"
// Time and sys time libraries for setting the system time
#include <time.h>
#include <sys/time.h>

// Defining the data pin for the LEDs
#define LED_PIN 14
// Defining how many LEDS are on the strip
#define NUM_LEDS 15
// Creating an LED arrray
CRGB leds[NUM_LEDS];

// Initialising the UDP server and setting the NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

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

      //Starting the time client and getting the current time
      timeClient.begin();
      timeClient.update();

      // Getting the current time in epoch
      timeval epoch = {timeClient.getEpochTime(), 0};
      timezone tz = {0, 0};
      // Setting the system time to the current time
      settimeofday(&epoch, &tz);
      // Closing the connection
      timeClient.end();

      // Calling the function to load the saved schedules
      loadSchedules();

      // Sends the network IP address
      server.on("/getIp", HTTP_GET, sendIp);
    
      // Get the current RGB values
      server.on("/colour", HTTP_GET, getColour);

      // Update the LED colour
      server.on("/colour", HTTP_POST, updateColour);

      // Switch the LEDs on or off from a switch
      server.on("/switch", HTTP_PUT, switchControl);

      // Add a new schedule
      server.on("/schedule", HTTP_POST, addSchedule);

      // Get a schedule
      server.on("/schedule", HTTP_GET, getSchedule);

      // Delete a schedule
      server.on("/deleteSchedule", HTTP_PUT, deleteSchedule);

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
  // Check for any cron tasks
  Cron.delay();
  //Listening for incomming requests
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
    turnOff();
  }
  else {
    turnOn();
  }
  server.send(204);
}

// Function to add a schedule
void addSchedule() {
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
  server.send(201);
}

// Function to delete a schedule
void deleteSchedule() {
  // Initialising a variable to store a row read from the cron file
  String cronRow;

  // Opening the cron file and creating a new file in which the jobs that aren't being deleted will be copied to
  File cronFile = SPIFFS.open("/cron.txt", "r");
  File cronFileNew = SPIFFS.open("/cronNew.txt", "w");

  // Read every row in the cron file
  while (cronFile.available()) {
    cronRow = cronFile.readStringUntil('\n');
    cronRow.trim();

    // If the row isn't the job that needs to be deleted then copy it to a new file
    if (!cronRow.startsWith(server.arg(0), 0)) {
      Serial.println("Copying: " + cronRow);
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
}

// Function to get the details of a specific cron job
void getSchedule() {
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

/* LIGHT FUNCTION------------------------------------------------------------------------------------------------------------------*/
// Function to update the LEDs to new values
void updateLeds(int rNew, int gNew, int bNew) {
  // Loops through all the LEDs and sets the new values
  for (int i=1; i<=NUM_LEDS; i++) {
    leds[i-1] = CRGB(rNew, gNew, bNew);
  }
  // Shows the updated values on the strip
  FastLED.show();
}

// Function to turn on the light
void turnOn() {
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

// Function to turn off the light
void turnOff() {
  christmas = false;
  // Turn the LEDs off
  updateLeds(0,0,0);
}

/* LIGHT FUNCTION------------------------------------------------------------------------------------------------------------------*/

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
  }
  cronFile.close();
  cronFileNew.close();

  // Replace the old file with the new file
  SPIFFS.remove("/cron.txt");
  SPIFFS.rename("/cronNew.txt", "/cron.txt");
}
