//####################################################################
//########################## LIBRARIES ###############################
//####################################################################

#include <esp_task_wdt.h>
//#include <esp_wifi.h>

#include <WiFi.h>          
#include "ESP32Ping.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>

#include <MQTT.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include "PMS.h"
#include "SHTSensor.h"

#include <FastLED.h>

//####################################################################



//####################################################################
//#################### DEFINES & PROTOTYPES ##########################
//####################################################################

#define WDT_TIMEOUT 600
#define WIFIMANAGER_TIMEOUT 300
#define DEBUG 1
#define WIFI_CONNECT_TIMEOUT 30000

#define RXD2_PIN 16
#define TXD2_PIN 17
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

#define uS_TO_MIN_FACTOR    60*1000000  /* Conversion factor for micro seconds to minutes */
#define TIME_TO_SLEEP       9        /* Time ESP32 will go to sleep (in minutes) */
#define TIME_TO_RECONNECT   1

#define NUM_LEDS 1
#define DATA_PIN 5
#define CLOCK_PIN 13

void ConnectMQTT(void);
void MQTTMessageReceived(String &topic, String &payload);
void MQTTPubblishJson();
void PrintWakeupReason();

//####################################################################



//####################################################################
//########################## GLOBAL VAR ##############################
//####################################################################

WiFiManager wifiManager;
const char* stationName = "AirM8-YOURNAMES";

WiFiClient net;
MQTTClient client;
const char* mqttServer = "192.168.1.99";
const int mqttPort = 1883;

unsigned long lastMillis = 0;

PMS pms(Serial2);
PMS::DATA data, averageData;
SHTSensor sht31;
float shtHumidity;
float shtTemperature;

const size_t capacity = JSON_OBJECT_SIZE(6);
DynamicJsonDocument jsonDoc(capacity);

bool connectionSuccess;

CRGB leds[NUM_LEDS];
//####################################################################


//####################################################################
//############################# SETUP ################################
//####################################################################


void setup() {
  Serial.begin(115200);
  Serial.println(F("PM Logger Node")); 
  Serial.println();

  Serial2.begin(9600, SERIAL_8N1, RXD2_PIN, TXD2_PIN);
  Serial.println("Serial Txd is on pin: "+String(TX));
  Serial.println("Serial Rxd is on pin: "+String(RX));

  Serial.println("Configuring WDT...  " + String(WDT_TIMEOUT) + " seconds");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  PrintWakeupReason();

  pms.passiveMode();    // Switch to passive mode

  pinMode(PIN_I2C_SDA, OUTPUT);
  pinMode(PIN_I2C_SCL, OUTPUT);
  digitalWrite(PIN_I2C_SDA, 1); // activate internal pullups for twi.
  digitalWrite(PIN_I2C_SCL, 1);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);  
  Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties

  Serial.println("Initialization SHT31: ");
  if (sht31.init()) {
      Serial.print("init(): success\n");
  } else {
      Serial.print("init(): failed\n");
  }
  sht31.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM); // only supported by SHT3x

  wifiManager.setConfigPortalTimeout(WIFIMANAGER_TIMEOUT);
  wifiManager.autoConnect(stationName);
  Serial.println("Wifi");
 
  client.begin(mqttServer, net);
  client.onMessage(MQTTMessageReceived);
  
  pms.sleep();
  delay(1000);

  bool ret = Ping.ping(mqttServer, 5);
  if (ret)
    Serial.println("Ping successfull");
  else
    Serial.println("Ping NOT successfull");
  
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed

  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);
  // LED light up in green 
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(500);
  // LED light up in blue 
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(500);

}

//####################################################################


//####################################################################
//############################# LOOP #################################
//####################################################################


void loop() 
{  

  Serial.println("Waking up, wait 30 seconds for stable readings...");
  pms.wakeUp();
  delay(30000);
  Serial.println ("ARPA Limits | PM10 | PM2.5 | PM1");
  Serial.println("            | 40   | 25    | ??");

  averageData.PM_AE_UG_1_0 = 0;
  averageData.PM_AE_UG_2_5 = 0;
  averageData.PM_AE_UG_10_0 = 0;
  averageData.PM_SP_UG_1_0 = 0;
  averageData.PM_SP_UG_2_5 = 0;
  averageData.PM_SP_UG_10_0 = 0;

  for(int i=0;i<10;i++)
  {
    Serial.println("Send read request #" + String(i));
    pms.requestRead();
    if (pms.readUntil(data,1000))
    {
      Serial.print("PM 1.0 (ug/m3): " + String(data.PM_SP_UG_1_0) + "\t");
      Serial.print("PM 2.5 (ug/m3): " + String(data.PM_SP_UG_2_5) + "\t");
      Serial.print("PM 10.0 (ug/m3): " + String(data.PM_SP_UG_10_0) + "\n");

      averageData.PM_AE_UG_1_0 += data.PM_AE_UG_1_0;
      averageData.PM_AE_UG_2_5 += data.PM_AE_UG_2_5;
      averageData.PM_AE_UG_10_0 += data.PM_AE_UG_10_0;
      averageData.PM_SP_UG_1_0 += data.PM_SP_UG_1_0;
      averageData.PM_SP_UG_2_5 += data.PM_SP_UG_2_5;
      averageData.PM_SP_UG_10_0 += data.PM_SP_UG_10_0;
    }
    else
    {
      Serial.println("No data.");
    }
    delay(3000);
  }

  averageData.PM_AE_UG_1_0 /= 10;
  averageData.PM_AE_UG_2_5 /= 10;
  averageData.PM_AE_UG_10_0 /= 10;
  averageData.PM_SP_UG_1_0 /= 10;
  averageData.PM_SP_UG_2_5 /= 10;
  averageData.PM_SP_UG_10_0 /= 10;

  Serial.println("MEAN PM 1.0 (ug/m3): " + String(averageData.PM_SP_UG_1_0));
  Serial.println("MEAN 2.5 (ug/m3): " + String(averageData.PM_SP_UG_2_5));
  Serial.println("MEAN 10.0 (ug/m3): " + String(averageData.PM_SP_UG_10_0));

  delay(1000);
  pms.sleep();

  shtHumidity = sht31.getHumidity();
  shtTemperature = sht31.getTemperature();

  Serial.println(String("")+"Humidity [%RH]  :  "+shtHumidity+"%");
  Serial.println(String("")+"Temperature [°C]:  "+shtTemperature+"°C");

  bool pingSuccess = Ping.ping(mqttServer, 5);
  if (pingSuccess)
    Serial.println("Ping successfull");
  else
  {
    Serial.println("Ping NOT successfull, reconnecting Wifi...");
    //ReconnectWiFi();
    wifiManager.autoConnect(stationName);
  }

  delay(1000);

  if (client.connected() != true) {
    Serial.println("MQTT client was not connected, reconnecting MQTT...");
    ConnectMQTT();
  }

  
  delay(1000);  // <- fixes some issues with WiFi stability 

  MQTTPubblishJson();
  

  if (!client.connected())
    connectionSuccess = 0;
  else
    connectionSuccess = 1;
  
  client.loop();

  delay(1000);
  WiFi.disconnect(true);

  if (connectionSuccess)
  {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_MIN_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Minutes");
    //delay(9*1000*60);
  }
  else
  {
    esp_sleep_enable_timer_wakeup(TIME_TO_RECONNECT * uS_TO_MIN_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_RECONNECT) + " Minutes");
  }

  client.disconnect();


  delay(1000);


  if (averageData.PM_SP_UG_10_0 < 25)
    leds[0] = CRGB::Green;
  else if (averageData.PM_SP_UG_10_0 < 50)
    leds[0] = CRGB::Yellow;
  else
    leds[0] = CRGB::Red;

  FastLED.show();  

  esp_deep_sleep_start();


}

//####################################################################


//####################################################################
//########################## FUNCTIONS ###############################
//####################################################################

void ConnectMQTT() 
{
  Serial.print("checking wifi...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi NOT connected!");
    delay(1000);
  }

  Serial.print("\nMQTT connecting...");
  lastMillis = millis();
  
  client.connect("esp32temtop");

  while (!client.connected()) {
    Serial.print(".");
    delay(1000);
    if (millis() - lastMillis > 10000)
    {
      Serial.println("\nMQTT connection FAILED!");
      break;
    }
  }
  if (millis() - lastMillis < 10000)
    Serial.println("\nMQTT connected!");

  client.subscribe("/hello");
}

void MQTTMessageReceived(String &topic, String &payload) 
{
  Serial.println("incoming: " + topic + " - " + payload);
}

void MQTTPubblishJson(void)
{
  char outputJson[300];
  jsonDoc["PM1"] = averageData.PM_SP_UG_1_0;
  jsonDoc["PM25"] = averageData.PM_SP_UG_2_5;
  jsonDoc["PM10"] = averageData.PM_SP_UG_10_0;
  jsonDoc["TEMP"] = shtTemperature;
  jsonDoc["RH"] = shtHumidity;
  jsonDoc["millis"] = millis() - lastMillis;
  serializeJson(jsonDoc, outputJson);
  delay(10);  // <- fixes some issues with WiFi stability
  client.publish("/environmentB", outputJson);
  delay(10);
  
  delay(10);  // <- fixes some issues with WiFi stability
  if (DEBUG)
    Serial.println("MQTT message published");
}

void PrintWakeupReason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}