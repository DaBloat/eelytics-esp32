#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Hardware Pins ---
#define TRIG_PIN 5
#define ECHO_PIN 18
#define MAX_LEVEL 13.0     // Your calibrated max depth
#define RELAY_FILL 16
#define RELAY_DRAIN 27
#define RELAY_ON LOW       // Active Low Relays
#define RELAY_OFF HIGH

// --- MQTT Configuration ---
const char* STATUS_TOPIC = "eelytics/tank/status";
const char* CONTROL_TOPIC = "eelytics/tank/control";
const char* MQTT_SERVER = "192.168.1.220"; 

// --- Control Variables ---
float TARGET_LEVEL_CM = 5.50;
String currentMode = "NONE"; 
const float DEADBAND_CM = 0.20;

// --- WiFi Credentials ---
const char* WIFI_PRIMARY = "PASADO_KINSE";
const char* WIFI_PRIMARY_PASS = "traceydee@15";
const char* WIFI_SECONDARY = "VILLAMOR";
const char* WIFI_SECONDARY_PASS = "1nonlyPatatas";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

// --- WiFi Setup with Failover ---
void setupWifi() {
  Serial.println("\n--- WiFi Setup ---");
  Serial.print("Connecting to Primary: "); Serial.println(WIFI_PRIMARY);
  WiFi.begin(WIFI_PRIMARY, WIFI_PRIMARY_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("\nPrimary Failed. Trying Secondary: "); Serial.println(WIFI_SECONDARY);
    WiFi.disconnect(); 
    delay(1000);
    WiFi.begin(WIFI_SECONDARY, WIFI_SECONDARY_PASS);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500); Serial.print("."); attempts++;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi CONNECTED SUCCESSFULLY");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
  }
}

// --- MQTT Callback: Receives Commands from Raspberry Pi ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n[MQTT Incoming] Topic: "); Serial.println(topic);
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON Parse Failed: "); Serial.println(error.c_str());
    return;
  }

  // Update Maintain Goal
  if (doc.containsKey("maintain")) {
    TARGET_LEVEL_CM = doc["maintain"];
    Serial.print("New Maintain Goal: "); Serial.print(TARGET_LEVEL_CM); Serial.println(" cm");
  }

  // Update Mode
  if (doc.containsKey("mode")) {
    currentMode = doc["mode"].as<String>();
    Serial.print("New Mode Set: "); Serial.println(currentMode);
  }
}

// --- MQTT Connection Handling ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection to "); Serial.println(MQTT_SERVER);
    // Connect as a unique client ID
    if (client.connect("ESP32_Tank_Node")) {
      Serial.println("MQTT CONNECTED");
      client.subscribe(CONTROL_TOPIC); 
      Serial.print("Subscribed to: "); Serial.println(CONTROL_TOPIC);
    } else {
      Serial.print("Failed, rc="); Serial.print(client.state());
      Serial.println(" retrying in 5s...");
      delay(5000);
    }
  }
}

// --- Sensor Filtering (Median Filter) ---
float getFilteredDistance() {
  const int numReadings = 5;
  float readings[numReadings];
  for (int i = 0; i < numReadings; i++) {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    readings[i] = (duration == 0) ? 999.0 : (duration * 0.0343) / 2.0;
    delay(10);
  }
  // Simple Sort for Median
  for (int i = 0; i < numReadings - 1; i++) {
    for (int j = i + 1; j < numReadings; j++) {
      if (readings[i] > readings[j]) { 
        float temp = readings[i]; readings[i] = readings[j]; readings[j] = temp; 
      }
    }
  }
  return readings[numReadings / 2];
}

void setup() {
  Serial.begin(115200);
  
  // Pin Modes
  pinMode(TRIG_PIN, OUTPUT); 
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_FILL, OUTPUT); 
  pinMode(RELAY_DRAIN, OUTPUT);
  
  // Start with Relays OFF
  digitalWrite(RELAY_FILL, RELAY_OFF); 
  digitalWrite(RELAY_DRAIN, RELAY_OFF);

  setupWifi();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop(); // Process incoming MQTT messages

  unsigned long now = millis();
  if (now - lastMsg > 2000) { // Every 2 seconds
    lastMsg = now;
    
    float distance = getFilteredDistance();
    float trueLevel = (distance < 999.0) ? constrain(MAX_LEVEL - distance, 0.0, MAX_LEVEL) : 0;
    String action = "IDLE";

    // --- State & Relay Logic ---
    if (currentMode == "AUTO") {
      if (trueLevel < (TARGET_LEVEL_CM - DEADBAND_CM)) {
        digitalWrite(RELAY_FILL, RELAY_ON); 
        digitalWrite(RELAY_DRAIN, RELAY_OFF);
        action = "FILLING";
      } else if (trueLevel > (TARGET_LEVEL_CM + DEADBAND_CM)) {
        digitalWrite(RELAY_DRAIN, RELAY_ON); 
        digitalWrite(RELAY_FILL, RELAY_OFF);
        action = "DRAINING";
      } else {
        digitalWrite(RELAY_FILL, RELAY_OFF); 
        digitalWrite(RELAY_DRAIN, RELAY_OFF);
        action = "MAINTAINED";
      }
    } else {
      // Force Safety Off for MANUAL, NONE, or IDLE
      digitalWrite(RELAY_FILL, RELAY_OFF); 
      digitalWrite(RELAY_DRAIN, RELAY_OFF);
      action = "MANUAL_STOP";

      // Explicitly set display modes for the App
      if (currentMode == "NONE") currentMode = "IDLE"; 
      else currentMode = "WAITING"; 
    }

    // --- Serial Debugging ---
    Serial.println("\n--- TANK STATUS ---");
    Serial.print("Mode:   "); Serial.println(currentMode);
    Serial.print("Level:  "); Serial.print(trueLevel); Serial.println(" cm");
    Serial.print("Action: "); Serial.println(action);

    // --- Publish Status (JSON) back to MQTT ---
    StaticJsonDocument<256> statusDoc;
    statusDoc["water_level"] = trueLevel;
    statusDoc["action"] = action;
    statusDoc["mode"] = currentMode; // Confirming mode back to Raspi/App
    
    char buffer[256];
    serializeJson(statusDoc, buffer);
    client.publish(STATUS_TOPIC, buffer);
    
    Serial.print("Published to MQTT: "); Serial.println(buffer);
    Serial.println("--------------------");
  }
}