#include <Preferences.h>

Preferences preferences;
unsigned int counter;
void setup() {
  Serial.begin(115200);
  Serial.println();
  preferences.begin("my-app", false);
  counter = preferences.getUInt("counter", 0);
  counter++;
  // Print the counter to Serial Monitor
  Serial.printf("Current counter value: %u\n", counter);
  // Store the counter to the Preferences
  preferences.putUInt("counter", counter);

  // Close the Preferences
  preferences.end();

  // Wait 10 seconds
  Serial.println("Restarting in 10 seconds...");
  delay(10000);

  // Restart ESP
  ESP.restart();
}

void loop() {
  Serial.printf("Interation : %u", counter);
}
