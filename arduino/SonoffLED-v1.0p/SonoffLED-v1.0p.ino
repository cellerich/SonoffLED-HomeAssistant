#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define cLED               12                                           // Don't Change for Sonoff LED
#define sLED               13                                           // Don't Change for Sonoff LED)
#define wLED               14                                           // Don't Change for Sonoff LED)
   
#define MQTT_CLIENT        "Sonoff_LED_strip_v1.0p"                     // mqtt client_id (Must be unique for each Sonoff)
#define MQTT_SERVER        "192.168.1.100"                              // mqtt server
#define MQTT_PORT          1883                                         // mqtt port
#define MQTT_TOPIC         "homenet/sonoff_led/living_room"             // mqtt topic (Base topic)
#define MQTT_BRIGHT_TOPIC  "homenet/sonoff_led/living_room/brightness"  // mqtt topic (Brightness topic)
#define MQTT_COLOR_TOPIC   "homenet/sonoff_led/living_room/color"       // mqtt topic (Color topic)
#define MQTT_USER          "user"                                       // mqtt user
#define MQTT_PASS          "pass"                                       // mqtt password

#define WIFI_SSID          "homepass"                                   // wifi ssid
#define WIFI_PASS          "homewifi"                                   // wifi password

#define VERSION    "\n\n------------------  Sonoff LED Strip v1.0p  --------------------"

extern "C" { 
  #include "user_interface.h" 
}

bool debug = false;                                                     // True for easy access to serial window at startup
bool onAtStart = false;                                                 // True to turn on LED as soon as power is applied
bool c_enable = true;
bool w_enable = true;

int kRetries = 10;
int kUpdFreq = 1;
int bright = 255;
int color = 328;
int fadeDelay = 0;                                                      // 0 for no fade, 1 to 5 for delay to fade up & down
int nBright = 255;

unsigned long TTasks;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient, MQTT_SERVER, MQTT_PORT);

void callback(const MQTT::Publish& pub) {
  blinkLED(sLED, 100, 1);
  if (pub.topic() == MQTT_TOPIC) {
    mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/stat", pub.payload_string()).set_retain().set_qos(1));
    if (pub.payload_string() == "OFF") {
      analogWrite(cLED, 0);
      analogWrite(wLED, 0);
      Serial.println("LED Status . . . . . . . . . . . . . . . . . . . . . . . . . OFF");
    }
    else if (pub.payload_string() == "ON") {
      if (c_enable) {
        analogWrite(cLED, bright);
      }
      if (w_enable) {
        analogWrite(wLED, bright);
      }
      Serial.println("LED Status . . . . . . . . . . . . . . . . . . . . . . . . . ON");
    }
    else if (pub.payload_string() == "reset") {
      blinkLED(sLED, 400, 4);
      ESP.restart();
    }
  }
  else if (pub.topic() == MQTT_BRIGHT_TOPIC) {
    mqttClient.publish(MQTT::Publish(MQTT_BRIGHT_TOPIC"/stat", pub.payload_string()).set_retain().set_qos(1));
    nBright = pub.payload_string().toInt();
    if (nBright < bright) {
      while(nBright < bright) {
        if (c_enable) {
          analogWrite(cLED, bright);
        }
        if (w_enable) {
          analogWrite(wLED, bright);
        }
        bright--;
        delay(fadeDelay);
      }
    }
    else if (nBright > bright) {
      while(nBright > bright) {
        if (c_enable) {
          analogWrite(cLED, bright);
        }
        if (w_enable) {
          analogWrite(wLED, bright);
        }
        bright++;
        delay(fadeDelay);
      }
    }
    Serial.print("Brightness Level . . . . . . . . . . . . . . . . . . . . . . ");
    Serial.println(bright);
  }
  else if (pub.topic() == MQTT_COLOR_TOPIC) {
    mqttClient.publish(MQTT::Publish(MQTT_COLOR_TOPIC"/stat", pub.payload_string()).set_retain().set_qos(1));
    color = pub.payload_string().toInt();
    if (color >= 154 && color < 269) {
       w_enable = false;
       c_enable = true;
       analogWrite(wLED, 0);       
       analogWrite(cLED, bright);
       Serial.println("LED Colour . . . . . . . . . . . . . . . . . . . . . . . . . COLD");
    }
    else if (color >= 270 && color < 384) {
       w_enable = true;
       c_enable = true;
       analogWrite(wLED, bright);
       analogWrite(cLED, bright);
       Serial.println("LED Colour . . . . . . . . . . . . . . . . . . . . . . . . . BOTH");
    }
    else if (color >= 385 && color <= 500) {
       w_enable = true;
       c_enable = false;
       analogWrite(wLED, bright);
       analogWrite(cLED, 0);
       Serial.println("LED Colour . . . . . . . . . . . . . . . . . . . . . . . . . WARM");
    }
  }
}

void setup() {
  pinMode(cLED, OUTPUT);
  pinMode(sLED, OUTPUT);
  pinMode(wLED, OUTPUT);
  if (onAtStart) {
    analogWrite(cLED, bright);
    analogWrite(wLED, bright);
  }
  mqttClient.set_callback(callback);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.begin(115200);
  if (debug) {
   blinkLED(sLED, 25, 120); 
  }
  digitalWrite(sLED, HIGH);
  Serial.println(VERSION);
  Serial.print("\nESP ChipID: ");
  Serial.print(ESP.getChipId(), HEX);
  Serial.print("\nConnecting to "); Serial.print(WIFI_SSID); Serial.print(" Wifi"); 
  while ((WiFi.status() != WL_CONNECTED) && kRetries --) {
    delay(500);
    Serial.print(" .");
  }
  if (WiFi.status() == WL_CONNECTED) {  
    Serial.println(" DONE");
    Serial.print("IP Address is: "); Serial.println(WiFi.localIP());
    Serial.print("Connecting to ");Serial.print(MQTT_SERVER);Serial.print(" Broker . .");
    delay(1000);
    while (!mqttClient.connect(MQTT::Connect(MQTT_CLIENT).set_keepalive(90).set_auth(MQTT_USER, MQTT_PASS)) && kRetries --) {
      Serial.print(" .");
      delay(1000);
    }
    if(mqttClient.connected()) {
      Serial.println(" DONE");
      Serial.println("\n----------------------------  Logs  ----------------------------");
      Serial.println();
      mqttClient.subscribe(MQTT_TOPIC, 1);
      mqttClient.subscribe(MQTT_BRIGHT_TOPIC, 1);
      mqttClient.subscribe(MQTT_COLOR_TOPIC, 1);
      blinkLED(sLED, 40, 8);
      digitalWrite(sLED, LOW);
      mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/stat", "OFF").set_retain().set_qos(1));
      mqttClient.publish(MQTT::Publish(MQTT_BRIGHT_TOPIC"/stat", "255").set_retain().set_qos(1));
      mqttClient.publish(MQTT::Publish(MQTT_COLOR_TOPIC"/stat", "328").set_retain().set_qos(1));
      if (onAtStart) {
        mqttClient.publish(MQTT::Publish(MQTT_TOPIC"/stat", "ON").set_retain().set_qos(1));
      }
    }
    else {
      Serial.println(" FAILED!");
      Serial.println("\n----------------------------------------------------------------");
      Serial.println();
    }
  }
  else {
    Serial.println(" WiFi FAILED!");
    Serial.println("\n----------------------------------------------------------------");
    Serial.println();
  }
}

void blinkLED(int pin, int del, int n) {             
  for(int i=0; i<n; i++)  {  
    digitalWrite(pin, HIGH);        
    delay(del);
    digitalWrite(pin, LOW);
    delay(del);
  }
}

void checkConnection() {
  if (WiFi.status() == WL_CONNECTED)  {
    if (mqttClient.connected()) {
      Serial.println("mqtt broker connection . . . . . . . . . . . . . . . . . . . OK");
    } 
    else {
      Serial.println("mqtt broker connection . . . . . . . . . . . . . . . . . . LOST");
      blinkLED(sLED, 400, 4);
      ESP.restart();
    }
  }
  else { 
    Serial.println("WiFi Access Point . . . . . . . . . . . . . . . . . . . . LOST");
    blinkLED(sLED, 400, 4);
    ESP.restart();
  }
}

void loop() { 
  mqttClient.loop();
  timedTasks();
}

void timedTasks() {
  if ((millis() > TTasks + (kUpdFreq*60000)) || (millis() < TTasks)) { 
    TTasks = millis();
    checkConnection();
  }
}
