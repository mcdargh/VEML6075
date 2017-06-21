#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "VEML6075.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

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
  Serial.begin(9600);
  while(!Serial); //wait for serial port to connect (needed for Leonardo only)
  Wire.begin();
}

void startOLED() {
// by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done
  
  display.display(); // show splashscreen
  delay(2000);
  display.clearDisplay();   // clears the screen and buffer
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
  char cuv[10], cuva[10], cuvb[10];
  
  flashLED();

  // Poll sensor
  my_veml6075.poll();
  // Get "raw" UVA and UVB counts, with the dark current removed
  float uva = my_veml6075.getUVA();
  float uvb = my_veml6075.getUVB();
  // Get calculated UV index based on Vishay's application note
  float uv_index = my_veml6075.getUVIndex();

  dtostrf(uva, 4, 2, cuva);
  dtostrf(uvb, 4, 2, cuvb);
  dtostrf(uv_index, 4, 2, cuv);

  snprintf(msg, 256, "UV Index: %s, UVA: %s, UVB: %s\n", cuv, cuva, cuvb);
  message(msg);
  snprintf(msg, 256, "uv=%s&uva=%s&uvb=%s", cuv, cuva, cuvb);
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

void message(const char* msg) {
  // dump to serial
  Serial.print(msg);

  // if oled then send it there
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print(msg);
}

// EOF
