#include <WiFi.h>                    // if ESP32
#include "EspMQTTClient.h"           // https://github.com/plapointe6/EspMQTTClient
#include <ArduinoJson.h>

char payload[128];

EspMQTTClient client(
  "pbk",
  "12345678",
  "mqtt-iarc.eiotsoft.net",  // MQTT Broker server ip
  "iarc",   // Can be omitted if not needed
  "mqtt-iarc",   // Can be omitted if not needed
  "TestClient"      // Client name that uniquely identify your device
);

//
//const char* ssid = "pbk";        // You need to put your own WiFi name, DON'T LEAVE SPACE CAUSE CANNOT CONNECT WIFI
//const char* password = "12345678";     //  You need to put your own WiFi password

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
}

void blink(){
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
  }
  
void onConnectionEstablished() {
  client.subscribe("mytopic/test", [] (const String &payload)  {
    Serial.println("Subscribe:" + payload);
  });
  client.publish("mytopic/test", "Test esp32 send mqtt !");
}

void loop() {
  // Parse to JSON for sned mqtt
  DynamicJsonDocument doc(1024);
  JsonObject object = doc.to<JsonObject>();
  object["TEMP_1"] = random(20, 30);
  object["HUM_1"]  = random(70, 80);
  object["LUX_1"]  = random(100, 300);
  object["FLOW_1"] = random(5);
  serializeJson(doc, payload);
  // Send mqtt
  client.publish("mytopic/test", String(payload));
  client.loop();
  // Test board
  blink();
  
}
