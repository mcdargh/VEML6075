#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "VEML6075.h"

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

// forwards
void connectToSerial(void);
void connectToWifi(void);
bool initialize_VEML(void);
void sendToDweet(const char* uvlevel);
void flashLED();

void setup()
{
  connectToSerial();
  connectToWifi();

   if(initialize_VEML() != true) {
     Serial.println("Unable to initialize");

   }

   // get led ready
   pinMode(2, OUTPUT);
}

void connectToSerial() {
  Serial.begin(9600);
  while(!Serial); //wait for serial port to connect (needed for Leonardo only)
}

void connectToWifi() {
  // Connect to WiFi
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
   }
   Serial.println("");
   Serial.println("WiFi connected");

   // Print the IP address
   Serial.println(WiFi.localIP());
}


bool initialize_VEML() {
  // Initialize i2c bus and sensor
  Wire.begin();
  if (my_veml6075.begin()) {
      Serial.println("Initialized successfully.");
      return true;
    }
    return false;
}

void loop()
{
  char msg[256];
  
  flashLED();

  // Poll sensor
  my_veml6075.poll();
  // Get "raw" UVA and UVB counts, with the dark current removed
  uint16_t uva = my_veml6075.getUVA();
  uint16_t uvb = my_veml6075.getUVB();
  // Get calculated UV index based on Vishay's application note
  float uv_index = my_veml6075.getUVIndex();

  snprintf(msg, sizeof(msg)/sizeof(char), "UV Index: %f, UVA: %d, UVB: %d\n", uv_index, uva, uvb);
  Serial.print(msg);
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
    Serial.println("connection failed");
    delay(1000);
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

// EOF
