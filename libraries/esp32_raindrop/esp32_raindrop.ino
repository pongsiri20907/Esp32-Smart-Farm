int ledPin = 13;
int analogPin = 4; //ประกาศตัวแปร ให้ analogPin แทนขา analog ขาที่5
int val = 0;
void setup() {
pinMode(ledPin, OUTPUT); // sets the pin as output
Serial.begin(9600);
}
 
void loop() {
val = analogRead(analogPin); //อ่านค่าสัญญาณ analog ขา5
Serial.print("val = "); // พิมพ์ข้อมความส่งเข้าคอมพิวเตอร์ "val = "
Serial.println(val); // พิมพ์ค่าของตัวแปร val
if (val < 500) { // สามารถกำหนดปรับค่าได้ตามสถานที่ต่างๆ
digitalWrite(ledPin, HIGH); // สั่งให้ LED ติดสว่าง
}
else {
digitalWrite(ledPin, LOW); // สั่งให้ LED ดับ
}
delay(100);
}
