/*
 * ViClean Menu - Konfigurierbare Version mit Settings-Menue
 *
 * Basiert auf viclean_simple_battery, erweitert um ein 2-Tasten-Menue
 * zur individuellen Konfiguration der Programme W und M.
 *
 * Hardware: LilyGO T-Display v1.1 + 450mAh Akku
 *
 * Buttons:
 *   GPIO 35 (oben)  - SEL: Navigieren / Wert aendern
 *   GPIO 0  (unten) - GO:  Bestaetigen / Starten / Stoppen
 *
 * Zustandsmaschine:
 *   STATE_MAIN -> STATE_RUNNING (GO auf W/M)
 *   STATE_MAIN -> STATE_SETTINGS_LIST (GO auf SET)
 *   STATE_SETTINGS_LIST -> STATE_SETTINGS_EDIT (GO auf Parameter)
 *   STATE_SETTINGS_EDIT -> STATE_SETTINGS_LIST (GO = bestaetigen)
 *   STATE_SETTINGS_LIST -> STATE_MAIN (GO auf SPEICHERN/ZURUECK)
 *   STATE_RUNNING -> STATE_MAIN (GO = STOP)
 *
 * Persistenz: Einstellungen werden per NVS (Preferences) gespeichert
 *             und ueberleben Deep Sleep sowie Stromverlust.
 */

#define USER_SETUP_LOADED
#include <User_Setup.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <Preferences.h>

// === Hardware Pins ===
#define TFT_BL 4
#define BUTTON_SELECT 35  // Oben: SEL
#define BUTTON_ACTION 0   // Unten: GO

// === BLE UUIDs ===
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// === Command Codes ===
#define CMD_START_PROGRAM         0x8E
#define CMD_BREAK_PROGRAM         0x8F
#define MODBUS_WRITE_COIL         0x05

// === Sleep Konfiguration ===
#define SLEEP_TIMEOUT_SEC    60    // Sekunden bis Deep Sleep
#define DIM_WARNING_SEC      10    // Sekunden vor Sleep: Display dimmen

// === Zustandsmaschine ===
enum AppState {
  STATE_MAIN,
  STATE_RUNNING,
  STATE_SETTINGS_LIST,
  STATE_SETTINGS_EDIT
};

AppState appState = STATE_MAIN;

// === Programm-Einstellungen ===
struct ProgramSettings {
  uint8_t pump;       // 1-6
  uint8_t nozzle;     // 1-6
  uint8_t temp;       // 1-5
  uint8_t harmonic;   // 0=OFF, 1=ON
  uint8_t pulsation;  // 0=OFF, 1=ON
};

ProgramSettings settingsW = {3, 3, 3, 1, 0};  // Defaults
ProgramSettings settingsM = {3, 3, 3, 1, 0};  // Defaults

Preferences preferences;

// === Display ===
TFT_eSPI tft = TFT_eSPI();

// === BLE Variablen ===
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristicRX;
static BLERemoteCharacteristic* pRemoteCharacteristicTX;
static BLEAdvertisedDevice* myDevice;

// === Programm State ===
uint8_t selectedProgram = 0;  // 0=W(Lady), 1=M(Gent)
bool programRunning = false;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 300;

// Flags fuer Button-Events (gesetzt in ISR, verarbeitet in loop)
volatile bool selectButtonPressed = false;
volatile bool actionButtonPressed = false;

// === Main-Screen Auswahl ===
uint8_t mainSelection = 0;  // 0=W, 1=M, 2=SET

// === Settings State ===
uint8_t settingsEditProfile = 0;  // 0=W, 1=M - welches Profil wird bearbeitet
uint8_t settingsCursor = 0;       // 0-7: Profil, Pump, Duese, Temp, Harmonic, Pulsation, SPEICHERN, ZURUECK
ProgramSettings settingsTemp;     // Temporaere Kopie zum Bearbeiten
uint8_t editValue = 0;            // Aktueller Wert im Edit-Modus

// Settings-Menu Eintraege
#define SETTINGS_ITEMS 8
#define SETTINGS_PROFIL    0
#define SETTINGS_PUMP      1
#define SETTINGS_DUESE     2
#define SETTINGS_TEMP      3
#define SETTINGS_HARMONIC  4
#define SETTINGS_PULSATION 5
#define SETTINGS_SPEICHERN 6
#define SETTINGS_ZURUECK   7

// === Sleep State ===
unsigned long lastActivityTime = 0;
bool displayDimmed = false;

// ==================== NVS Persistenz ====================

void loadSettings() {
  preferences.begin("viclean", true);  // read-only

  settingsW.pump      = preferences.getUChar("w_pump", 3);
  settingsW.nozzle    = preferences.getUChar("w_nozzle", 3);
  settingsW.temp      = preferences.getUChar("w_temp", 3);
  settingsW.harmonic  = preferences.getUChar("w_harmonic", 1);
  settingsW.pulsation = preferences.getUChar("w_pulsation", 0);

  settingsM.pump      = preferences.getUChar("m_pump", 3);
  settingsM.nozzle    = preferences.getUChar("m_nozzle", 3);
  settingsM.temp      = preferences.getUChar("m_temp", 3);
  settingsM.harmonic  = preferences.getUChar("m_harmonic", 1);
  settingsM.pulsation = preferences.getUChar("m_pulsation", 0);

  preferences.end();

  Serial.println("Einstellungen geladen:");
  Serial.printf("  W: Pump=%d Duese=%d Temp=%d Harmonic=%d Puls=%d\n",
    settingsW.pump, settingsW.nozzle, settingsW.temp, settingsW.harmonic, settingsW.pulsation);
  Serial.printf("  M: Pump=%d Duese=%d Temp=%d Harmonic=%d Puls=%d\n",
    settingsM.pump, settingsM.nozzle, settingsM.temp, settingsM.harmonic, settingsM.pulsation);
}

void saveSettings() {
  preferences.begin("viclean", false);  // read-write

  preferences.putUChar("w_pump", settingsW.pump);
  preferences.putUChar("w_nozzle", settingsW.nozzle);
  preferences.putUChar("w_temp", settingsW.temp);
  preferences.putUChar("w_harmonic", settingsW.harmonic);
  preferences.putUChar("w_pulsation", settingsW.pulsation);

  preferences.putUChar("m_pump", settingsM.pump);
  preferences.putUChar("m_nozzle", settingsM.nozzle);
  preferences.putUChar("m_temp", settingsM.temp);
  preferences.putUChar("m_harmonic", settingsM.harmonic);
  preferences.putUChar("m_pulsation", settingsM.pulsation);

  preferences.end();

  Serial.println("Einstellungen gespeichert!");
}

// ==================== Activity Tracking ====================

void resetActivityTimer() {
  lastActivityTime = millis();
  if (displayDimmed) {
    displayDimmed = false;
    analogWrite(TFT_BL, 255);
  }
}

// ==================== Deep Sleep ====================

void goToSleep() {
  Serial.println("Gehe in Deep Sleep...");

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(50, 60);
  tft.print("Schlafe...");
  delay(500);
  digitalWrite(TFT_BL, LOW);

  BLEDevice::deinit(false);
  delay(100);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  esp_sleep_enable_ext1_wakeup(1ULL << GPIO_NUM_35, ESP_EXT1_WAKEUP_ALL_LOW);

  Serial.println("Deep Sleep - druecke beliebigen Button zum Aufwachen");
  Serial.flush();

  esp_deep_sleep_start();
}

void printWakeReason() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

  switch (reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Aufgewacht: GO-Button (GPIO 0)");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Aufgewacht: SEL-Button (GPIO 35)");
      break;
    default:
      Serial.println("Normaler Start (kein Sleep)");
      break;
  }
}

// ==================== Display ====================

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, 255);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  bool fromSleep = (reason == ESP_SLEEP_WAKEUP_EXT0 || reason == ESP_SLEEP_WAKEUP_EXT1);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("ViClean Menu");
  tft.setCursor(10, 30);
  tft.setTextSize(1);
  if (fromSleep) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Aufgewacht! Verbinde...");
  } else {
    tft.println("Verbinde...");
  }
}

// --- STATE_MAIN Anzeige ---
void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);

  // Titel oben
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 5);
  tft.print("ViClean");

  // Sleep-Timer
  if (!programRunning) {
    unsigned long elapsed = (millis() - lastActivityTime) / 1000;
    unsigned long remaining = 0;
    if (elapsed < SLEEP_TIMEOUT_SEC) {
      remaining = SLEEP_TIMEOUT_SEC - elapsed;
    }
    tft.setCursor(80, 5);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.printf("Zzz %lus", remaining);
  }

  // BLE Status
  tft.setCursor(180, 5);
  if (connected) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("CONN");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("----");
  }

  // === 3 Optionen: W, M, SET ===
  // W
  tft.setTextSize(5);
  if (mainSelection == 0) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawRect(8, 30, 65, 55, TFT_GREEN);
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  }
  tft.setCursor(20, 38);
  tft.print("W");

  // M
  if (mainSelection == 1) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawRect(83, 30, 65, 55, TFT_GREEN);
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  }
  tft.setCursor(95, 38);
  tft.print("M");

  // SET
  tft.setTextSize(2);
  if (mainSelection == 2) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawRect(158, 30, 48, 55, TFT_YELLOW);
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  }
  tft.setCursor(163, 48);
  tft.print("SET");

  // Status unten
  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("STOPPED");

  // Button-Labels rechts
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(215, 30);
  tft.print("SEL");
  tft.setCursor(215, 115);
  tft.print("GO");
}

// --- STATE_RUNNING Anzeige ---
void drawRunningScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 5);
  tft.print("ViClean");

  // BLE Status
  tft.setCursor(180, 5);
  if (connected) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("CONN");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("----");
  }

  // Laufendes Programm gross anzeigen
  tft.setTextSize(6);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(90, 35);
  tft.print(selectedProgram == 0 ? "W" : "M");

  // Status
  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("RUNNING");

  // Button-Label
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(210, 115);
  tft.print("STOP");
}

// --- STATE_SETTINGS_LIST Anzeige ---
void drawSettingsListScreen() {
  tft.fillScreen(TFT_BLACK);

  // Titel
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 3);
  tft.printf("Einstellungen [%s]", settingsEditProfile == 0 ? "W" : "M");

  // Sleep-Timer
  unsigned long elapsed = (millis() - lastActivityTime) / 1000;
  unsigned long remaining = 0;
  if (elapsed < SLEEP_TIMEOUT_SEC) {
    remaining = SLEEP_TIMEOUT_SEC - elapsed;
  }
  tft.setCursor(180, 3);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.printf("Zzz %lus", remaining);

  // Eintraege
  const int startY = 18;
  const int lineH = 14;

  for (int i = 0; i < SETTINGS_ITEMS; i++) {
    int y = startY + i * lineH;

    // Cursor
    if (i == settingsCursor) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setCursor(5, y);
      tft.print(">");
    }

    // Label + Wert
    tft.setCursor(15, y);
    switch (i) {
      case SETTINGS_PROFIL:
        if (i == settingsCursor) tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        else tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Profil:     %s", settingsEditProfile == 0 ? "W" : "M");
        break;
      case SETTINGS_PUMP:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Pump:       %d", settingsTemp.pump);
        break;
      case SETTINGS_DUESE:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Duese:      %d", settingsTemp.nozzle);
        break;
      case SETTINGS_TEMP:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Temp:       %d", settingsTemp.temp);
        break;
      case SETTINGS_HARMONIC:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Harmonic:   %s", settingsTemp.harmonic ? "ON" : "OFF");
        break;
      case SETTINGS_PULSATION:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.printf("Pulsation:  %s", settingsTemp.pulsation ? "ON" : "OFF");
        break;
      case SETTINGS_SPEICHERN:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.print("SPEICHERN");
        break;
      case SETTINGS_ZURUECK:
        if (i == settingsCursor) tft.setTextColor(TFT_GREEN, TFT_BLACK);
        else tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.print("ZURUECK");
        break;
    }
  }

  // Button-Labels
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(215, 30);
  tft.print("SEL");
  tft.setCursor(215, 115);
  tft.print("OK");
}

// --- STATE_SETTINGS_EDIT Anzeige ---
void drawSettingsEditScreen() {
  tft.fillScreen(TFT_BLACK);

  // Parameter-Name
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(20, 15);

  const char* paramName = "";
  uint8_t maxVal = 6;
  bool isBool = false;

  switch (settingsCursor) {
    case SETTINGS_PUMP:
      paramName = "Pump";
      maxVal = 6;
      break;
    case SETTINGS_DUESE:
      paramName = "Duese";
      maxVal = 6;
      break;
    case SETTINGS_TEMP:
      paramName = "Temp";
      maxVal = 5;
      break;
    case SETTINGS_HARMONIC:
      paramName = "Harmonic";
      isBool = true;
      break;
    case SETTINGS_PULSATION:
      paramName = "Pulsation";
      isBool = true;
      break;
  }

  tft.print(paramName);

  // Profil-Anzeige
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(180, 20);
  tft.printf("[%s]", settingsEditProfile == 0 ? "W" : "M");

  if (isBool) {
    // Boolean: ON/OFF gross anzeigen
    tft.setTextSize(3);
    tft.setCursor(60, 60);
    if (editValue) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print("ON");
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.print("OFF");
    }
  } else {
    // Numerisch: Balken + Wert
    int barWidth = 150;
    int barHeight = 20;
    int barX = 20;
    int barY = 55;

    // Hintergrund-Balken
    tft.drawRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);

    // Gefuellter Balken
    int fillWidth = (barWidth - 4) * editValue / maxVal;
    tft.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, TFT_GREEN);

    // Wert-Anzeige
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(barX + barWidth + 10, barY - 2);
    tft.printf("%d/%d", editValue, maxVal);
  }

  // Button-Labels
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(220, 30);
  tft.print("+");
  tft.setCursor(215, 115);
  tft.print("OK");
}

// Zentrale Draw-Funktion basierend auf aktuellem State
void drawUI() {
  switch (appState) {
    case STATE_MAIN:
      drawMainScreen();
      break;
    case STATE_RUNNING:
      drawRunningScreen();
      break;
    case STATE_SETTINGS_LIST:
      drawSettingsListScreen();
      break;
    case STATE_SETTINGS_EDIT:
      drawSettingsEditScreen();
      break;
  }
}

// Sleep-Timer Anzeige aktualisieren (ohne volles Redraw)
void updateSleepTimer() {
  if (programRunning) return;
  if (appState == STATE_SETTINGS_EDIT) return;  // Kein Timer im Edit-Screen

  unsigned long elapsed = (millis() - lastActivityTime) / 1000;
  unsigned long remaining = 0;
  if (elapsed < SLEEP_TIMEOUT_SEC) {
    remaining = SLEEP_TIMEOUT_SEC - elapsed;
  }

  if (appState == STATE_MAIN) {
    tft.fillRect(80, 3, 90, 12, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(80, 5);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.printf("Zzz %lus", remaining);
  } else if (appState == STATE_SETTINGS_LIST) {
    tft.fillRect(180, 1, 55, 12, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(180, 3);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.printf("Zzz %lus", remaining);
  }
}

// ==================== BLE Funktionen ====================

uint8_t calculateLRC(const char* data, size_t length) {
  uint8_t lrc = 0;
  for (size_t i = 0; i < length; i++) {
    lrc ^= (uint8_t)data[i];
  }
  return lrc;
}

String createCommand(uint8_t modbusCmd, uint8_t subCmd, uint8_t* data, size_t dataLen) {
  String command = ":FFFF";
  char hex[3];

  sprintf(hex, "%02X", modbusCmd);
  command += hex;

  sprintf(hex, "%02X", subCmd);
  command += hex;

  for (size_t i = 0; i < dataLen; i++) {
    sprintf(hex, "%02X", data[i]);
    command += hex;
  }

  String commandForLRC = command.substring(1);
  uint8_t lrc = calculateLRC(commandForLRC.c_str(), commandForLRC.length());

  sprintf(hex, "%02X", lrc);
  command += hex;
  command += "\r\n";

  return command;
}

void sendCommand(String command) {
  if (!connected || pRemoteCharacteristicRX == nullptr) {
    Serial.println("Nicht verbunden!");
    return;
  }

  Serial.print("TX: ");
  Serial.println(command);

  size_t cmdLen = command.length();

  if (cmdLen > 20) {
    int numBlocks = (cmdLen / 20) + 1;

    for (int i = 0; i < numBlocks; i++) {
      size_t blockStart = i * 20;
      size_t blockSize = min((size_t)20, cmdLen - blockStart);
      String block = command.substring(blockStart, blockStart + blockSize);

      pRemoteCharacteristicRX->writeValue(block.c_str(), blockSize);

      if (i < numBlocks - 1) {
        delay(50);
      }
    }
    delay(100);
  } else {
    pRemoteCharacteristicRX->writeValue(command.c_str(), cmdLen);
    delay(100);
  }
}

void startProgram(uint8_t program) {
  // Einstellungen aus dem jeweiligen Profil lesen
  ProgramSettings& settings = (program == 0) ? settingsW : settingsM;

  uint8_t gender = program;  // 0=Lady, 1=Gent
  uint8_t oscillate = settings.harmonic;
  uint8_t rotate = settings.harmonic;
  uint8_t pulsate = settings.pulsation;
  uint8_t pump = settings.pump;
  uint8_t nozzle = settings.nozzle;
  uint8_t temp = settings.temp;

  uint8_t bitmask = ((gender + 1) & 0x03) |
                    ((oscillate & 0x01) << 2) |
                    ((rotate & 0x01) << 3) |
                    ((pulsate & 0x01) << 4);

  uint8_t data[] = {bitmask, pump, nozzle, temp};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);

  Serial.printf("START %s: Pump=%d Duese=%d Temp=%d Harmonic=%d Puls=%d\n",
    program == 0 ? "LADY" : "GENT", pump, nozzle, temp, settings.harmonic, pulsate);

  sendCommand(cmd);
  programRunning = true;
  appState = STATE_RUNNING;
  resetActivityTimer();
  drawUI();
}

void stopProgram() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM, nullptr, 0);
  Serial.println("STOP");
  sendCommand(cmd);
  programRunning = false;
  appState = STATE_MAIN;
  resetActivityTimer();
  drawUI();
}

// ==================== BLE Callbacks ====================

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  Serial.print("RX: ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.println("VERBUNDEN");
    resetActivityTimer();
    drawUI();
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    programRunning = false;
    Serial.println("GETRENNT");
    if (appState == STATE_RUNNING) {
      appState = STATE_MAIN;
    }
    resetActivityTimer();
    drawUI();
    doScan = true;
  }
};

bool connectToServer() {
  Serial.print("Verbinde zu: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  pClient->connect(myDevice);

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service nicht gefunden");
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristicRX = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
  pRemoteCharacteristicTX = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);

  if (pRemoteCharacteristicRX == nullptr || pRemoteCharacteristicTX == nullptr) {
    Serial.println("Characteristics nicht gefunden");
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristicTX->canNotify()) {
    pRemoteCharacteristicTX->registerForNotify(notifyCallback);
  }

  return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {

      Serial.print("ViClean gefunden: ");
      Serial.println(advertisedDevice.getName().c_str());

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

// ==================== Settings Hilfsfunktionen ====================

// Temporaere Einstellungen vom aktuellen Profil laden
void loadTempSettings() {
  if (settingsEditProfile == 0) {
    settingsTemp = settingsW;
  } else {
    settingsTemp = settingsM;
  }
}

// Temporaere Einstellungen ins aktuelle Profil schreiben
void saveTempToProfile() {
  if (settingsEditProfile == 0) {
    settingsW = settingsTemp;
  } else {
    settingsM = settingsTemp;
  }
}

// Aktuellen Edit-Wert aus settingsTemp laden
uint8_t getEditValueFromSettings() {
  switch (settingsCursor) {
    case SETTINGS_PUMP:      return settingsTemp.pump;
    case SETTINGS_DUESE:     return settingsTemp.nozzle;
    case SETTINGS_TEMP:      return settingsTemp.temp;
    case SETTINGS_HARMONIC:  return settingsTemp.harmonic;
    case SETTINGS_PULSATION: return settingsTemp.pulsation;
    default: return 0;
  }
}

// Edit-Wert zurueck in settingsTemp schreiben
void setEditValueToSettings() {
  switch (settingsCursor) {
    case SETTINGS_PUMP:      settingsTemp.pump = editValue; break;
    case SETTINGS_DUESE:     settingsTemp.nozzle = editValue; break;
    case SETTINGS_TEMP:      settingsTemp.temp = editValue; break;
    case SETTINGS_HARMONIC:  settingsTemp.harmonic = editValue; break;
    case SETTINGS_PULSATION: settingsTemp.pulsation = editValue; break;
  }
}

// ==================== Button Handling ====================

void IRAM_ATTR buttonSelectISR() {
  unsigned long now = millis();
  if (now - lastButtonPress > DEBOUNCE_MS) {
    selectButtonPressed = true;
    lastButtonPress = now;
  }
}

void IRAM_ATTR buttonActionISR() {
  unsigned long now = millis();
  if (now - lastButtonPress > DEBOUNCE_MS) {
    actionButtonPressed = true;
    lastButtonPress = now;
  }
}

void handleButtonEvents() {
  bool selPressed = selectButtonPressed;
  bool actPressed = actionButtonPressed;
  selectButtonPressed = false;
  actionButtonPressed = false;

  if (!selPressed && !actPressed) return;

  resetActivityTimer();

  switch (appState) {

    // ========== STATE_MAIN ==========
    case STATE_MAIN:
      if (selPressed) {
        // Zyklisch: W -> M -> SET -> W
        mainSelection = (mainSelection + 1) % 3;
        drawUI();
      }
      if (actPressed) {
        if (mainSelection == 0 || mainSelection == 1) {
          // W oder M starten
          selectedProgram = mainSelection;  // 0=W, 1=M
          if (connected) {
            startProgram(selectedProgram);
          }
        } else if (mainSelection == 2) {
          // SET -> Settings oeffnen
          settingsEditProfile = 0;  // Start mit Profil W
          settingsCursor = 0;
          loadTempSettings();
          appState = STATE_SETTINGS_LIST;
          drawUI();
        }
      }
      break;

    // ========== STATE_RUNNING ==========
    case STATE_RUNNING:
      if (actPressed) {
        if (connected) {
          stopProgram();
        }
      }
      // SEL hat keine Funktion waehrend RUNNING
      break;

    // ========== STATE_SETTINGS_LIST ==========
    case STATE_SETTINGS_LIST:
      if (selPressed) {
        // Cursor nach unten (zyklisch)
        settingsCursor = (settingsCursor + 1) % SETTINGS_ITEMS;
        drawUI();
      }
      if (actPressed) {
        switch (settingsCursor) {
          case SETTINGS_PROFIL:
            // Profil togglen W <-> M
            saveTempToProfile();  // Aktuelle Aenderungen sichern
            settingsEditProfile = 1 - settingsEditProfile;
            loadTempSettings();   // Neues Profil laden
            drawUI();
            break;

          case SETTINGS_PUMP:
          case SETTINGS_DUESE:
          case SETTINGS_TEMP:
          case SETTINGS_HARMONIC:
          case SETTINGS_PULSATION:
            // Parameter bearbeiten
            editValue = getEditValueFromSettings();
            appState = STATE_SETTINGS_EDIT;
            drawUI();
            break;

          case SETTINGS_SPEICHERN:
            // Einstellungen speichern
            saveTempToProfile();
            saveSettings();
            appState = STATE_MAIN;
            drawUI();
            break;

          case SETTINGS_ZURUECK:
            // Verwerfen
            appState = STATE_MAIN;
            drawUI();
            break;
        }
      }
      break;

    // ========== STATE_SETTINGS_EDIT ==========
    case STATE_SETTINGS_EDIT:
      if (selPressed) {
        // Wert erhoehen mit Wrap
        switch (settingsCursor) {
          case SETTINGS_PUMP:
          case SETTINGS_DUESE:
            // 1-6, Wrap: 6 -> 1
            editValue = (editValue % 6) + 1;
            break;
          case SETTINGS_TEMP:
            // 1-5, Wrap: 5 -> 1
            editValue = (editValue % 5) + 1;
            break;
          case SETTINGS_HARMONIC:
          case SETTINGS_PULSATION:
            // Toggle ON/OFF
            editValue = 1 - editValue;
            break;
        }
        drawUI();
      }
      if (actPressed) {
        // Wert bestaetigen -> zurueck zur Liste
        setEditValueToSettings();
        appState = STATE_SETTINGS_LIST;
        drawUI();
      }
      break;
  }
}

// ==================== Sleep Check ====================

void checkSleepTimeout() {
  // Kein Sleep waehrend Programm laeuft oder im Edit-Modus
  if (programRunning) return;
  if (appState == STATE_SETTINGS_EDIT) return;

  unsigned long elapsed = (millis() - lastActivityTime) / 1000;

  // Dimmen als Warnung
  if (elapsed >= (SLEEP_TIMEOUT_SEC - DIM_WARNING_SEC) && !displayDimmed) {
    displayDimmed = true;
    analogWrite(TFT_BL, 30);
    Serial.println("Display gedimmt - Sleep in Kuerze...");
  }

  // Sleep-Timeout
  if (elapsed >= SLEEP_TIMEOUT_SEC) {
    goToSleep();
  }
}

// ==================== Setup & Loop ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ViClean Menu ===");
  printWakeReason();

  // Einstellungen aus NVS laden
  loadSettings();

  // Buttons
  pinMode(BUTTON_SELECT, INPUT);  // GPIO35 ist input-only
  pinMode(BUTTON_ACTION, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT), buttonSelectISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_ACTION), buttonActionISR, FALLING);

  // Display
  initDisplay();

  // BLE
  BLEDevice::init("ViClean-Menu");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("Suche ViClean...");
  pBLEScan->start(5, false);

  // Activity-Timer starten
  resetActivityTimer();

  Serial.printf("Sleep nach %d Sekunden Inaktivitaet\n", SLEEP_TIMEOUT_SEC);
}

unsigned long lastTimerUpdate = 0;

void loop() {
  // Verbindung herstellen
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("Bereit!");
    }
    doConnect = false;
  }

  // Erneut scannen wenn getrennt
  if (!connected && doScan) {
    delay(1000);
    Serial.println("Erneuter Scan...");
    BLEDevice::getScan()->start(0);
  }

  // Button-Events verarbeiten
  handleButtonEvents();

  // Sleep-Timer Anzeige jede Sekunde aktualisieren
  if (!programRunning && (appState == STATE_MAIN || appState == STATE_SETTINGS_LIST)) {
    unsigned long now = millis();
    if (now - lastTimerUpdate >= 1000) {
      lastTimerUpdate = now;
      updateSleepTimer();
    }
  }

  // Sleep-Timeout pruefen
  checkSleepTimeout();

  delay(10);
}
