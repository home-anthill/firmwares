// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>
// include json library (https://github.com/bblanchon/ArduinoJson)
#include <ArduinoJson.h>

// include libraries
// - IRremoteESP8266: https://github.com/crankyoldgit/IRremoteESP8266
#include <IRac.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
// Import the specific implementation to use LG protocol to control LG ACs
#include <ir_LG.h>

#include "secrets.h"

// private functions
void ir_send_signal();

// ------------------------------------------------------
// ------------------ IRremoteESP8266 -------------------
// GPIO pin to use to send IR signals
#define IR_SEND_PIN 4
// ------------------------------------------------------
// ------------------ LG protocol -----------------------
#define SEND_LG
// Temoerature ranges
#define TEMP_MIN 18 // forced to 18, because kLgAcMinTemp=16
#define TEMP_MAX kLgAcMaxTemp // 30
// Mode possibile values (defined in ir_LG.h)
#define MODE_COOL kLgAcCool // 0
#define MODE_DRY kLgAcDry // 1
#define MODE_FAN kLgAcFan // 2
#define MODE_AUTO kLgAcAuto // 3
#define MODE_HEAT kLgAcHeat // 4
// Fan values (defined in ir_LG.h)
#define FAN_MAX kLgAcFanHigh // 10
#define FAN_MED kLgAcFanMedium // 2
#define FAN_MIN kLgAcFanLowest // 0
#define FAN_AUTO kLgAcFanAuto // 5

 // Create a A/C object using GPIO to sending messages with
IRLgAc ac(IR_SEND_PIN);
// ------------------------------------------------------
// ------------------------------------------------------

void ir_init() {
  // set model version
  // based on your remote controller
  ac.setModel(lg_ac_remote_model_t::AKB74955603);

  ac.calibrate();
  delay(1000);
  // Start AC
  ac.begin();
}

void ir_send_command(char* topic, uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("ir_send_command - deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  JsonArray mqttFeatures = doc.as<JsonArray>();
  for (size_t i = 0; i < mqttFeatures.size(); i++) {
    JsonObject mqttFeature = mqttFeatures[i];
    const char* apiTokenval    = mqttFeature["apiToken"];
    const char* deviceUuidval  = mqttFeature["deviceUuid"];
    const char* macval         = mqttFeature["mac"];
    const char* modelval       = mqttFeature["model"];
    const char* featureUuidval = mqttFeature["featureUuid"];
    const char* featureNameval = mqttFeature["featureName"];
    float valueval             = mqttFeature["value"];

    // Validate required fields before use
    if (apiTokenval == nullptr || modelval == nullptr || featureNameval == nullptr) {
      Serial.println("ir_send_command - skipping entry with null required field");
      continue;
    }
    if (strcmp(apiTokenval, API_TOKEN) != 0 || strcmp(modelval, MODEL) != 0) {
      Serial.println("ir_send_command - apiToken or model mismatch, ignoring command");
      return;
    }

    Serial.printf("ir_send_command - apiTokenval: %s\n", apiTokenval);
    Serial.printf("ir_send_command - deviceUuidval: %s\n", deviceUuidval ? deviceUuidval : "(null)");
    Serial.printf("ir_send_command - macval: %s\n", macval ? macval : "(null)");
    Serial.printf("ir_send_command - modelval: %s\n", modelval);
    Serial.printf("ir_send_command - featureUuidval: %s\n", featureUuidval ? featureUuidval : "(null)");
    Serial.printf("ir_send_command - featureNameval: %s\n", featureNameval);
    Serial.printf("ir_send_command - valueval: %.2f\n", valueval);

    int cmd = (int)roundf(valueval);

    if (strcmp(featureNameval, "on") == 0) {
      if (cmd == 1) {
        Serial.println("ir_send_command - setting On");
        ac.on();
      } else if (cmd == 0) {
        Serial.println("ir_send_command - setting Off");
        ac.off();
        // because OFF is a special fixed command, and you cannot set any other parameters
        ir_send_signal();
        return;
      }
    }
    if (strcmp(featureNameval, "setpoint") == 0) {
      if (valueval < TEMP_MIN || valueval > TEMP_MAX) {
        Serial.printf("ir_send_command - cannot set value, because temperature is out of range. Temperature must be >= %d and <= %d\n", TEMP_MIN, TEMP_MAX);
        return;
      }
      Serial.println("ir_send_command - setting temperature");
      ac.setTemp(valueval);
    }
    if (strcmp(featureNameval, "mode") == 0) {
      switch (cmd) {
        case 1: Serial.println("ir_send_command - setting mode to Cool"); ac.setMode(MODE_COOL); break;
        case 2: Serial.println("ir_send_command - setting mode to Auto"); ac.setMode(MODE_AUTO); break;
        case 3: Serial.println("ir_send_command - setting mode to Heat"); ac.setMode(MODE_HEAT); break;
        case 4: Serial.println("ir_send_command - setting mode to Fan");  ac.setMode(MODE_FAN);  break;
        case 5: Serial.println("ir_send_command - setting mode to Dry");  ac.setMode(MODE_DRY);  break;
        default: Serial.println("ir_send_command - cannot set mode. Unsupported value!"); break;
      }
    }
    if (strcmp(featureNameval, "fanSpeed") == 0) {
      switch (cmd) {
        case 1: Serial.println("ir_send_command - setting fan speed to Min");  ac.setFan(FAN_MIN);  break;
        case 2: Serial.println("ir_send_command - setting fan speed to Med");  ac.setFan(FAN_MED);  break;
        case 3: Serial.println("ir_send_command - setting fan speed to Max");  ac.setFan(FAN_MAX);  break;
        case 4: Serial.println("ir_send_command - setting fan speed to Auto"); ac.setFan(FAN_AUTO); break;
        case 5: Serial.println("ir_send_command - Auto0 not supported on this device"); break;
        default: Serial.println("ir_send_command - cannot set fan speed. Unsupported fan value!"); break;
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
