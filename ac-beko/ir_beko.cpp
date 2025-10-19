// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>

// include libraries
// - IRremoteESP8266: https://github.com/crankyoldgit/IRremoteESP8266
#include <IRremoteESP8266.h>
#include <IRsend.h>
// Import the specific implementation to use COOLIX protocol to control Beko ACs
#include <ir_Coolix.h>

// private functions
void ir_send_signal();

// ------------------------------------------------------
// ------------------ IRremoteESP8266 -------------------
// GPIO pin to use to send IR signals
#define IR_SEND_PIN 4
// ------------------------------------------------------
// ---------------- COOLIX protocol ---------------------
#define SEND_COOLIX
// Temoerature ranges
#define TEMP_MIN kCoolixTempMin // 17
#define TEMP_MAX kCoolixTempMax // 30
// Mode possibile values (defined in ir_Coolix.h)
#define MODE_COOL kCoolixCool // 0
#define MODE_DRY kCoolixDry // 1
#define MODE_AUTO kCoolixAuto // 2
#define MODE_HEAT kCoolixHeat // 3
#define MODE_FAN kCoolixFan // 4
// Fan values (defined in ir_Coolix.h)
#define FAN_AUTO0 kCoolixFanAuto0 // 0
#define FAN_MAX kCoolixFanMax // 1
#define FAN_MED kCoolixFanMed // 2
#define FAN_MIN kCoolixFanMin // 4
#define FAN_AUTO kCoolixFanAuto // 5
// global initial state
struct state {
  bool powerStatus = false;
  uint8_t temperature = TEMP_MAX;
  uint8_t operation = MODE_COOL; // mode (heat, cold, ...)
  uint8_t fan = FAN_AUTO0;
};
state acState;
 // Create a A/C object using GPIO to sending messages with
IRCoolixAC ac(IR_SEND_PIN);
// ------------------------------------------------------
// ------------------------------------------------------

void ir_init() {
  ac.calibrate();
  delay(1000);
  // Start AC
  ac.begin();
}

void ir_send_command(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<250> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("ir_send_command - deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonArray mqttFeatures = doc.as<JsonArray>();
  for (int i = 0; i < mqttFeatures.size(); i++) {
    JsonObject mqttFeature = mqttFeatures[i];
    const char* apiTokenval = mqttFeature["apiToken"];
    const char* deviceUuidval = mqttFeature["deviceUuid"];
    const char* macval = mqttFeature["mac"];
    const char* modelval = mqttFeature["model"];
    const char* featureUuidval = mqttFeature["featureUuid"];
    const char* featureNamelval = mqttFeature["featureName"];
    float valueval = mqttFeature["value"];
    Serial.printf("ir_send_command - apiTokenval: %s\n", apiTokenval);
    Serial.printf("ir_send_command - deviceUuidval: %s\n", deviceUuidval);
    Serial.printf("ir_send_command - macval: %s\n", macval);
    Serial.printf("ir_send_command - modelval: %s\n", modelval);
    Serial.printf("ir_send_command - featureUuidval: %s\n", featureUuidval);
    Serial.printf("ir_send_command - featureNamelval: %s\n", featureNamelval);
    Serial.printf("ir_send_command - valueval: %.2f\n", valueval);
    if (strcmp(featureNamelval, "on") == 0) {
      if (valueval == 1) {
        Serial.println("ir_send_command - setting On");
        ac.on();
      } else if (valueval == 0) {
        Serial.println("ir_send_command - setting Off");
        ac.off();
        // because OFF is a special fixed command, and you cannot set any other parameters
        ir_send_signal();
        return;
      }
    }
    if (strcmp(featureNamelval, "setpoint") == 0) {
      if (valueval < TEMP_MIN || valueval > TEMP_MAX) {
        Serial.printf("ir_send_command - cannot set value, because temperature is out of range. Temperature must be >= %d and <= %d\n", TEMP_MIN, TEMP_MAX);
        return;
      }
      Serial.println("ir_send_command - setting temperature");
      ac.setTemp(valueval);
    }
    if (strcmp(featureNamelval, "mode") == 0) {
      if (valueval == 1.0) {
        Serial.println("ir_send_command - setting mode to Cool");
        ac.setMode(MODE_COOL);
      } else if (valueval == 2.0) {
        Serial.println("ir_send_command - setting mode to Auto");
        ac.setMode(MODE_AUTO);
      } else if (valueval == 3.0) {
        Serial.println("ir_send_command - setting mode to Heat");
        ac.setMode(MODE_HEAT);
      } else if (valueval == 4.0) {
        Serial.println("ir_send_command - setting mode to Fan");
        ac.setMode(MODE_FAN);
      } else if (valueval == 5.0) {
        Serial.println("ir_send_command - setting mode to Dry");
        ac.setMode(MODE_DRY);
      } else {
        Serial.println("ir_send_command - cannot set mode. Unsupported value!");
      }
    }
    if (strcmp(featureNamelval, "fanSpeed") == 0) {
      if (valueval == 1.0) {
        Serial.println("ir_send_command - setting fan speed to Min");
        ac.setFan(FAN_MIN);
      } else if (valueval == 2.0) {
        Serial.println("ir_send_command - setting fan speed to Med");
        ac.setFan(FAN_MED);
      } else if (valueval == 3.0) {
        Serial.println("ir_send_command - setting fan speed to Max");
        ac.setFan(FAN_MAX);
      } else if (valueval == 4.0) {
        Serial.println("ir_send_command - setting fan speed to Auto");
        ac.setFan(FAN_AUTO);
      } else if (valueval == 5.0) {
        Serial.println("ir_send_command - setting fan speed to Auto0");
        ac.setFan(FAN_AUTO0);
      } else {
        Serial.println("ir_send_command - cannot set fan speed. Unsupported fan value!");
      }
    }
  }
  ir_send_signal();
}

void ir_send_signal() {
  Serial.println("ir_send_signal - sending value via IR...");
  ac.send();
  Serial.println("ir_send_signal - value sent successfully!");
}