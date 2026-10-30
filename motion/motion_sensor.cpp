#include "config.h"
#include "secrets.h"

// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// include specific libraries:
// `ld2410` by Nick Reynolds v0.2.2
#include <ld2410.h>

ld2410 radar;
HardwareSerial RadarSerial(1);

// global variable used to store the previous value read by the sensor
int previousState = LOW;

unsigned long lastNotification = 0;
const unsigned long notificationCooldown = 30000; // 30 s


void motion_init_sensor() {
  Serial.println("motion_init_sensor - called");

  RadarSerial.begin(
    256000,
    SERIAL_8N1,
    MOTION_UART_TX,
    MOTION_UART_RX
  );

  if (!radar.begin(RadarSerial)) {
    Serial.println("LD2410 non trovato");
  }

  Serial.println("Sensore trovato.");

  if (radar.requestCurrentConfiguration()) {

    Serial.println("\n--- CONFIGURAZIONE ---");

    Serial.print("Max moving gate: ");
    Serial.println(radar.max_moving_gate); // 8

    Serial.print("Max stationary gate: ");
    Serial.println(radar.max_stationary_gate); // 8

    for (int gate = 0; gate <= radar.max_gate; gate++) {

      Serial.print("Gate ");
      Serial.print(gate);

      Serial.print("  Moving: ");
      Serial.print(radar.motion_sensitivity[gate]);

      Serial.print("  Stationary: ");
      Serial.println(radar.stationary_sensitivity[gate]);
      // 23:48:25.616 -> Gate 0  Moving: 50  Stationary: 0
      // 23:48:25.616 -> Gate 1  Moving: 50  Stationary: 0
      // 23:48:25.616 -> Gate 2  Moving: 40  Stationary: 40
      // 23:48:25.616 -> Gate 3  Moving: 30  Stationary: 40
      // 23:48:25.616 -> Gate 4  Moving: 20  Stationary: 30
      // 23:48:25.616 -> Gate 5  Moving: 15  Stationary: 30

    }
  }

  // configure
  // Movimento up to gate 8 (~6 m nominal)
  // Presence limited to minimum = 0
  // Timeout 2 s
  if (!radar.setMaxValues(8, 0, 2)) {
      Serial.println("ERRORE setMaxValues");
      return;
  }

  // gate, Moving sensitivity, Stationary sensitivity
  // Threshold lower = higher sensibility
  // Threshold 100 = mostly disabled

  // config to be aggressive on far gates
  // | Gate |     approx distance     | Moving threshold |
  // | ---: | ----------------------: | ---------------: |
  // |    0 |                0–0,75 m |               55 |
  // |    1 |              0,75–1,5 m |               50 |
  // |    2 |              1,5–2,25 m |               45 |
  // |    3 |                2,25–3 m |               40 |
  // |    4 |                3–3,75 m |               35 |
  // |    5 |              3,75–4,5 m |               30 |
  // |    6 |              4,5–5,25 m |               25 |
  // |    7 |                5,25–6 m |               20 |
  // |    8 |                6–6,75 m |           **15** |
  radar.setGateSensitivityThreshold(0, 55, 100);
  radar.setGateSensitivityThreshold(1, 50, 100);
  radar.setGateSensitivityThreshold(2, 45, 100);
  radar.setGateSensitivityThreshold(3, 40, 100);
  radar.setGateSensitivityThreshold(4, 35, 100);
  radar.setGateSensitivityThreshold(5, 30, 100);
  radar.setGateSensitivityThreshold(6, 25, 100);
  radar.setGateSensitivityThreshold(7, 20, 100);
  radar.setGateSensitivityThreshold(8, 15, 100);

  Serial.println("Configurazione completata.");

  pinMode(MOTION_PIN, INPUT);

  Serial.println("motion_init_sensor - pin configured as input to read values");
}

int motion_get_value() {
  Serial.println("motion_get_value - called");

  if (radar.movingTargetDetected()) {
    Serial.println("MOVIMENTO");
  }

  int state = digitalRead(MOTION_PIN);
  Serial.printf("motion_get_value - digital value read = %d\n", state);

  if (state == HIGH && previousState == LOW) {
    unsigned long now = millis();

    if (now - lastNotification > notificationCooldown) {
      Serial.println("Invio notifica movimento");
      lastNotification = now;
      return state;
    }
  }

  previousState = state;
  return state;
}

int motion_get_prev_value() {
  return previousState;
}