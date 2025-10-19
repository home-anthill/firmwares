// include Arduino library to use Arduino function in cpp files
#include <Arduino.h>

// Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Configure I2C for Display
// OLED GND --> GND
// OLED VCC --> 3.3V
// OLED SCL --> GPIO_40
// OLED SDA --> GPIO_39
#define I2C_SDA 39
#define I2C_SCL 40
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool showDisplay = true;

void init_display() {  
  // init display
  Wire.setPins(I2C_SDA, I2C_SCL);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // Address 0x3C for 128x32
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    showDisplay = false;
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.cp437(true);
  display.setCursor(0,0);
  display.print("Starting");
  display.display();
}

void update_display(float value) {
  if (showDisplay) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.cp437(true);
    display.setCursor(0,0);
    display.print(value);
    display.display();
  }
}