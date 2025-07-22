
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <time.h>

#define WIFI_SSID "KFON_ACV_@_JACK"
#define WIFI_PASSWORD "JILL@JILL"
#define API_KEY "AIzaSyBQyZmatpVDg30nIRhbMJl8qo9K7s7JANY"
#define DATABASE_URL "https://njoyfarm-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

bool lastDryRunStatus = false;
bool lastMotorRunningStatus = false;  

#define MOTOR_START_RELAY 13
#define MOTOR_STOP_RELAY 12
#define AUTO 14
#define MANUAL 27
#define MOTOR_STATUS_PIN 26
#define V1_PIN 25
#define V2_PIN 33
#define V3_PIN 32
#define V4_PIN 22
#define V5_PIN 23
#define V6_PIN 5
#define V7_PIN 18
#define V8_PIN 19
#define V9_PIN 21
#define V10_PIN 22
#define WIFI_LED_PIN 2
#define FIREBASE_LED_PIN 4

struct Device {
  const char* name;
  int pin;
  bool isOn = false;
  unsigned long startMillis = 0;
  unsigned long durationMillis = 0;

  bool shouldTurnOff = false;

  bool on = false;
  bool timerenabled = false;
  int timer = 0;
};

Device devices[] = {
  {"AUTO", AUTO},{"MANUAL",MANUAL}, {"V1", V1_PIN}, {"V2", V2_PIN}, {"V3", V3_PIN},
  {"V4", V4_PIN}, {"V5", V5_PIN}, {"V6", V6_PIN}, {"V7", V7_PIN},
  {"V8", V8_PIN}, {"V9", V9_PIN}, {"V10", V10_PIN}
};

unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL = 3000;

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void setupTime() {
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
}

String getCurrentTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Error";

  char buffer[10];
  strftime(buffer, sizeof(buffer), "%I:%M %p", &timeinfo);
  return String(buffer);
}



void setupfirebase(){
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void logToFirebase(const String& msg) {
  String time = getCurrentTimeString();
  String logMsg = time+" : "+ msg;
  Firebase.RTDB.pushString(&fbdo, "logs", logMsg);
  Serial.println(logMsg);
}

void fetchControlData() {
  FirebaseJson json;
  if (!Firebase.RTDB.getJSON(&fbdo, "control") || fbdo.dataType() != "json") return;
  json = fbdo.jsonObject();
  FirebaseJsonData result;

  for (auto& dev : devices) {
    String base = String(dev.name);
    if (json.get(result, base + "/on")) dev.on = result.boolValue;
    if (json.get(result, base + "/timerenabled")) dev.timerenabled = result.boolValue;
    if (json.get(result, base + "/timer")) dev.timer = result.intValue;
  }
}

void setup() {
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(FIREBASE_LED_PIN, OUTPUT);
  pinMode(MOTOR_STATUS_PIN, INPUT_PULLUP);
  pinMode(MOTOR_START_RELAY, OUTPUT);
  pinMode(MOTOR_STOP_RELAY, OUTPUT);
  digitalWrite(MOTOR_START_RELAY, HIGH);
  digitalWrite(MOTOR_STOP_RELAY, LOW);

  Serial.begin(115200);
  setupWiFi(); setupTime();
  struct tm timeinfo;
while (!getLocalTime(&timeinfo)) {
  Serial.println("Waiting for time sync...");
  delay(500);
}
   setupfirebase();
  for (auto& dev : devices) {
    pinMode(dev.pin, OUTPUT);
    digitalWrite(dev.pin, HIGH);
  }
}

void loop() {
  //For Reconnection
if (WiFi.status() != WL_CONNECTED) {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("WiFi reconnected...");
  delay(3000);
}

  digitalWrite(WIFI_LED_PIN, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  digitalWrite(FIREBASE_LED_PIN, (Firebase.ready() && signupOK) ? HIGH : LOW);


  if (millis() - lastFetch > FETCH_INTERVAL) {
    fetchControlData();
    lastFetch = millis();
  }
    if (!Firebase.ready() || !signupOK) {
    setupfirebase();
  };

  String time = getCurrentTimeString();

  bool checkRequested = false;
  if (Firebase.RTDB.getBool(&fbdo, "status/check") && fbdo.dataType() == "boolean") {
    checkRequested = fbdo.boolData();
  }

  if (checkRequested) {
    Firebase.RTDB.setString(&fbdo, "status/response", getCurrentTimeString());

    Firebase.RTDB.setBool(&fbdo, "status/check", false);

    logToFirebase("Status is Live");
  }

  for (auto& dev : devices) {
    String base = "control/" + String(dev.name);

    if ((String(dev.name) == "MANUAL" || String(dev.name) == "AUTO") && dev.on) {
      bool motorActuallyOn = digitalRead(MOTOR_STATUS_PIN) == LOW;
      if (dev.isOn && !motorActuallyOn) {
        // Motor was commanded ON, but dry run unit has cut it OFF
        Firebase.RTDB.setBool(&fbdo, "dryrun", true);
        if (!lastDryRunStatus || !lastMotorRunningStatus) {
          logToFirebase("Fault Detected!");
          lastDryRunStatus = true;
          lastMotorRunningStatus = true;
        }
      } else if (dev.isOn && motorActuallyOn) {
        Firebase.RTDB.setBool(&fbdo, "dryrun", false);
            if (lastDryRunStatus || !lastMotorRunningStatus) {
              logToFirebase("Running OK");
              lastDryRunStatus = false;
              lastMotorRunningStatus = true;
            }
      } else {
        Firebase.RTDB.setBool(&fbdo, "dryrun", false);
            lastDryRunStatus = true;
      }
    }

    if (dev.on && !dev.isOn) {
      if (String(dev.name) == "MANUAL") {
        digitalWrite(AUTO,HIGH);
        Firebase.RTDB.setBool(&fbdo, "control/AUTO/on", false);
        digitalWrite(MANUAL,LOW);
        delay(1000);
          digitalWrite(MOTOR_START_RELAY, LOW);
          delay(1000);
          digitalWrite(MOTOR_START_RELAY, HIGH);
          lastMotorRunningStatus = false;
      } else if(String(dev.name) == "AUTO") {
        digitalWrite(MOTOR_STOP_RELAY, HIGH);
        delay(1000);
        digitalWrite(MOTOR_STOP_RELAY, LOW);
        delay(1000);
        digitalWrite(MANUAL,HIGH);
        digitalWrite(AUTO,LOW);
        Firebase.RTDB.setBool(&fbdo, "control/MANUAL/on", false);
        lastMotorRunningStatus = false;
      }else {
        digitalWrite(dev.pin, LOW);
      }
      dev.isOn = true;
      dev.startMillis = millis();
      Firebase.RTDB.setString(&fbdo, base + "/ontime", time);
      if(dev.timerenabled){
              dev.durationMillis = dev.timer * 60000UL;
              logToFirebase( String(dev.name)+ " ON for "+dev.timer+" min");
      }
      else{
              dev.durationMillis = 0;
              logToFirebase( String(dev.name) +" ON");
      }
    }

    if (!dev.on && dev.isOn) {
      if (String(dev.name) == "MANUAL") {
          digitalWrite(MOTOR_STOP_RELAY, HIGH);
          delay(1000);
          digitalWrite(MOTOR_STOP_RELAY, LOW);
          delay(1000);
          digitalWrite(MANUAL,HIGH);
      } else if(String(dev.name) == "AUTO") {
        digitalWrite(AUTO, HIGH);
      }
      else {
        digitalWrite(dev.pin, HIGH);
      }
      dev.isOn = false;
      logToFirebase( String(dev.name) + " OFF");
    }
    if (dev.isOn && dev.timerenabled && dev.durationMillis > 0) {
      if (millis() - dev.startMillis >= dev.durationMillis) {
        digitalWrite(dev.pin, HIGH);
        dev.isOn = false;
        dev.on=false;
        if (WiFi.status() == WL_CONNECTED) {
            Firebase.RTDB.setBool(&fbdo, base + "/on", false);
        } else {
            dev.shouldTurnOff = true;
        }

        logToFirebase( String(dev.name) +" OFF by timer");
      }
    }
    if (dev.shouldTurnOff && WiFi.status() == WL_CONNECTED) {
        if (Firebase.RTDB.setBool(&fbdo, base + "/on", false)) {
            dev.shouldTurnOff = false; 
        }
    }
  }
}
