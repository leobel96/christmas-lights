#include "main.h"

WiFiClient espClient;
AsyncWebServer server(80);
//--------------------------------------------------------------------------------------

// Leds initialization
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
//--------------------------------------------------------------------------------------

// Variables initialization
bool isOn = true;
uint8_t currEffectIdx = 0;
EffectFnPtr currEffectHandler = effectsArr[currEffectIdx];
bool effectsCarousel = true;
bool randomColor = true;
unsigned long lastEffectChange = 0;
bool effectInitialized = false;
//--------------------------------------------------------------------------------------

void setup() {
  colors = {255, 0, 0}; // Set initial LEDs color to red

  // Serial
  Serial.begin(9600);

  // WiFi
  wifiConnect();
  delay(50);

  // LEDs
  pixels.begin();
  delay(50);
  pixels.clear();
  pixels.show();

  // WebServer
  if (LittleFS.begin()) {
    Serial.print(F("FS mounted\n"));
  } else {
    Serial.print(F("FS mount failed\n"));
  }
  serverRouting();
  server.begin();

  // OTA
  OTASetup();
  delay(50);
}

void loop() {
  ArduinoOTA.handle();
  if (isOn) {
    currEffectHandler();
    if (effectsCarousel) {
      // Change effect every 30 seconds or when the watchdog resets (~ 50 days)
      bool shouldChange =
          (millis() - lastEffectChange) > 30000 || millis() < lastEffectChange;
      if (shouldChange) {
        lastEffectChange = millis();
        setRandomColor();
        nextEffect();
      }
    }
  }
}

// Connects to WiFi
void wifiConnect() {
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.println(" ...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi, IP address: ");
  Serial.println(WiFi.localIP());
}

void OTASetup() {
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using
    // SPIFFS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

/* -------------------------------------------------------------------------- */
/*                                   Server                                   */
/* -------------------------------------------------------------------------- */

void serverRouting() {
  server.serveStatic("/index", LittleFS, "/index.html");

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("/state");
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->addHeader("Access-Control-Allow-Origin", "*");
    StaticJsonDocument<128> doc;

    doc["on"] = isOn;
    doc["effect"] = currEffectIdx;
    doc["carousel"] = effectsCarousel;
    doc["brightness"] = pixels.getBrightness();

    JsonArray color = doc.createNestedArray("color");
    color.add(colors.red);
    color.add(colors.green);
    color.add(colors.blue);

    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/on", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("/on");
    isOn = true;
    request->send(200);
  });

  server.on("/off", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("/off");
    isOn = false;
    pixels.clear();
    pixels.show();
    request->send(200);
  });

  server.on("/brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      Serial.print("/brightness: Setting brightness: ");
      String brightness = request->getParam("value", true)->value();
      Serial.println(brightness);
      pixels.setBrightness(brightness.toInt());
      request->send(200);
    } else {
      Serial.println("/brightness: No 'value' found");
      request->send(400);
    }
  });

  server.on("/color", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      String strColor = request->getParam("value", true)->value();
      if (strColor == "random") {
        randomColor = true;
        Serial.println("/color: Setting random color");
      } else {
        randomColor = false;
        uint32_t hexColor = strtoul(strColor.c_str(), NULL, 16);
        colors.red = (uint8_t)(hexColor >> 16);
        colors.green = (uint8_t)(hexColor >> 8);
        colors.blue = (uint8_t)hexColor;
        Serial.printf("/color: Setting color: r=%d, g=%d, b=%d\n",
                      colors.red, colors.green, colors.blue);
      }
      request->send(200);
    }
  });

  server.on("/effect", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("name", true)) {
      String effectName = request->getParam("name", true)->value();
      if (effectName == "carousel") {
        Serial.println("/effect: Setting carousel");
        effectsCarousel = true;
        request->send(200);
      } else {
        uint8_t effectIdx = effectName.toInt();
        if (effectIdx < sizeof(effectsArr)) {
          currEffectIdx = effectIdx;
          Serial.printf("/effect: Setting effect: %d\n", currEffectIdx);
          currEffectHandler = effectsArr[currEffectIdx];
          effectsCarousel = false;
          request->send(200);
        } else {
          Serial.printf("/effect: Effect %d not found\n", effectIdx);
          request->send(400);
        }
      }
    } else {
      Serial.println("/effect: No 'name' found");
      request->send(400);
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Uri not found " + request->url());
  });
}

/* -------------------------------------------------------------------------- */
/*                                  Utilities                                 */
/* -------------------------------------------------------------------------- */

void setAll(uint8_t red, uint8_t green, uint8_t blue) {
  pixels.fill(pixels.Color(red, green, blue));
  pixels.show();
}

void nextEffect() {
  currEffectIdx = (currEffectIdx + 1) % (sizeof(effectsArr)/sizeof(effectsArr[0]));
  currEffectHandler = effectsArr[currEffectIdx];
  effectInitialized = false;
}

void setRandomColor() {
  // Using subsequent analogRead for all the seeds, was resulting in very similar colors for me.
  // This method seems to be working fine.
  randomSeed(analogRead(0));
  colors.red = random(255);
  randomSeed(colors.red);
  colors.green = random(255);
  randomSeed(colors.green);
  colors.blue = random(255);
  randomSeed(colors.blue);
}

/* -------------------------------------------------------------------------- */
/*                                   Effects                                  */
/* -------------------------------------------------------------------------- */

void animation0() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  setAll(red, green, blue);
  pixels.show();
}

void fadeInOut() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  float r, g, b;
  for (int k = 0; k < 256; k = k + 1) {
    delay(0);
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setAll(r, g, b);
    pixels.show();
  }
  for (int k = 255; k >= 0; k = k - 2) {
    delay(0);
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setAll(r, g, b);
    pixels.show();
  }
}

void strobe() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  for (int j = 0; j < 10; j++) {
    setAll(red, green, blue);
    pixels.show();
    delay(50);
    setAll(0, 0, 0);
    pixels.show();
    delay(50);
  }
}

void cylon() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  for (int i = 0; i < NUM_LEDS - 4 - 2; i++) {
    setAll(0, 0, 0);
    pixels.setPixelColor(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= 4; j++) {
      pixels.setPixelColor(i + j, red, green, blue);
    }
    pixels.setPixelColor(i + 4 + 1, red / 10, green / 10, blue / 10);
    pixels.show();
    ;
    delay(10);
  }
  delay(50);
  for (int i = NUM_LEDS - 4 - 2; i > 0; i--) {
    setAll(0, 0, 0);
    pixels.setPixelColor(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= 4; j++) {
      pixels.setPixelColor(i + j, red, green, blue);
    }
    pixels.setPixelColor(i + 4 + 1, red / 10, green / 10, blue / 10);
    pixels.show();
    delay(10);
  }
  delay(50);
}

void twinkle() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  setAll(0, 0, 0);
  for (int i = 0; i < 10; i++) {
    pixels.setPixelColor(random(NUM_LEDS), red, green, blue);
    pixels.show();
    delay(100);
  }
  delay(100);
}

void sparkle() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  int Pixel = random(NUM_LEDS);
  pixels.setPixelColor(Pixel, red, green, blue);
  pixels.show();
  pixels.setPixelColor(Pixel, 0, 0, 0);
}

void runningLights() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  int Position = 0;
  for (int j = 0; j < NUM_LEDS * 2; j++) {
    Position++;
    for (int i = 0; i < NUM_LEDS; i++) {
      pixels.setPixelColor(i, ((sin(i + Position) * 127 + 128) / 255) * red,
                           ((sin(i + Position) * 127 + 128) / 255) * green,
                           ((sin(i + Position) * 127 + 128) / 255) * blue);
    }
    pixels.show();
    delay(50);
  }
}

void colorWipe() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, red, green, blue);
    pixels.show();
    delay(30);
  }
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    pixels.setPixelColor(i, 0, 0, 0);
    pixels.show();
  }
}

byte *Wheel(byte WheelPos) {
  static byte c[3];
  if (WheelPos < 85) {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  } else {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }
  return c;
}

void raibowCycle() {
  byte *c;
  uint16_t i, j;
  for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
    for (i = 0; i < NUM_LEDS; i++) {
      c = Wheel(((i * 256 / NUM_LEDS) + j) & 255);
      pixels.setPixelColor(i, *c, *(c + 1), *(c + 2));
    }
    pixels.show();
    delay(20);
  }
}

void singleDynamic() {
  byte *c;
  if (!effectInitialized) {
    for(int i = 0; i < NUM_LEDS; i++) {
      c = Wheel(random(255));
      pixels.setPixelColor(i, *c, *(c + 1), *(c + 2));
      pixels.show();
    }
    effectInitialized = true;
  }
  c = Wheel(random(255));
  pixels.setPixelColor(random(NUM_LEDS), *c, *(c + 1), *(c + 2));
  pixels.show();
  delay(20);
}

void theatreChase() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  for (int j = 0; j < 10; j++) {
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < NUM_LEDS; i = i + 3) {
        pixels.setPixelColor(i + q, red, green, blue);
      }
      pixels.show();
      delay(50);
      for (int i = 0; i < NUM_LEDS; i = i + 3) {
        pixels.setPixelColor(i + q, 0, 0, 0);
      }
    }
  }
}

void theatreChaseRainbow() {
  byte *c;
  for (int j = 0; j < 256; j++) { // cycle all 256 colors in the wheel
    for (int q = 0; q < 3; q++) {
      for (int i = 0; i < NUM_LEDS; i = i + 3) {
        c = Wheel((i + j) % 255);
        pixels.setPixelColor(i + q, *c, *(c + 1),
                             *(c + 2)); // turn every third pixel on
      }
      pixels.show();
      delay(50);
      for (int i = 0; i < NUM_LEDS; i = i + 3) {
        pixels.setPixelColor(i + q, 0, 0, 0); // turn every third pixel off
      }
    }
  }
}

void bouncingBalls() {
  uint8_t red = colors.red;
  uint8_t green = colors.green;
  uint8_t blue = colors.blue;
  int boh = NUM_LEDS * 30;
  int BallCount = (int)(1 + (NUM_LEDS / 30));
  float Gravity = -9.81;
  int StartHeight = 1;
  float Height[BallCount];
  float ImpactVelocityStart = sqrt(-2 * Gravity * StartHeight);
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int Position[BallCount];
  long ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  for (int i = 0; i < BallCount; i++) {
    delay(0);
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i) / pow(BallCount, 2);
  }
  while (boh > 0) {
    delay(0);
    for (int i = 0; i < BallCount; i++) {
      TimeSinceLastBounce[i] = millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow(TimeSinceLastBounce[i] / 1000, 2.0) +
                  ImpactVelocity[i] * TimeSinceLastBounce[i] / 1000;
      if (Height[i] < 0) {
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();
        if (ImpactVelocity[i] < 0.01) {
          ImpactVelocity[i] = ImpactVelocityStart;
        }
      }
      Position[i] = round(Height[i] * (NUM_LEDS - 1) / StartHeight);
    }
    for (int i = 0; i < BallCount; i++) {
      pixels.setPixelColor(Position[i], red, green, blue);
    }
    pixels.show();
    setAll(0, 0, 0);
    boh--;
  }
}

void multiColorBouncingBalls() {
  randomSeed(analogRead(0));
  uint8_t red = random(255);
  randomSeed(analogRead(0));
  uint8_t green = random(255);
  randomSeed(analogRead(0));
  uint8_t blue = random(255);
  int boh = NUM_LEDS * 30;
  int BallCount = (int)(1 + (NUM_LEDS / 30));
  float Gravity = -9.81;
  int StartHeight = 1;
  float Height[BallCount];
  float ImpactVelocityStart = sqrt(-2 * Gravity * StartHeight);
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int Position[BallCount];
  long ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  for (int i = 0; i < BallCount; i++) {
    delay(0);
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i) / pow(BallCount, 2);
  }
  while (boh > 0) {
    delay(0);
    for (int i = 0; i < BallCount; i++) {
      TimeSinceLastBounce[i] = millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow(TimeSinceLastBounce[i] / 1000, 2.0) +
                  ImpactVelocity[i] * TimeSinceLastBounce[i] / 1000;
      if (Height[i] < 0) {
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();
        if (ImpactVelocity[i] < 0.01) {
          ImpactVelocity[i] = ImpactVelocityStart;
        }
      }
      Position[i] = round(Height[i] * (NUM_LEDS - 1) / StartHeight);
    }
    for (int i = 0; i < BallCount; i++) {
      pixels.setPixelColor(Position[i], red, green, blue);
    }
    pixels.show();
    setAll(0, 0, 0);
    boh--;
  }
}

void fadeToBlack(int ledNo, byte fadeValue) {
  uint32_t oldColor;
  uint8_t r, g, b;
  // int value;
  oldColor = pixels.getPixelColor(ledNo);
  r = (oldColor & 0x00ff0000UL) >> 16;
  g = (oldColor & 0x0000ff00UL) >> 8;
  b = (oldColor & 0x000000ffUL);
  r = (r <= 10) ? 0 : (int)r - (r * fadeValue / 256);
  g = (g <= 10) ? 0 : (int)g - (g * fadeValue / 256);
  b = (b <= 10) ? 0 : (int)b - (b * fadeValue / 256);
  pixels.setPixelColor(ledNo, r, g, b);
}
