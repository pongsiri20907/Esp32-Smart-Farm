#include <ESP32Time.h>
#include <WiFiManager.h>    // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>   // Mqtt
#include <Preferences.h>    // For save data to ROM
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>
#include "DHT.h"
#include <BH1750FVI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <Preferences.h>
StaticJsonDocument<200> doc;
Preferences preferences;
ESP32Time rtc;

// Setup GPIO
#define TRIGGER_PIN 0
#define DHTPIN 16       // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define rain_sensor 4
#define flow_sensor 18
#define RELAY 5
#define TRIGGER_PIN 0

// Setup Date Time NTCClient
#define UTC_OFFSET_IN_SECONDS 25200
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_IN_SECONDS);

// Setup Global Variable
float t,h,f;
uint16_t lux;
int rain_val;
float flow = 0;
DHT dht(DHTPIN, DHTTYPE);
BH1750FVI LightSensor(BH1750FVI::k_DevModeContLowRes);    // LUX SENSOR
char payload[128];        // For JSON parse
char timer[128];          // For JSON parse
String timerValve1;
// Control threshold Temperature
bool valve, AUTO_VALVE1_TEMP_ENABLED;
float AUTO_VALVE1_TEMP_ON, AUTO_VALVE1_TEMP_OFF;
// Control threshold Humidity
bool AUTO_VALVE1_HUM_ENABLED;
float AUTO_VALVE1_HUM_ON, AUTO_VALVE1_HUM_OFF;
int HH, MM, DD;

// SETUP MQTT 
#define MQTT_SERVER "mqtt-iarc.eiotsoft.net"
#define MQTT_PORT 1883
#define MQTT_USER "iarc"
#define MQTT_PASS "mqtt-iarc"
#define MQTT_NAME "IARC-SMART_FARM"
WiFiClient client;
PubSubClient mqtt(client);

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String topic_str = topic, payload_str = (char*)payload;
  Serial.println("[" + topic_str + "]: " + payload_str);
  deserializeJson(doc, payload_str); 

  if(doc.containsKey("VALVE_1")){
    preferences.begin("iarc-smart-farm", false);
    valve = doc["VALVE_1"];
    preferences.putBool("VALVE_1", valve);
    preferences.end();
    // RELAY
    digitalWrite(RELAY, valve);
    Serial.print(valve);
  }
  if(doc.containsKey("AUTO_VALVE1_TEMP")){
    preferences.begin("iarc-smart-farm", false);
    AUTO_VALVE1_TEMP_ON = doc["AUTO_VALVE1_TEMP"]["on"];
    AUTO_VALVE1_TEMP_OFF = doc["AUTO_VALVE1_TEMP"]["off"];
    AUTO_VALVE1_TEMP_ENABLED = doc["AUTO_VALVE1_TEMP"]["enabled"];
    preferences.putFloat("AUTO_VALVE1_TEMP_ON", AUTO_VALVE1_TEMP_ON);
    preferences.putFloat("AUTO_VALVE1_TEMP_OFF", AUTO_VALVE1_TEMP_OFF);
    preferences.putBool("AUTO_VALVE1_TEMP_ENABLED", AUTO_VALVE1_TEMP_ENABLED);
    preferences.end();
    Serial.print("| ON: " +String(AUTO_VALVE1_TEMP_ON) + "| OFF: " +String(AUTO_VALVE1_TEMP_OFF) + "| ENABLED: "  + String(AUTO_VALVE1_TEMP_ENABLED));
  }
   if(doc.containsKey("AUTO_VALVE1_HUM")){
    preferences.begin("iarc-smart-farm", false);
    AUTO_VALVE1_HUM_ON = doc["AUTO_VALVE1_HUM"]["on"];
    AUTO_VALVE1_HUM_OFF = doc["AUTO_VALVE1_HUM"]["off"];
    AUTO_VALVE1_HUM_ENABLED = doc["AUTO_VALVE1_HUM"]["enabled"];
    preferences.putFloat("AUTO_VALVE1_HUM_ON", AUTO_VALVE1_HUM_ON);
    preferences.putFloat("AUTO_VALVE1_HUM_OFF", AUTO_VALVE1_HUM_OFF);
    preferences.putBool("AUTO_VALVE1_HUM_ENABLED", AUTO_VALVE1_HUM_ENABLED);
    preferences.end();
    Serial.print("| ON: " +String(AUTO_VALVE1_HUM_ON) + "| OFF: " +String(AUTO_VALVE1_HUM_OFF) + "| ENABLED: "  + String(AUTO_VALVE1_HUM_ENABLED));
  }
  if(doc.containsKey("TIMER_VALVE1")){
    timerValve1 = payload_str;
    preferences.begin("iarc-smart-farm", false);
    preferences.putString("TIMER_VALVE1", timerValve1);
    preferences.end();
    Serial.print(timerValve1);
  }
}

// SETUP LCD
#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>
#define TFT_GREY 0x5AEB // New colour
TFT_eSPI tft = TFT_eSPI();  // Invoke library

// SETUP Function
void lcdDisplay(float temp, float hum, float lux, float flow, float rain);
void thresholdControl(float ON, float OFF, float SENSOR_VALUE);
void timerControl();

// SETUP WiFiManager
WiFiManager wm; // global wm instance
bool wm_nonblocking = false; // change to true to use non blocking
WiFiManagerParameter custom_field; // global param ( for non blocking w params )

void checkButton() {
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if ( digitalRead(TRIGGER_PIN) == LOW ) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }

      // start portal w delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);

      if (!wm.startConfigPortal("IARC-SMART-FARM", "12345678")) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback() {
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

//// Internal interupt: Inject every 1 sec
//int count = 0;
//hw_timer_t *My_timer = NULL;
//void IRAM_ATTR onTimer(){
//  Serial.printf("Interupt # %d", count++);
//}

void setup() {
  Serial.begin(9600);
  // Initial Global Variable
  preferences.begin("iarc-smart-farm", false);
  valve = preferences.getBool("VALVE_1", 0);
  AUTO_VALVE1_TEMP_ON = preferences.getFloat("AUTO_VALVE1_TEMP_ON", 0);
  AUTO_VALVE1_TEMP_OFF = preferences.getFloat("AUTO_VALVE1_TEMP_OFF", 0);
  AUTO_VALVE1_TEMP_ENABLED = preferences.getBool("AUTO_VALVE1_TEMP_ENABLED", 0);

  AUTO_VALVE1_HUM_ON = preferences.getFloat("AUTO_VALVE1_HUM_ON", 0);
  AUTO_VALVE1_HUM_OFF = preferences.getFloat("AUTO_VALVE1_HUM_OFF", 0);
  AUTO_VALVE1_HUM_ENABLED = preferences.getBool("AUTO_VALVE1_HUM_ENABLED", 0);
  Serial.println("-------------------------------------------------------------------------------------------------------------------------------------------");
  Serial.println("| VALVE: " + String(valve) + "| AUTO_VALVE1_TEMP_ON: " + String(AUTO_VALVE1_TEMP_ON) + "| AUTO_VALVE1_TEMP_OFF: " + String(AUTO_VALVE1_TEMP_OFF) + "| AUTO_VALVE1_TEMP_ENABLED: " + String(AUTO_VALVE1_TEMP_ENABLED));
  Serial.println("| VALVE: " + String(valve) + "| AUTO_VALVE1_HUM_ON: " + String(AUTO_VALVE1_HUM_ON) + "| AUTO_VALVE1_HUM_OFF: " + String(AUTO_VALVE1_HUM_OFF) + "| AUTO_VALVE1_HUM_ENABLED: " + String(AUTO_VALVE1_HUM_ENABLED));
  Serial.println("-------------------------------------------------------------------------------------------------------------------------------------------");
  
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  pinMode(TRIGGER_PIN, INPUT);
  if (wm_nonblocking)
    wm.setConfigPortalBlocking(false);
  int customFieldLength = 40;
  const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
  new (&custom_field) WiFiManagerParameter(custom_radio_str);
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(saveParamCallback);
  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  wm.setMenu(menu);
  wm.setClass("invert");
  wm.setConfigPortalTimeout(60); // auto close configportal after n seconds
  bool res;
  res = wm.autoConnect("IARC-SMART_FARM", "12345678"); // password protected ap
  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...");
  }

  String ssid = wm.getWiFiSSID(true);
  String pass = wm.getWiFiPass(true);
  Serial.printf("WIFI: %s", ssid);
  Serial.printf("PASSWORD: %s", pass);

  // Mqtt Setup
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(callback);

  // Sensor and Device
  dht.begin();
  LightSensor.begin();
  pinMode(flow_sensor, INPUT);
  pinMode(rain_sensor, INPUT);
  pinMode(RELAY, OUTPUT);
  tft.init();
  tft.setRotation(1);

  //Setup NTPClient
  timeClient.begin();
//  // Internal interupt
//  My_timer = timerBegin(0, 80, true);
//  timerAttachInterrupt(My_timer, &onTimer, true);
//  timerAlarmWrite(My_timer, 1000000, true);
//  timerAlarmEnable(My_timer); //Just Enable
}

void loop() {
  timeClient.update();
  HH = timeClient.getHours();
  MM = timeClient.getMinutes();
  DD = timeClient.getDay();
  Serial.println("HH: " + String(HH) + " " + "MM: " + String(MM) + " " + "DD: " + String(DD));
  // avoid delays() in loop when non-blocking and other long running code
  if (wm_nonblocking) wm.process();
  checkButton();

  // Mqtt Subscribe
  if (mqtt.connected() == false) {
    Serial.print("MQTT connection... ");
    if (mqtt.connect(MQTT_NAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      mqtt.subscribe("IARC_SMART_FARM_controls");
    } else {
      Serial.println("failed");
      delay(5000);
    }
  } else {
    mqtt.loop();
  }

  // Sensor Read data
  // DHT SENSOR
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  f = dht.readTemperature(true);
  // LUX SENSOR
  lux = LightSensor.GetLightIntensity();
  // RAIN DROP SENSOR
  rain_val = analogRead(rain_sensor);
  // FLOW SENSOR
  uint32_t pulse = pulseIn(flow_sensor, HIGH);
  if (pulse < 1) {
    //Serial.println("Pulse < 1");
  }
  else {
    float Hz = 1 / (2 * pulse * pow(10, -6));
    float flow = 7.2725 * (float)Hz + 3.2094;
  }
    
  // CHECK THRESHOLD CONTROL
  if(AUTO_VALVE1_TEMP_ENABLED == true){
      Serial.println("AUTO_VALVE1_TEMP_ENABLED Active ...");
      thresholdControl(AUTO_VALVE1_TEMP_ON, AUTO_VALVE1_TEMP_OFF, t); 
    }
  if(AUTO_VALVE1_HUM_ENABLED == true){
      Serial.println("AUTO_VALVE1_HUM_ENABLED Active ...");
      thresholdControl(AUTO_VALVE1_HUM_ON, AUTO_VALVE1_HUM_OFF, h); 
    }

  // CHECK TIMER CONTROL
  timerControl();

  // DEBUG
  lcdDisplay(t, h, lux, flow, rain_val);
  Serial.println("Humidity : " + String(h, 2) + " %Rh" + "|" + " Temperature : " + String(t, 2) + " Celsius" + "|" + " Light : " + String(lux, 2) + " LUX" + "|" + " FLOW : " + String(flow) + " m^3" + "|" + " Rain" + String(rain_val) + " Unit");
//  // Plot Graph
//  Serial.print(h);
//  Serial.print(" "); // a space ' ' or  tab '\t' character is printed between the two values.
//  Serial.println(t);

  // Parse to JSON for sned mqtt
  DynamicJsonDocument doc(1024);
  JsonObject object = doc.to<JsonObject>();
  object["TEMP"] = t;
  object["HUM"]  = h;
  object["LUX"]  = float(lux);
  object["FLOW"] = flow;
  object["VALVE_1"] = valve;
  serializeJson(doc, payload);

  // Send Data
  mqtt.publish("IARC_SMART_FARM", payload);
  // END LOOP -----------------------------------------------------------
}

void lcdDisplay(float temp, float hum, float lux, float flow, float rain){
  // Fill screen with grey so we can see the effect of printing with and without 
  // a background colour defined
  tft.fillScreen(TFT_BLACK);
  
  // Set "cursor" at top left corner of display (0,0) and select font 2
  // (cursor will move to next line automatically during printing with 'tft.println'
  //  or stay on the line is there is room for the text with tft.print)
  tft.setCursor(0, 0, 2);
  // Set the font colour to be white with a black background, set text size multiplier to 1
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  tft.setTextSize(2);
  // We can now plot text on screen using the "print" class
  tft.println("IARC SMART FARM");
  tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextSize(1);
  tft.println("Monitoring & Control System");

  // Set the font colour to be blue with no background, set to font 4
  tft.setTextColor(TFT_YELLOW);    tft.setTextSize(2); tft.setTextFont(1);
  tft.println("");
  tft.print("Temperature : "); tft.print(temp); tft.println("     C");
  tft.println("");
  tft.print("Humidity    : "); tft.print(hum);  tft.println("    %Rh");
  tft.println("");
  tft.print("Light       : "); tft.print(lux);  tft.println(" Lux");
  tft.println("");
  tft.print("Water Usage : "); tft.print(flow); tft.println("     m^3");
  tft.println("");
  tft.print("Rainfall    : "); tft.print(rain); tft.println("   mm");

  //tft.print("Binary = "); tft.println((int)fnumber, BIN); // Print as integer value in binary
  //tft.print("Hexadecimal = "); tft.println((int)fnumber, HEX); // Print as integer number in Hexadecimal
  //delay(500);
}

void thresholdControl(float ON, float OFF, float SENSOR_VALUE){
  if(ON <= OFF){
    if(SENSOR_VALUE <= ON){
      digitalWrite(RELAY, 1);
      }
     if (SENSOR_VALUE >= OFF)
     digitalWrite(RELAY, 0);
    }
   else if(ON > OFF){
    if(SENSOR_VALUE > ON){
      digitalWrite(RELAY, 1);
      }
    if(SENSOR_VALUE <= OFF){
      digitalWrite(RELAY, 0);
      }
    }
    else{
      digitalWrite(RELAY, 0);
    }
}

void timerControl(){
    //Serial.println("In timerControl() ...");
    DynamicJsonDocument myTimer(1024);
    deserializeJson(myTimer, timerValve1);
    String weekday;
    switch(DD){
      case 0:
        weekday = "Sunday";
        break;
      case 1:
        weekday = "Monday";
        break;
      case 2:
        weekday = "Tuesday";
        break;
      case 3:
        weekday = "Wednesday";
        break;
      case 4:
        weekday = "Thursday";
        break;
      case 5:
        weekday = "Friday";
        break;
      case 6:
        weekday = "Saturday";
        break;
      }

    for(int i=0; i<3; i++){
      Serial.println("-----------------------------------------");
      if(myTimer["TIMER_VALVE1"][i]["isTimerEnable"] == true){
        Serial.println("Timer #"+ String(i) + " " + "isEnabled ...");
        if(weekday == "Sunday" && myTimer["TIMER_VALVE1"][i]["days"]["sunday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else if(weekday == "Monday" && myTimer["TIMER_VALVE1"][i]["days"]["monday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else if(weekday == "Tuesday" && myTimer["TIMER_VALVE1"][i]["days"]["tuesday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else if(weekday == "Wednesday" && myTimer["TIMER_VALVE1"][i]["days"]["wednesday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else if(weekday == "Thursday" && myTimer["TIMER_VALVE1"][i]["days"]["thursday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else if(weekday == "Friday" && myTimer["TIMER_VALVE1"][i]["days"]["friday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else if(weekday == "Saturday" && myTimer["TIMER_VALVE1"][i]["days"]["saturday"] == true){
          if(int(myTimer["TIMER_VALVE1"][i]["startHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["startMinute"]) == MM){
            digitalWrite(RELAY, 1);
            Serial.println("Relay is Active ...");
          }
          else if(int(myTimer["TIMER_VALVE1"][i]["stopHour"]) == HH && int(myTimer["TIMER_VALVE1"][i]["stopMinute"]) == MM){
            digitalWrite(RELAY, 0);
          }
        }
        else{
          continue;
        }    
       }
     }
}
