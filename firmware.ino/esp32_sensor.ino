#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <WiFi.h>
#include <PubSubClient.h>

MCUFRIEND_kbv tft;
RTC_DS3231 rtc;
File dataFile;

// --- Color definitions ---
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// --- LCD Pins ---
#define LCD_CS   33
#define LCD_RS   15
#define LCD_WR   4
#define LCD_RD   2
#define LCD_RST  32
#define LCD_D0   12
#define LCD_D1   13
#define LCD_D2   26
#define LCD_D3   25
#define LCD_D4   17
#define LCD_D5   16
#define LCD_D6   27
#define LCD_D7   14

// --- SD ---
#define SD_CS 5

// --- Ultrasonic ---
#define TRIG_PIN 0  
#define ECHO_PIN 34  

// --- Button ---
#define BUTTON_PIN 35

// --- Passive Buzzer ---
#define BUZZER_PIN 9

// --- Variables ---
long duration;
float distance;
bool isPaused = false;

unsigned long lastLogTime = 0;
const long logInterval = 5000; 

// --- WiFi + MQTT ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "172.20.10.2";
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMQTT = 0;
const long mqttInterval = 1000; 

// ---------------- MQTT Callback ----------------
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.trim();

  if (String(topic) == "parking/control") {
    if (msg.equalsIgnoreCase("PAUSE")) {
      isPaused = true;
      Serial.println("Paused via MQTT");
    } else if (msg.equalsIgnoreCase("UNPAUSE")) {
      isPaused = false;
      Serial.println("Unpaused via MQTT");
    }
  }
}

// ---------------- Functions ----------------
void setup_wifi() {
  delay(10);
  Serial.print("Connecting to WiFi ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32_Parking")) {
      Serial.println("MQTT connected");
      client.publish("parking/status", "ESP32 online");
      client.subscribe("parking/control"); 
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2s");
      delay(2000);
    }
  }
}

void publishDistance(float d) {
  if (client.connected()) {
    char buf[16];
    dtostrf(d, 0, 2, buf);
    client.publish("parking/distance", buf);
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);

  // --- LCD INIT ---
  uint16_t ID = tft.readID();
  tft.begin(ID);
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.setCursor(30, 20);
  tft.println("SMART PARKING SYSTEM");
  delay(500);

  // --- RTC INIT ---
  Wire.begin(21, 22);
  if (!rtc.begin()) {
    tft.setCursor(20, 70);
    tft.setTextColor(RED);
    tft.println("RTC not found!");
  } else {
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC was reset. Time auto-set!");
    }
    tft.setCursor(20, 70);
    tft.setTextColor(GREEN);
    tft.println("RTC OK");
  }

  // --- SD INIT ---
  if (!SD.begin(SD_CS)) {
    tft.setCursor(20, 100);
    tft.setTextColor(RED);
    tft.println("SD init failed!");
  } else {
    tft.setCursor(20, 100);
    tft.setTextColor(GREEN);
    tft.println("SD OK");
    dataFile = SD.open("/log.txt", FILE_APPEND);
  }

  // --- Pins ---
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // --- WiFi + MQTT ---
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); 

  delay(2000);
  tft.fillScreen(BLACK);
}

// ---------------- Main Loop ----------------
void loop() {
  // --- MQTT loop ---
  if (!client.connected()) reconnectMQTT();
  client.loop();

  // --- Button ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    isPaused = !isPaused;
    delay(300); // debounce
  }

  // --- RTC Time ---
  DateTime now = rtc.now();

  // --- Ultrasonic ---
  if (!isPaused) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH, 40000);
    distance = duration * 0.034 / 2;
  }

  // --- TFT Display ---
  tft.fillRect(0, 40, 320, 200, BLACK);
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(50, 10);
  tft.println("SMART PARKING SYSTEM");

  tft.setTextColor(YELLOW);
  tft.setTextSize(2);
  tft.setCursor(20, 50);
  tft.printf("Date: %02d/%02d/%04d", now.day(), now.month(), now.year());
  tft.setCursor(20, 75);
  tft.printf("Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second());

  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 110);
  tft.printf("Dist: %.1f cm", distance);

  // --- Parking Status and Buzzer ---
  if (!isPaused) {
    if (distance < 15) { // STOP
      tft.setTextColor(RED);
      tft.setTextSize(4);
      tft.setCursor(100, 150);
      tft.println("STOP!");
      for(int i=0; i<3; i++){
        tone(BUZZER_PIN, 1000, 200);
        delay(250);
      }
    } else if (distance < 40) { 
      tft.setTextColor(YELLOW);
      tft.setTextSize(4);
      tft.setCursor(100, 150);
      tft.println("SLOW");
      tone(BUZZER_PIN, 1000, 200);
      delay(250);
      noTone(BUZZER_PIN);
    } else { // GO
      tft.setTextColor(GREEN);
      tft.setTextSize(4);
      tft.setCursor(120, 150);
      tft.println("GO");
      noTone(BUZZER_PIN);
    }
  } else { // PAUSE
    tft.setTextColor(MAGENTA);
    tft.setTextSize(4);
    tft.setCursor(100, 150);
    tft.println("PAUSE");
    noTone(BUZZER_PIN);
  }

  // --- SD Logging every 5s ---
  unsigned long nowMillis = millis();
  if (nowMillis - lastLogTime > logInterval) {
    lastLogTime = nowMillis;
    if (dataFile) {
      dataFile.printf("%02d/%02d/%04d %02d:%02d:%02d Distance: %.2f cm\n",
                      now.day(), now.month(), now.year(),
                      now.hour(), now.minute(), now.second(),
                      distance);
      dataFile.flush();
    }
  }

  // --- MQTT Publish every 1s ---
  if (nowMillis - lastMQTT > mqttInterval) {
    lastMQTT = nowMillis;
    publishDistance(distance);
  }

  // --- Serial ---
  Serial.printf("%02d/%02d/%04d %02d:%02d:%02d Distance: %.2f cm\n",
                now.day(), now.month(), now.year(),
                now.hour(), now.minute(), now.second(),
                distance);

  delay(1000);
}
