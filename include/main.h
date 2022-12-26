#ifndef neopixel_mqtt_h
#define neopixel_mqtt_h

#include "credentials.h"       // A file containing all your credentials

// stdlib
#include <iostream>
#include <map>
#include <string>

// ESP8266 libraries
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>

// Third-party libraries (you have to install them)
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// Defines
#define NUM_LEDS 226
#define LED_PIN D2
//--------------------------------------------------------------------------------------

// Functions declaration
void wifiConnect();
void OTASetup();
void serverRouting();
void setAll(uint8_t red, uint8_t green, uint8_t blue);
void nextEffect();
void setRandomColor();
void animation0();
void fadeInOut();
void strobe();
void cylon();
void twinkle();
void sparkle();
void runningLights();
void colorWipe();
void raibowCycle();
void singleDynamic();
void theatreChase();
void theatreChaseRainbow();
void bouncingBalls();
void multiColorBouncingBalls();
byte *Wheel(byte WheelPos);
void fadeToBlack(int ledNo, byte fadeValue);
//---------------------------------------------------------------------------------------

// Variables declaration
typedef void (*EffectFnPtr)();

EffectFnPtr effectsArr[] = {
    animation0,
    fadeInOut,
    strobe,
    cylon,
    twinkle,
    sparkle,
    runningLights,
    colorWipe,
    raibowCycle,
    singleDynamic,
    theatreChase,
    theatreChaseRainbow,
    bouncingBalls,
    multiColorBouncingBalls
};

struct color {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} colors;
//---------------------------------------------------------------------------------------

#endif // neopixel_mqtt_h
