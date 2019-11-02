// WiFi library
#include <ESP8266WiFi.h>
// Webserver library
#include <ESP8266WebServer.h>
// Flash memory library
#include <EEPROM.h>

// Initialise the address counter
int writeAddress = 0;
// Initialise the array to store the network details
String readValues[2];

// Creating a webserver on port 80
ESP8266WebServer server(80);


void setup() {
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
      delay(1000);
      Serial.print(++i); Serial.print(' ');
    }
    // Determining if the WiFi connection was successfull
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Connecting to WiFi timed out!");
      Serial.println("Entering setup mode");
      //Setting the network name to null so that the setup mode runs
      networkName = "";
    }
    else {
      Serial.println("Connection established!");  
      Serial.print("IP address: ");
      Serial.print(WiFi.localIP());
      Serial.print("\n");

      pinMode(12, OUTPUT);
      pinMode(13, OUTPUT);
    
      // Saves the network details submitted by the form
      server.on("/switch", HTTP_PUT, switchStatus);
    
      // Starts the server
      server.begin();
    }
  }
/* WIFI MODE--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP MODE-------------------------------------------------------------------------------------------------------------------------*/
  if (networkName == "") {
    
    // Starting up the access point with the provided ssid and password
    WiFi.softAP("smartPlug", "");
  
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

/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/
void loop(void){
  if (readValues[0] == "" || WiFi.status() != WL_CONNECTED) {
   // Listen for HTTP requests from clients 
   server.handleClient();
  }
  else {
    server.handleClient();
  }
}
/* MAIN LOOP--------------------------------------------------------------------------------------------------------------------------*/

/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/
void networkForm(){
  server.send(200, "text/html", "<form action=\"/setup\" method=\"POST\" enctype=\"multipart/form-data\"> SSID: <input type=\"text\" name=\"ssid\"><br> Password: <input type=\"text\" name=\"password\"><br><input type=\"submit\" value=\"Submit\">");
}

void postSetup(){
  String ssid, password;
  ssid = server.arg(0);
  password = server.arg(1);
  cleanMem();
  saveToMem(ssid);
  saveToMem(password);
  server.send(201, "text/plain", "201: Saved WiFi details, please connect to your main network");
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found");
}
/* SETUP REQUEST FUNCTIONS------------------------------------------------------------------------------------------------------------*/

/* CONTROL REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/
void switchStatus(){
  String statusVal;
  statusVal = server.arg(0);
  Serial.print("Switch status: ");
  Serial.print(statusVal);
  Serial.print("\n");
  if (statusVal == "true")
  {
    digitalWrite(12, HIGH);
    delay(10);
    digitalWrite(12, LOW);
  }
  else {
    digitalWrite(13, HIGH);
    delay(10);
    digitalWrite(13, LOW);
  }
  server.send(204);
}
/* CONTROL REQUEST FUNCTIONS----------------------------------------------------------------------------------------------------------*/

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
