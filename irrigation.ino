#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <time.h>

// Your WiFi and Firebase credentials
#define WIFI_SSID "Deep"
#define WIFI_PASSWORD "Sudeepan"
#define API_KEY "AIzaSyAmyCmpEqPNl8ZJljAQ4Hsi3kHcsCT290I"
#define DATABASE_URL "https://iotirrigation-fcd5a-default-rtdb.firebaseio.com/"  // must end with '/'

// Firebase setup
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

bool lastDryRunStatus = false;
bool lastMotorRunningStatus = false;


// IST time setup
#define GMT_OFFSET_SEC 19800
#define NTP_SERVER "pool.ntp.org"

#define MOTOR_START_RELAY 23  
#define MOTOR_STOP_RELAY 21  
#define MOTOR_PIN 14
#define V1_PIN 13
#define V2_PIN 12
#define V3_PIN 14
#define V4_PIN 27
#define V5_PIN 26
#define V6_PIN 25
#define V7_PIN 33
#define V8_PIN 32
#define V9_PIN 35
#define V10_PIN 34
#define DRP_MODE_RELAY 22
#define WIFI_LED_PIN 2 
#define FIREBASE_LED_PIN 4     // Firebase status LED
#define DRY_RUN_STATUS_PIN 15  // Connect this to the relay output of dry run unit



struct Device {
  const char* name;
  int pin;
  bool isOn = false;
  unsigned long startMillis = 0;
  unsigned long durationMillis = 0;
};

Device devices[] = {
  {"motor", MOTOR_PIN},
  {"v1", V1_PIN},
  {"v2", V2_PIN},
  {"v3", V3_PIN},
  {"v4", V4_PIN},
  {"v5", V5_PIN},
  {"v6", V6_PIN},
  {"v7", V7_PIN},
  {"v8", V8_PIN},
  {"v9", V9_PIN},
  {"v10", V10_PIN}
};

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWifi connected :");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void setupTime() {
  configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);
}

int getCurrentTimeHHMM() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return -1;
  return timeinfo.tm_hour * 100 + timeinfo.tm_min;
}

void setupfirebase(){
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  if(Firebase.signUp(&config, &auth,"","")){
    Serial.println("Signup OK");
    signupOK = true;
  }else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}
void pressRelay(int pin) {
  digitalWrite(pin, LOW);
  delay(1000); // Simulate 1-sec press
  digitalWrite(pin, HIGH);
}


void setup() {
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(FIREBASE_LED_PIN, OUTPUT);
  pinMode(DRY_RUN_STATUS_PIN, INPUT); // Use INPUT_PULLUP if needed
  pinMode(DRP_MODE_RELAY, OUTPUT);
  digitalWrite(DRP_MODE_RELAY, HIGH); // Start in Manual mode
  pinMode(MOTOR_START_RELAY, OUTPUT);
pinMode(MOTOR_STOP_RELAY, OUTPUT);
digitalWrite(MOTOR_START_RELAY, HIGH);
digitalWrite(MOTOR_STOP_RELAY, HIGH);

  Serial.begin(115200);
  setupWiFi();
  setupTime();
  setupfirebase();
  // Setup GPIOs
  for (auto& dev : devices) {
    pinMode(dev.pin, OUTPUT);
    digitalWrite(dev.pin, HIGH);
  }

}
void logToFirebase(const String& msg) {
  int hhmm = getCurrentTimeHHMM();
  String logMsg = "[" + String(hhmm) + "] " + msg;
  Firebase.RTDB.pushString(&fbdo, "logs", logMsg);
  Serial.println(logMsg);
}

void loop() {
  // --- LED: WiFi Status ---
  digitalWrite(WIFI_LED_PIN, WiFi.status() == WL_CONNECTED ? LOW : HIGH);

  // --- LED: Firebase Status ---
  digitalWrite(FIREBASE_LED_PIN, (Firebase.ready() && signupOK) ? LOW : HIGH);

  if (Firebase.ready() && signupOK && (millis()-sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
  } else {
    Serial.println("Firebase NOT ready");
  }


  bool checkRequested = false;
  if (Firebase.RTDB.getBool(&fbdo, "status/check") && fbdo.dataType() == "boolean") {
    checkRequested = fbdo.boolData();
  }
  Serial.print("Check value: ");
  Serial.println(checkRequested);

  if (checkRequested) {
    // Send back a response
    String responseMsg = String(getCurrentTimeHHMM());
    Firebase.RTDB.setString(&fbdo, "status/response", responseMsg);

    // Reset check to false
    Firebase.RTDB.setBool(&fbdo, "status/check", false);

    logToFirebase("Check requested → responded to Firebase");
  }


  for (auto& dev : devices) {
    String base = "Control/" + String(dev.name);
    bool on = false;
    bool timerenabled = false;
    int timer = 0;
    bool scheduleenabled = false;
    int schedule = 0;


    // Fetch all relevant values
    if (Firebase.RTDB.getBool(&fbdo, base + "/on")) {
      on = fbdo.boolData();
    }

    if (Firebase.RTDB.getBool(&fbdo, base + "/timerenabled")) {
      timerenabled = fbdo.boolData();
    }

    if (Firebase.RTDB.getInt(&fbdo, base + "/timer")) {
      timer = fbdo.intData(); // in minutes
    }

    if (Firebase.RTDB.getBool(&fbdo, base + "/scheduleenabled")) {
      scheduleenabled = fbdo.boolData();
    }

    if (Firebase.RTDB.getInt(&fbdo, base + "/schedule")) {
      schedule = fbdo.intData(); // HHMMHHMM
    }

    int nowHHMM = getCurrentTimeHHMM();

    if (String(dev.name) == "motor") {
      bool motorActuallyOn = digitalRead(DRY_RUN_STATUS_PIN) == LOW;  // adjust depending on logic
      if (dev.isOn && !motorActuallyOn) {
        // Motor was commanded ON, but dry run unit has cut it OFF
        Firebase.RTDB.setBool(&fbdo, "Control/motor/dryrun", true);
        if (!lastDryRunStatus) {
          logToFirebase("[motor] Dry Run or Voltage Fault Detected! Motor OFF via preventor.");
          lastDryRunStatus = true;
        }
        lastMotorRunningStatus = false;
      } else if (dev.isOn && motorActuallyOn) {
        // Motor is ON and preventor allows it
        Firebase.RTDB.setBool(&fbdo, "Control/motor/dryrun", false);
            if (!lastMotorRunningStatus) {
              logToFirebase("[motor] Running OK");
              lastMotorRunningStatus = true;
            }

            lastDryRunStatus = false;
      } else {
        // Motor OFF manually or by schedule/timer
        Firebase.RTDB.setBool(&fbdo, "Control/motor/dryrun", false);
            lastDryRunStatus = false;
            lastMotorRunningStatus = false;
      }
    }


    // --- Schedule Mode ---
    if (scheduleenabled && schedule > 0) {
      int from = schedule / 10000;
      int to = schedule % 10000;
      if (nowHHMM >= from && nowHHMM <= to && !dev.isOn) {
        digitalWrite(dev.pin, LOW);
        dev.isOn = true;
        logToFirebase("[" + String(dev.name) + "] ON by schedule (" + String(from) + " - " + String(to) + ")");
      } else if (dev.isOn && (nowHHMM < from || nowHHMM > to)) {
        digitalWrite(dev.pin, HIGH);
        dev.isOn = false;
        logToFirebase("[" + String(dev.name) + "] OFF by schedule end");
      }
    }
    // --- Manual ON/OFF Mode ---
    if (on && !dev.isOn) {
      if (String(dev.name) == "motor") {
          bool autoMode = false;
          if (Firebase.RTDB.getBool(&fbdo, "mode")) {
              autoMode = fbdo.boolData();
          }
          digitalWrite(DRP_MODE_RELAY, autoMode ? LOW : HIGH);
          logToFirebase("DRP set to " + String(autoMode ? "AUTO" : "MANUAL") + " mode");
        pressRelay(MOTOR_START_RELAY);
      } else {
        digitalWrite(dev.pin, LOW); // For other devices
      }
      dev.isOn = true;
      dev.startMillis = millis();
      dev.durationMillis = timerenabled ? timer * 60000UL : 0;

      int nowHHMM = getCurrentTimeHHMM();
      Firebase.RTDB.setInt(&fbdo, base + "/ontime", nowHHMM);

      if (timerenabled) {
        logToFirebase("[" + String(dev.name) + "] ON manually with timer " + String(timer));
      } else {
        logToFirebase("[" + String(dev.name) + "] ON manually");
      }
    }
    else if (!on && dev.isOn) {
      if (String(dev.name) == "motor") {
        pressRelay(MOTOR_STOP_RELAY);
      } else {
        digitalWrite(dev.pin, HIGH);
      }

      dev.isOn = false;
      logToFirebase("[" + String(dev.name) + "] OFF manually");
    }

    // --- Timer Auto-OFF ---
    if (dev.isOn && timerenabled && dev.durationMillis > 0) {
      if (millis() - dev.startMillis >= dev.durationMillis) {
        digitalWrite(dev.pin, HIGH);
        dev.isOn = false;
        Firebase.RTDB.setBool(&fbdo, base + "/on", false);
        logToFirebase("[" + String(dev.name) + "] OFF by timer");
      }
    }
  }
}
