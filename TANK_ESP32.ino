#define TRIG_PI>N 5
#define ECHO_PIN 18
#define MAX_LEVEL 13.0
#define RELAY_FILL   16
#define RELAY_DRAIN  27
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// --- NEW MAINTENANCE LOGIC ---
const float TARGET_LEVEL_CM = 5.50;
const float DEADBAND_CM     = 0.20;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  digitalWrite(RELAY_FILL, RELAY_OFF);
  digitalWrite(RELAY_DRAIN, RELAY_OFF);
  pinMode(RELAY_FILL, OUTPUT);
  pinMode(RELAY_DRAIN, OUTPUT);

  Serial.println("Water Level Controller: 5.5cm Target READY");
}

float getRawDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  
  if (duration == 0) return 999.0; 
  return (duration * 0.0343) / 2.0;
}

float getFilteredDistance() {
  const int numReadings = 5;
  float readings[numReadings];

  for (int i = 0; i < numReadings; i++) {
    readings[i] = getRawDistance();
    delay(10); 
  }

  for (int i = 0; i < numReadings - 1; i++) {
    for (int j = i + 1; j < numReadings; j++) {
      if (readings[i] > readings[j]) {
        float temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  return readings[numReadings / 2]; 
}

void loop() {
  float waterLevelDistance = getFilteredDistance();
  
  if (waterLevelDistance < 999.0) {
    float trueWaterLevel = MAX_LEVEL - waterLevelDistance;
    trueWaterLevel = constrain(trueWaterLevel, 0.0, MAX_LEVEL);
    
    Serial.print("Current Level: ");
    Serial.print(trueWaterLevel);
    Serial.println(" cm");

    if (trueWaterLevel < (TARGET_LEVEL_CM - DEADBAND_CM)) {
      digitalWrite(RELAY_FILL, RELAY_ON);
      digitalWrite(RELAY_DRAIN, RELAY_OFF);
      Serial.println("Action: FILLING");
      
    } else if (trueWaterLevel > (TARGET_LEVEL_CM + DEADBAND_CM)) {
      digitalWrite(RELAY_DRAIN, RELAY_ON);
      digitalWrite(RELAY_FILL, RELAY_OFF);
      Serial.println("Action: DRAINING");
      
    } else {
      digitalWrite(RELAY_FILL, RELAY_OFF);
      digitalWrite(RELAY_DRAIN, RELAY_OFF);
      Serial.println("Action: LEVEL MAINTAINED");
    }

    Serial.println("----------------------");
  } else {
    digitalWrite(RELAY_FILL, RELAY_OFF);
    digitalWrite(RELAY_DRAIN, RELAY_OFF);
    Serial.println("Sensor timeout! Relays forced OFF for safety.");
  }
  
  delay(1000); 
}