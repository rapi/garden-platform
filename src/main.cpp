#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// const char *ssid = "MOLDTELECOM_F47";
// const char *password = "3043501914";
const String ssid = "HUAWEI-aah9";  
const String password = "4gd19aib"; 

#define RESET_EEPROM false


int PIN_COUNT = 16;
int TIME_PER_ZONE = 20;
bool IS_STOP_WATERING = false;
bool IS_LOCAL_AP = false;
bool IS_STARTED_WATERING = false;
int active_pin = 0;
int pins[] = {23, 33, 25, 14, 26, 32, 27, 13, 22, 21, 19, 18, 5, 17, 4, 15};
int zone_time[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
String TIMER = "";

TaskHandle_t Task1;
TaskHandle_t Task2;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80);

void WRITE_VARIABLES()
{
  for (int i = 0; i < PIN_COUNT; i++)
  {
    EEPROM.write(i, zone_time[i]);
  }
  if (TIMER != "")
  {
    EEPROM.write(PIN_COUNT, TIMER.substring(0, 2).toInt());
    EEPROM.write(PIN_COUNT + 1, TIMER.substring(3, 5).toInt());
  }
  EEPROM.commit();
}

void READ_VARIABLES()
{
  // read intervals
  for (int i = 0; i < PIN_COUNT; i++)
  {
    zone_time[i] = EEPROM.read(i);
  }

  // read times
  int h = EEPROM.read(PIN_COUNT);
  int m = EEPROM.read(PIN_COUNT + 1);
  String Sh = String(h < 10 ? "0" + String(h) : h);
  String Sm = String(m < 10 ? "0" + String(m) : m);
  Serial.println(Sh + ":" + Sm);
  TIMER = Sh + ":" + Sm;
}

void GET_INFO(AsyncWebServerRequest *request)
{
  String response = "{";
  response += "\"time\":\"" + TIMER + "\"";
  response += ",\"isWatering\":" + String(active_pin ? "true" : "false") + ",";
  response += "\"zones\":[";
  for (int i = 0; i < PIN_COUNT; i++)
  {
    Serial.print(String(pins[i]) + " ");
    response += "{";
    response += "\"pin\":" + String(pins[i]);
    response += ",\"interval\":" + String(zone_time[i]);
    response += String(",\"isEnabled\":") + String((digitalRead(pins[i]) == LOW ? "true" : "false"));
    response += "}";
    if (i < sizeof(pins) / sizeof(pins[0]) - 1)
    {
      response += ",";
    }
  }
  Serial.println();
  response += "]";
  response += "}";
  AsyncWebServerResponse *res = request->beginResponse(200, "text/plain", response);
  res->addHeader("Access-Control-Allow-Origin", "*");

  request->send(res);
}

void Task1code(void *parameter)
{
  while (true)
  {
  start:
    if (!active_pin)
    {
      delay(1000);
      continue;
    }
    for (int i = 0; i < PIN_COUNT; i++)
    {
      digitalWrite(pins[i], LOW);
      Serial.println("Start zone" + String(i));

      for (int j = 0; j < zone_time[i] * 60; j++)
      {
        if (active_pin == 0)
        {
          Serial.println("Success Stop");
          digitalWrite(pins[i], HIGH);
          goto start;
        }
        delay(1000);
      }
      digitalWrite(pins[i], HIGH);
      active_pin = pins[i + 1];
    }
    active_pin = 0;
  }
}

void STOP_WATERING(AsyncWebServerRequest *request)
{

  active_pin = 0;
  AsyncWebServerResponse *res = request->beginResponse(200, "text/plain", "No problem, sir \n");
  res->addHeader("Access-Control-Allow-Origin", "*");
  request->send(res);
}

void START_ONE(AsyncWebServerRequest *request)
{
  AsyncWebParameter *p1 = request->getParam(0);
  int pin = String(p1->value()).toInt();
  Serial.println(pin);
  if (digitalRead(pin))
  {
    digitalWrite(pin, LOW);
  }
  else
  {
    digitalWrite(pin, HIGH);
  }
  AsyncWebServerResponse *res = request->beginResponse(200, "text/plain", "No problem, sir \n");
  res->addHeader("Access-Control-Allow-Origin", "*");
  request->send(res);
}

void SET_INTERVAL(AsyncWebServerRequest *request)
{

  AsyncWebParameter *p1 = request->getParam(0);
  AsyncWebParameter *p2 = request->getParam(1);
  AsyncWebParameter *pin = p1;
  AsyncWebParameter *value = p2;
  if (p1->name() != "pin")
  {
    pin = p2;
    value = p1;
  }
  for (int i = 0; i < PIN_COUNT; i++)
  {
    if (String(pins[i]) == pin->value())
    {
      zone_time[i] = String(value->value()).toInt();
      Serial.print(zone_time[i]);
      WRITE_VARIABLES();
      break;
    }
  }

  AsyncWebServerResponse *res = request->beginResponse(200, "text/plain", "No problem, sir \n");
  res->addHeader("Access-Control-Allow-Origin", "*");

  request->send(res);
}

void SET_TIMER(AsyncWebServerRequest *request)
{
  AsyncWebParameter *p = request->getParam(0);
  TIMER = p->value();
  WRITE_VARIABLES();
  AsyncWebServerResponse *res = request->beginResponse(200, "text/plain", "No problem, sir \n");
  res->addHeader("Access-Control-Allow-Origin", "*");
  request->send(res);
}

void SCAN(AsyncWebServerRequest *request)
{
  String json = "[";
  int n = WiFi.scanComplete();
  if (n == -2)
  {
    WiFi.scanNetworks(true);
  }
  else if (n)
  {
    for (int i = 0; i < n; ++i)
    {
      if (i)
        json += ",";
      json += "{";
      json += "\"rssi\":" + String(WiFi.RSSI(i));
      json += ",\"ssid\":\"" + WiFi.SSID(i) + "\"";
      json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
      json += ",\"channel\":" + String(WiFi.channel(i));
      json += ",\"secure\":" + String(WiFi.encryptionType(i));
      json += "}";
    }
    WiFi.scanDelete();
    if (WiFi.scanComplete() == -2)
    {
      WiFi.scanNetworks(true);
    }
  }
  json += "]";
  request->send(200, "application/json", json);
}

void START_WATERING(AsyncWebServerRequest *request)
{
  active_pin = pins[0];

  AsyncWebServerResponse *res = request->beginResponse(200, "text/plain", "No problem, sir \n");
  res->addHeader("Access-Control-Allow-Origin", "*");
  request->send(res);
}

void RESET_MEMORY()
{
  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  delay(500);
}

void REBOOT(AsyncWebServerRequest *request)
{
  RESET_MEMORY();
  ESP.restart();
  request->send(200, "text/plain", "No problem, sir \n");
}

void START_SERVER(void)
{
  Serial.println("Trying to connect to");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.println("");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (++i > 10)
    {
      ESP.restart();

      break;
    }
    Serial.print(".");
  }
  if (i <= 10)
  {
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  if (MDNS.begin("esp32"))
  {
    Serial.println("MDNS responder started");
  }

  server.on("/api/start_all", HTTP_GET, START_WATERING);
  server.on("/api/start_one", HTTP_GET, START_ONE);
  server.on("/api/stop", HTTP_GET, STOP_WATERING);
  server.on("/api/set_interval", HTTP_GET, SET_INTERVAL);
  server.on("/api/set_timer", HTTP_GET, SET_TIMER);
  server.on("/api/info", HTTP_GET, GET_INFO);
  server.on("/api/reboot", HTTP_GET, REBOOT);
  server.on("/api/scan", HTTP_GET, SCAN);

  server.begin();
  Serial.println("HTTP server started");
}

void UPGRADE_BOARD()
{
  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();
}

void setup(void)
{

  EEPROM.begin(100);
  delay(2000);

  Serial.begin(115200);

  for (int i = 0; i < PIN_COUNT; i++)
  {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], HIGH);
  }

  if (EEPROM.read(0))
    READ_VARIABLES();
  else
    WRITE_VARIABLES();

  START_SERVER();
  UPGRADE_BOARD();
  Serial.print(EEPROM.length());

  timeClient.begin();
  timeClient.setTimeOffset(10800);
  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task2",   /* Name of the task */
      10000,     /* Stack size in words */
      NULL,      /* Task input parameter */
      1,         /* Priority of the task */
      &Task2,    /* Task handle. */
      1);        /* Core where the task should run */
}

void loop(void)
{
  ArduinoOTA.handle();
  timeClient.forceUpdate();
  Serial.println(timeClient.getFormattedTime());
  if (timeClient.getFormattedTime().indexOf(TIMER) == 0 && TIMER != "" && !active_pin)
  {
    active_pin = pins[0];
  }
  delay(1000);
}