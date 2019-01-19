#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>

#include "SSD1306.h"

// Include the SparkFun VEML6075 library.
// Click here to get the library: http://librarymanager/All#SparkFun_VEML6075
#include <SparkFun_VEML6075_Arduino_Library.h>

/* Place your settings in the settings.h file for ssid, password, and dweet post string
FILE: settings.h
// WiFi settings
const char* ssid = "MYSSID";
const char* password = "MyPassword";
const char* dweet = "/dweet/for/myservice?uv=";
*/
#include "settings.h"

// constants
// Calibration constants:
// Four gain calibration constants -- alpha, beta, gamma, delta -- can be used to correct the output in
// reference to a GOLDEN sample. The golden sample should be calibrated under a solar simulator.
// Setting these to 1.0 essentialy eliminates the "golden"-sample calibration
const float CALIBRATION_ALPHA_VIS = 1.0; // UVA / UVAgolden
const float CALIBRATION_BETA_VIS = 1.0;  // UVB / UVBgolden
const float CALIBRATION_GAMMA_IR = 1.0;  // UVcomp1 / UVcomp1golden
const float CALIBRATION_DELTA_IR = 1.0;  // UVcomp2 / UVcomp2golden

// Responsivity:
// Responsivity converts a raw 16-bit UVA/UVB reading to a relative irradiance (W/m^2).
// These values will need to be adjusted as either integration time or dynamic settings are modififed.
// These values are recommended by the "Designing the VEML6075 into an application" app note for 100ms IT
const float UVA_RESPONSIVITY = 0.00110; // UVAresponsivity
const float UVB_RESPONSIVITY = 0.00125; // UVBresponsivity

// UV coefficients:
// These coefficients
// These values are recommended by the "Designing the VEML6075 into an application" app note
const float UVA_VIS_COEF_A = 2.22; // a
const float UVA_IR_COEF_B = 1.33;  // b
const float UVB_VIS_COEF_C = 2.95; // c
const float UVB_IR_COEF_D = 1.75;  // d

// Host
const char *host = "dweet.io";
// Declare sensor controller instance
VEML6075 my_veml6075;
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
  Serial.begin(9600);
  if (my_veml6075.begin() == false)
  {
    message("Unable to communicate with VEML6075.");
    while (1)
      ;
  }
  // Integration time and high-dynamic values will change the UVA/UVB sensitivity. That means
  // new responsivity values will need to be measured for every combination of these settings.
  my_veml6075.setIntegrationTime(VEML6075::IT_100MS);
  my_veml6075.setHighDynamic(VEML6075::DYNAMIC_NORMAL);

  message("Found VEML6075 sensor");
  return true;
}

void loop()
{
  char msg[256];

  ArduinoOTA.handle();
  flashLED();

  message("Taking reading...");

  uint16_t rawA, rawB, visibleComp, irComp;
  float uviaCalc, uvibCalc, uvia, uvib, uvi;

  // Read raw and compensation data from the sensor
  rawA = my_veml6075.rawUva();
  rawB = my_veml6075.rawUvb();
  visibleComp = my_veml6075.visibleCompensation();
  irComp = my_veml6075.irCompensation();

  // Calculate the simple UVIA and UVIB. These are used to calculate the UVI signal.
  uviaCalc = (float)rawA - ((UVA_VIS_COEF_A * CALIBRATION_ALPHA_VIS * visibleComp) / CALIBRATION_GAMMA_IR) - ((UVA_IR_COEF_B * CALIBRATION_ALPHA_VIS * irComp) / CALIBRATION_DELTA_IR);
  uvibCalc = (float)rawB - ((UVB_VIS_COEF_C * CALIBRATION_BETA_VIS * visibleComp) / CALIBRATION_GAMMA_IR) - ((UVB_IR_COEF_D * CALIBRATION_BETA_VIS * irComp) / CALIBRATION_DELTA_IR);

  // Convert raw UVIA and UVIB to values scaled by the sensor responsivity
  uvia = uviaCalc * (1.0 / CALIBRATION_ALPHA_VIS) * UVA_RESPONSIVITY;
  uvib = uvibCalc * (1.0 / CALIBRATION_BETA_VIS) * UVB_RESPONSIVITY;

  // Use UVIA and UVIB to calculate the average UVI:
  uvi = (uvia + uvib) / 2.0;

  Serial.println(String(uviaCalc) + ", " + String(uvibCalc) + ", " + String(uvi));

  dtostrf(uvia, 4, 2, _uva);
  dtostrf(uvib, 4, 2, _uvb);
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
