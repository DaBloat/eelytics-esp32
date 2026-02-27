#include <WiFi.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>

// pins
#define SERVO_LEFT 33
#define SERVO_RIGHT 32

// Servos
Servo servoMotorLeft;
Servo servoMotorRight;

// Clients
WiFiClient espClient;
PubSubClient client(espClient);

//MQTT Connections and Topics
const char* MQTT_BROKER = "192.168.1.220";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "eelytics/servos";
const char* STATUS_TOPIC = "eelytics/esp32/status";

// Wifi Connections
const char* WIFI_PRIMARY = "PASADO_KINSE";
const char* WIFI_PRIMARY_PASS = "traceydee@15";
const char* WIFI_SECONDARY = "VILLAMOR";
const char* WIFI_SECONDARY_PASS = "1nonlyPatatas";

void setupWifi() {
  Serial.println();

  Serial.print("Connecting to Primary Wifi: ");
  Serial.println(WIFI_PRIMARY);
  WiFi.begin(WIFI_PRIMARY, WIFI_PRIMARY_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("\nPrimary Failed, Connecting to Secondary Wifi: ");
    Serial.println(WIFI_SECONDARY);

    WiFi.disconnect();
    delay(1000);

    WiFi.begin(WIFI_SECONDARY, WIFI_SECONDARY_PASS);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nCONNECTED SUCCESSFULLY");
    Serial.print("IP ADDRESS: ");
    Serial.println(WiFi.localIP());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.print("Received from Pi: ");
  Serial.println(message);

  int angleLeft = 0;
  int angleRight = 0;

  // sscanf magically splits "45,0" into angleLeft=45 and angleRight=0
  if (sscanf(message, "%d,%d", &angleLeft, &angleRight) == 2) {
    servoMotorLeft.write(angleLeft);
    servoMotorRight.write(angleRight);
    
    Serial.print("Moved -> Left: "); 
    Serial.print(angleLeft);
    Serial.print(" | Right: "); 
    Serial.println(angleRight);
  } else {
    Serial.println("Error: Could not understand the command.");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT Broker...");
    
    if (client.connect("EelSorter-ESP32", NULL, NULL, STATUS_TOPIC, 0, true, "offline")) {
      Serial.println(" Connected!");
      
      client.publish(STATUS_TOPIC, "online", true);
      
      client.subscribe(MQTT_TOPIC);
    } else {
      Serial.print(" Failed, state: ");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  servoMotorLeft.attach(SERVO_LEFT);
  servoMotorRight.attach(SERVO_RIGHT);

  servoMotorLeft.write(45);
  servoMotorRight.write(0);

  setupWifi();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }
  // This one line handles all incoming messages and keeps the connection alive
  client.loop();
}
