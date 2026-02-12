/*
 * LilyGO T-Display - Einfacher Display-Test
 *
 * Dieser Test prüft nur das Display ohne BLE
 * Perfekt zum Debuggen der Display-Konfiguration
 */

#define USER_SETUP_LOADED
#include <User_Setup.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL 4

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== T-Display Test ===");

  // Backlight einschalten
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("Backlight: ON");

  // Display initialisieren
  tft.init();
  Serial.println("TFT initialisiert");

  tft.setRotation(1); // Landscape
  Serial.println("Rotation gesetzt");

  // Farbtest
  tft.fillScreen(TFT_RED);
  Serial.println("Display: ROT");
  delay(1000);

  tft.fillScreen(TFT_GREEN);
  Serial.println("Display: GRÜN");
  delay(1000);

  tft.fillScreen(TFT_BLUE);
  Serial.println("Display: BLAU");
  delay(1000);

  // Schwarzer Hintergrund
  tft.fillScreen(TFT_BLACK);

  // Weißer Text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("LilyGO");
  tft.setCursor(10, 30);
  tft.println("T-Display");
  tft.setCursor(10, 50);
  tft.println("v1.1");

  Serial.println("Display: Text angezeigt");
  Serial.println("\n=== Test erfolgreich! ===");

  // Wenn du bis hierhin Text auf dem Display siehst, funktioniert alles!
}

void loop() {
  // Blinkendes Display
  static unsigned long lastBlink = 0;
  static bool state = false;

  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    state = !state;

    tft.setTextColor(state ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 90);
    tft.setTextSize(3);
    tft.println(state ? "ON " : "OFF");

    Serial.print("Display blinkt: ");
    Serial.println(state ? "ON" : "OFF");
  }
}
