#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "VEML6075.h"

#include "SSD1306.h"

/* Place your settings in the settings.h file for ssid, password, and dweet post string
FILE: settings.h
// WiFi settings
const char* ssid = "MYSSID";
const char* password = "MyPassword";
const char* dweet = "/dweet/for/myservice?uv=";
*/
#include "settings.h"

// Host
const char* host = "dweet.io";
// Declare sensor controller instance
VEML6075 my_veml6075 = VEML6075();

// OLED
SSD1306 display(0x3c, D2, D1);

// statics for display
char _uv[32], _uva[32], _uvb[32];

// forwards
void connectToSerial(void);
void connectToWifi(void);
bool initialize_VEML(void);
void sendToDweet(const char* uvlevel);
void flashLED();
void startOLED();
void message(const char* msg);

void setup()
{
  // turn off LED
  digitalWrite(2, HIGH);
  
  connectToSerial();
  startOLED();
  connectToWifi();

   if(initialize_VEML() != true) {
     message("Unable to initialize\n");

   }

   // get led ready
   pinMode(2, OUTPUT);
}

void connectToSerial() {
  Serial.begin(115200);
  while(!Serial); //wait for serial port to connect (needed for Leonardo only)
  Wire.begin();
}

void startOLED() {
// Initialize the OLED display using Wire library
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Started...");
  display.display();
  yield();
}

void connectToWifi() {
  int numtries = 10;
  // Connect to WiFi
   WiFi.begin(ssid, password);
   while (numtries-- && (WiFi.status() != WL_CONNECTED)) {
     delay(500);
     message(".");
   }
   message("\n");
   message("WiFi connected\n");

   // Print the IP address
   message(WiFi.localIP().toString().c_str());
   message("\n");
}


bool initialize_VEML() {
  // Initialize i2c bus and sensor
  if (my_veml6075.begin()) {
      message("Initialized successfully.\n");
      return true;
    }
    return false;
}

void loop()
{
  char msg[256];
  
  //flashLED();

  message("Taking reading...");

  // Poll sensor
  my_veml6075.poll();

  // Get "raw" UVA and UVB counts, with the dark current removed
  float uva = my_veml6075.getUVA();
  float uvb = my_veml6075.getUVB();
  // Get calculated UV index based on Vishay's application note
  float uv_index = my_veml6075.getUVIndex();

  dtostrf(uva, 4, 2, _uva);
  dtostrf(uvb, 4, 2, _uvb);
  dtostrf(uv_index, 4, 2, _uv);
  message("Reading complete...");

  snprintf(msg, 256, "uv=%s&uva=%s&uvb=%s", _uv, _uva, _uvb);
  sendToDweet(msg);

  delay(1000);
}

void flashLED(void) {
  // flash led
    digitalWrite(2, LOW);
    delay(250);
    digitalWrite(2, HIGH);
}

void sendToDweet(const char* uvlevel) {
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    message("connection to dweet failed\n");
    return;
  }

  // This will send the request to the server
  client.print(String("POST ") + dweet + uvlevel + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);

  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
  }
}

void message(const char* pmsg) {
  char msg[128];

  // dump to serial
  Serial.print(msg);

  // show the UV info
  display.clear();

  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  snprintf(msg, 127, "UV Index: %s", _uv);
  display.drawString(0, 0, msg);
  display.setFont(ArialMT_Plain_10);
  snprintf(msg, 127, "UVA: %s", _uva);
  display.drawString(0, 20, msg);
  snprintf(msg, 127, "UVB: %s", _uvb);
  display.drawString(0, 30, msg);

  // show the message
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 45, pmsg);

  display.display();
  yield();
}

// EOF
