#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// --- Configuration ---
const char* WIFI_SSID = "PASADO_KINSE";
const char* WIFI_PASSWORD = "traceydee@15";

// Your Raspberry Pi's IP address (which is running the MQTT broker)
const char* MQTT_BROKER = "192.168.1.220"; 
const int MQTT_PORT = 1883;

// --- <-- NEW: Topic and Reset Configuration --> ---
const char* MQTT_TOPIC = "eelytics/servos";         // Main topic from RPi
const char* MQTT_TEST_TOPIC = "eelytics/servos/test"; // New test topic

#define DEFAULT_ANGLE_LEFT  45  // Default "home" position
#define DEFAULT_ANGLE_RIGHT 0   // Default "home" position
#define RESET_DELAY_MS 5000   // 5000ms = 5 seconds to wait before resetting
// --- <-- End New Config --> ---

// --- Servo Pins (from your image) ---
#define SERVO_PIN_RIGHT 32
#define SERVO_PIN_LEFT  33

// Create servo objects
Servo servoMotorLeft;
Servo servoMotorRight;

// Create WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// --- <-- NEW: Global variables for the reset timer --> ---
unsigned long lastMoveTime = 0;   // Stores the time of the last command
bool isWaitingToReset = false; // Flag to track if we need to reset
// --- <-- End New Globals --> ---

/**
 * MQTT Callback Function
 * This is the "brain". It runs every time a message is received.
 */
void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to a string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  String messageStr = String(message);
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(messageStr);

  int angleLeft = 0;
  int angleRight = 0;

  // 1. Parse the servo command. Both topics use the same "left,right" format.
  if (sscanf(message, "%d,%d", &angleLeft, &angleRight) != 2) {
    Serial.println("Failed to parse servo command.");
    return; // Exit if format is bad
  }
  
  // 2. Move the servos to the commanded position
  Serial.print("Setting Left: "); Serial.print(angleLeft);
  Serial.print(", Right: "); Serial.println(angleRight);
  servoMotorLeft.write(angleLeft);
  servoMotorRight.write(angleRight);

  // 3. Check WHICH topic it came from to decide on the reset
  String topicStr = String(topic);
  if (topicStr == MQTT_TOPIC) {
    // This is the automatic topic from the Pi. Start the auto-reset timer.
    Serial.println("Starting auto-reset timer.");
    lastMoveTime = millis();     // <-- NEW: Record the time
    isWaitingToReset = true;  // <-- NEW: Set the flag
  } else if (topicStr == MQTT_TEST_TOPIC) {
    // This is the manual test topic. Do NOT reset.
    Serial.println("Test command received. Holding position.");
    isWaitingToReset = false; // <-- NEW: Cancel any pending reset
  }
}

/**
 * Connects to WiFi
 */
void setupWifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * Reconnects to MQTT broker if connection is lost
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("EelSorter-ESP32")) {
      Serial.println("connected");
      // Subscribe to the main topic
      client.subscribe(MQTT_TOPIC);
      Serial.print("Subscribed to topic: ");
      Serial.println(MQTT_TOPIC);
      
      // <-- NEW: Subscribe to the test topic -->
      client.subscribe(MQTT_TEST_TOPIC);
      Serial.print("Subscribed to topic: ");
      Serial.println(MQTT_TEST_TOPIC);
      // <-- End New -->

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/**
 * Main Setup
 */
void setup() {
  Serial.begin(115200);
  
  // Attach servos
  servoMotorLeft.attach(SERVO_PIN_LEFT);
  servoMotorRight.attach(SERVO_PIN_RIGHT);

  // <-- CHANGED: Set servos to the default home position on startup -->
  Serial.println("Setting servos to default position.");
  servoMotorLeft.write(DEFAULT_ANGLE_LEFT);
  servoMotorRight.write(DEFAULT_ANGLE_RIGHT);
  
  setupWifi();
  
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback); // Set the "brain" function
}

/**
 * Main Loop
 * This loop must be non-blocking (no long delays!)
 */
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // This function processes incoming MQTT messages

  // --- <-- NEW: Auto-Reset Timer Logic --> ---
  if (isWaitingToReset) {
    // Check if the reset delay has passed
    if (millis() - lastMoveTime > RESET_DELAY_MS) {
      Serial.println("Reset delay finished. Returning to default position.");
      
      // Move servos back to default
      servoMotorLeft.write(DEFAULT_ANGLE_LEFT);
      servoMotorRight.write(DEFAULT_ANGLE_RIGHT);
      
      // Clear the flag
      isWaitingToReset = false; 
    }
  }
  // --- <-- End New Timer Logic --> ---
}