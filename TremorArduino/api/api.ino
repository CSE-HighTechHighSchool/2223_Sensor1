
/*
Purpose:
 This sketch collects data from an Arduino sensor and sends it
 to a Flask server.  The Flask server will then update the corresonding
 Firebase realtime database.
Notes:
 1.  This example is written for a network using WPA encryption. 
 2.  Circuit:  Arduino Nano IoT, HC_SR04 rangefinder.  Modify as 
     necessary for your setup.
Instructions:
 1.  Replace the asterisks (***) with your specific network SSIS (network name) 
     and password on the "arduino_secrets.h" tab (these are case sensitive). DO NOT change lines 34 & 35.
 2.  Update Line 49 with the IP address for the computer running the Flask server.
     Note the use of commas in the IP address format:  ***,***,***,***
 3.  Update Line 128 with the same IP address you added to Line 49, except this time
     use periods between groups of digits, not commas (i.e.,  ***.***.***.***)
 4.  Rename the range() function on line 117 with the function for your circuit
 5.  Replace the range() function (lines 155 - 164) with your the data collection function for
     your circuit.
 6.  Don't change any other lines of code.
 */

// Library Inclusions
#include <SPI.h>              // Wireless comms between sensor(s) and Arduino Nano IoT
#include <WiFiNINA.h>         // Used to connect Nano IoT to network
#include <ArduinoJson.h>      // Used for HTTP Request
#include "arduino_secrets.h"  // Used to store private network info
#include <Wire.h>             // Used for I2C
#include <Adafruit_LSM6DSOX.h>

// Define global variables and constants for the circuit & sensor
const int batchSize = 1000;
int32_t accelerometer[3];
//double resultant;
const int EMG_SIG = A6;
//int muscle;
Adafruit_LSM6DSOX sox;
double resultants[batchSize];
double muscles[batchSize];
int batchIndex = 0;


///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;             // your network key index number (needed only for WEP)
int status = WL_IDLE_STATUS;

// Initialize the Wifi client library
WiFiClient client;

// server address:
//char server[] = "jsonplaceholder.typicode.com"; // for public domain server
IPAddress server(192,168,86,20); // for localhost server (server IP address can be found with ipconfig or ifconfig)

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 40; // delay between updates, in milliseconds (10L * 50L is around 1 second between requests)
const unsigned long batchSize = 10; // postingInterval / batchSize = ms per data point
double batchIMU[batchSize];
int batchEMG[batchSize];

void setup(){
  Serial.begin(115200); // Start serial monitor

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Pin INPUTS/OUTPUT
  pinMode(EMG_SIG, INPUT);

  // Set up IMU sensor
  if (!sox.begin_I2C(0x6B)) {
    Serial.println("Failed to connect to IMU (bad solder?)");
  }
  sox.setAccelDataRate(LSM6DS_RATE_833_HZ);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!"); // don't continue
    while (true);
  }

  // check if firmware is outdated
  String fv = WiFi.firmwareVersion(); 
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid); // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    delay(1000); // wait 1 second for connection
  }

  printWifiStatus(); // you're connected now, so print out the status
}

void loop(){
  StaticJsonDocument<200> doc;

  // if there's incoming data from the net connection, append each character to a variable
  String response = "";
  while (client.available()) {
    char c = client.read();
    response += (c);
  }

  // print out non-empty responses to serial monitor
  if (response != "") {
    Serial.println(response);
  }

  // Collect data
  emg();
  imu();
  batchIndex += 1;
  
  if (batchIndex == batchSize) {
    httpRequest();
    batchIndex = 0;
  }
}

// this method makes a HTTP connection to the server:
void httpRequest() {
  // note the time that the connection was made:
  lastConnectionTime = millis();

  // close any connection before send a new request to free the socket
  client.stop();
  
  // if there's a successful connection:
  if (client.connect(server, 5000)) {
    //Serial.println("connecting...");

    // Parse data
    String data = "";
    for (int i = 0; i < batchSize; i++) {
      data += String(resultants[i]) + "," + String(muscles[i]) + ",";
    }

    // send the HTTP GET request with the distance as a parameter.
    // The Flask route to call should be inbetween the "/" and "?" (ex:  GET /test?...
    // where "test" is the Flask route that will GET the data, "distance" is the key
    // and the value is provided by:  String(distance))
    String request = "GET /test?data=" + data + " HTTP/1.1";
    client.println(request);

    // set the host as server IP address
    client.println("Host: 192.168.86.20");

    // other request properties
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
  } else {
    Serial.println("connection failed"); // couldn't make a connection
  }

  Serial.println("Connection length: " + String(millis() - lastConnectionTime));
}

// connect to wifi network and display status
void printWifiStatus(){
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP(); // your board's IP on the network
  Serial.print("IP Address: ");
  Serial.println(ip);
  long rssi = WiFi.RSSI(); // received signal strength
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// collect emg values
void emg(){
  // Read pin
  muscles[batchIndex] = analogRead(EMG_SIG);
}

// collect imu values
void imu(){
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;
  sox.getEvent(&accel, &gyro, &temp);

  // Calculate resultant acceleration
  resultants[batchIndex] = sqrt(sq(accel.acceleration.x)+sq(accel.acceleration.y)+sq(accel.acceleration.z));
}
