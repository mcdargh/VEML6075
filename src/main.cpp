#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>

#include "SSD1306.h"

#include <VEML6075.h>

/* Place your settings in the settings.h file for ssid, password, and dweet post string
FILE: settings.h
// WiFi settings
const char* ssid = "MYSSID";
const char* password = "MyPassword";
const char* dweet = "/dweet/for/myservice?uv=";
*/
#include "settings.h"

// Host
const char *host = "dweet.io";
// Declare sensor controller instance
VEML6075 my_veml6075 = VEML6075();

bool wifiConnected = false;

// OLED
SSD1306 display(0x3c, D2, D1);

// statics for display
char _uv[32], _uva[32], _uvb[32];

// forwards
void connectToSerial(void);
bool connectToWifi(void);
bool initialize_VEML(void);
void sendToDweet(const char *uvlevel);
void flashLED();
void startOLED();
void message(const char *msg);
void setupOTA(void);

void setup()
{
  // turn off LED
  digitalWrite(2, HIGH);

  connectToSerial();
  startOLED();
  if (connectToWifi())
  {
    setupOTA();
  }
  else
  {
    ESP.restart();
  }

  if (initialize_VEML() != true)
  {
    message("Unable to initialize\n");
  }

  // get led ready
  pinMode(2, OUTPUT);
}

void connectToSerial()
{
  Serial.begin(9600);
  while (!Serial)
    ; //wait for serial port to connect (needed for Leonardo only)
  Wire.begin();
}

void startOLED()
{
  // Initialize the OLED display using Wire library
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "Started...");
  display.display();
  yield();
}

bool connectToWifi()
{
  int numtries = 20;
  // Connect to WiFi
  WiFi.hostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  delay(500);
  while (numtries-- && (WiFi.status() != WL_CONNECTED))
  {
    delay(750);
    message(".");
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    message("WIFI not connected\n");
    return false;
  }

  message("\n");
  message("WiFi connected\n");

  // Print the IP address
  message(WiFi.localIP().toString().c_str());
  message("\n");

  WiFi.setAutoReconnect(true);
  wifiConnected = true;
  return true;
}

bool initialize_VEML()
{
  if (my_veml6075.begin() == false)
  {
    message("Unable to communicate with VEML6075.");
    while (1)
      ;
  }

  message("Found VEML6075 sensor");
  return true;
}

void loop()
{
  char msg[256];

  ArduinoOTA.handle();
  flashLED();

  message("Taking reading...");

  my_veml6075.poll();

  float intA = my_veml6075.getUVAIntensity();
  float intB = my_veml6075.getUVBIntensity();
  float uvi = my_veml6075.getUVIndex();

  Serial.printf("Readings - uva: %f µW/cm^2, uvb: %f µW/cm^2, uvi: %f\r\n", intA, intB, uvi);

  dtostrf(intA, 4, 2, _uva);
  dtostrf(intB, 4, 2, _uvb);
  dtostrf(uvi, 4, 2, _uv);
  message("Reading complete...");

  snprintf(msg, 256, "uv=%s&uva=%s&uvb=%s", _uv, _uva, _uvb);
  sendToDweet(msg);

  delay(1000);
}

void flashLED(void)
{
  // flash led
  digitalWrite(2, LOW);
  delay(250);
  digitalWrite(2, HIGH);
}

void sendToDweet(const char *uvlevel)
{
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort))
  {
    message("connection to dweet failed\n");
    return;
  }

  // This will send the request to the server
  client.print(String("POST ") + dweet + uvlevel + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);

  // Read all the lines of the reply from server and print them to Serial
  while (client.available())
  {
    String line = client.readStringUntil('\r');
  }
}

void message(const char *pmsg)
{
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
  snprintf(msg, 127, "UVA: %s µW/cm^2", _uva);
  display.drawString(0, 20, msg);
  snprintf(msg, 127, "UVB: %s µW/cm^2", _uvb);
  display.drawString(0, 30, msg);

  // show the message
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 45, pmsg);

  display.display();
  yield();
}

void setupOTA(void)
{
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  ArduinoOTA.setPassword(otapwd);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    flashLED();
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
#ifdef _DISPLAY_
    if (displayConnected)
    {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawString(64, 10, "OTA Update");
      display.drawProgressBar(2, 28, 124, 10, progress / (total / 100));
      display.display();
    }
#endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

// EOF
