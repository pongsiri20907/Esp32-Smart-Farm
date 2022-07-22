#include <BH1750FVI.h>
BH1750FVI LightSensor(BH1750FVI::k_DevModeContLowRes);
int ledPin = 2;
void setup()
{
pinMode(ledPin, OUTPUT);
Serial.begin(9600);
LightSensor.begin();
}
 
void loop()
{
uint16_t lux = LightSensor.GetLightIntensity();
Serial.print("Light: ");
Serial.print(lux);
Serial.println(" lux");
if (lux < 1000) {
digitalWrite(ledPin, HIGH); // สั่งให้ LED ติดสว่าง
Serial.println("LEDON");
}
if (lux > 1000) {
digitalWrite(ledPin, LOW); // สั่งให้ LED ดับ
Serial.println("LEDOFF");
}
delay(250);
}
