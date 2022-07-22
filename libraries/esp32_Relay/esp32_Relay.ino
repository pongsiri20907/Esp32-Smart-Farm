#define Relay 5

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(Relay, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(Relay, HIGH);
  delay(3000);
  digitalWrite(Relay, LOW);
  delay(3000);
}
