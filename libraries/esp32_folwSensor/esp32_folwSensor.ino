#define flow_sensor 18
#define LED_G 2
#define LED_R 3 

void setup() {
  Serial.begin(9600);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(flow_sensor, INPUT);
}

void loop() {
  uint32_t pulse = pulseIn(flow_sensor,HIGH);
  if(pulse < 1){
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    Serial.println("Pulse < 1");
  }

  else{
    float Hz = 1/(2*pulse*pow(10,-6));
    float flow = 7.2725*(float)Hz + 3.2094;
    Serial.print(Hz);
    Serial.print("Hz\t");
    Serial.print(flow/60);
    Serial.println(" L/minute");
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
  }
}
