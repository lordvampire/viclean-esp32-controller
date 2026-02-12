/*
 * ViClean DuschWC Controller für LilyGO T-Display v1.1
 *
 * Hardware:
 *   - LilyGO TTGO T-Display ESP32 v1.1
 *   - 135x240 TFT Display (ST7789)
 *   - 2x Buttons (GPIO35, GPIO0)
 *
 * Benötigte Bibliotheken (Arduino IDE):
 *   - TFT_eSPI (von Bodmer)
 *   - ESP32 BLE Arduino (in Board-Manager enthalten)
 *
 * Board-Einstellung:
 *   Tools > Board > ESP32 Arduino > ESP32 Dev Module
 *
 * TFT_eSPI Konfiguration:
 *   User_Setup.h im Projektordner wird automatisch verwendet
 *
 * Button-Funktionen:
 *   - Linker Button (GPIO35): Nächster Test-Befehl
 *   - Rechter Button (GPIO0): Programm starten/stoppen
 */

// Verwende lokale User_Setup.h aus dem Projektordner
#define USER_SETUP_LOADED
#include <User_Setup.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ==================== Display Konfiguration ====================
TFT_eSPI tft = TFT_eSPI();

// Display-Farben
#define COLOR_BG       0x0000  // Schwarz
#define COLOR_HEADER   0x001F  // Blau
#define COLOR_TEXT     0xFFFF  // Weiß
#define COLOR_SUCCESS  0x07E0  // Grün
#define COLOR_ERROR    0xF800  // Rot
#define COLOR_WARNING  0xFFE0  // Gelb
#define COLOR_INFO     0x07FF  // Cyan

// Button-Pins
#define BUTTON_LEFT  35
#define BUTTON_RIGHT 0

// ==================== BLE Konfiguration ====================
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ViClean Befehlscodes
#define CMD_START_PROGRAM         0x8E
#define CMD_BREAK_PROGRAM         0x8F
#define CMD_SET_NOZZLE_POSITION   0x81
#define CMD_SET_TEMP              0x85
#define CMD_SET_WATER_PUMP        0x7E
#define CMD_TOGGLE_OSCILLATION    0x88
#define CMD_TOGGLE_ROTATION       0x89
#define CMD_TOGGLE_PULSATING      0x8D
#define CMD_TOGGLE_HW             0x8A
#define CMD_TOGGLE_BUZZER         0x65

// Modbus-Befehle
#define MODBUS_WRITE_COIL      0x05

// ==================== Globale Variablen ====================
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean programRunning = false;
static BLERemoteCharacteristic* pRemoteCharacteristicRX;
static BLERemoteCharacteristic* pRemoteCharacteristicTX;
static BLEAdvertisedDevice* myDevice;

int currentTest = 0;
int displayLine = 3;
const int maxDisplayLines = 9;

unsigned long lastButtonPress = 0;
const unsigned long buttonDebounce = 200;

// ==================== Display-Funktionen ====================

void initDisplay() {
  // Backlight einschalten
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Backlight ON

  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  // Header
  tft.fillRect(0, 0, 240, 30, COLOR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.print("ViClean BLE");

  // Status-Bereich
  updateStatus("Initialisiere...", COLOR_INFO);
}

void updateStatus(const char* text, uint16_t color) {
  tft.fillRect(0, 35, 240, 20, COLOR_BG);
  tft.setTextSize(1);
  tft.setTextColor(color, COLOR_BG);
  tft.setCursor(5, 40);
  tft.print(text);
  displayLine = 3;
}

void addLogLine(const char* text, uint16_t color = COLOR_TEXT) {
  if (displayLine >= maxDisplayLines) {
    // Scroll: Bildschirm löschen und neu anfangen
    tft.fillRect(0, 60, 240, 75, COLOR_BG);
    displayLine = 3;
  }

  tft.setTextSize(1);
  tft.setTextColor(color, COLOR_BG);
  tft.setCursor(5, 60 + (displayLine * 10));
  tft.print(text);
  displayLine++;
}

void displayDeviceInfo(const char* name, const char* address) {
  tft.fillRect(0, 60, 240, 75, COLOR_BG);
  displayLine = 3;

  tft.setTextSize(1);
  tft.setTextColor(COLOR_SUCCESS, COLOR_BG);
  tft.setCursor(5, 60);
  tft.print("Geraet: ");
  tft.print(name);

  tft.setCursor(5, 72);
  tft.setTextColor(COLOR_INFO, COLOR_BG);
  tft.print(address);

  displayLine = 5;
}

void displayButtons() {
  // Button-Beschriftungen am unteren Rand
  tft.fillRect(0, 125, 240, 10, COLOR_BG);
  tft.setTextSize(1);

  if (connected) {
    // Linker Button
    tft.setTextColor(COLOR_WARNING, COLOR_BG);
    tft.setCursor(5, 125);
    tft.print("TEST");

    // Rechter Button
    tft.setTextColor(programRunning ? COLOR_ERROR : COLOR_SUCCESS, COLOR_BG);
    tft.setCursor(180, 125);
    tft.print(programRunning ? "STOP" : "START");
  } else {
    tft.setTextColor(COLOR_INFO, COLOR_BG);
    tft.setCursor(70, 125);
    tft.print("Warte...");
  }
}

// ==================== LRC Berechnung ====================

uint8_t calculateLRC(const char* data, size_t length) {
  uint8_t lrc = 0;
  for (size_t i = 0; i < length; i++) {
    lrc ^= (uint8_t)data[i];
  }
  return lrc;
}

// ==================== Befehlserstellung ====================

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

void sendCommand(String command, const char* description) {
  if (connected && pRemoteCharacteristicRX != nullptr) {
    addLogLine(description, COLOR_INFO);
    pRemoteCharacteristicRX->writeValue(command.c_str(), command.length());
    delay(50);
    addLogLine("-> OK", COLOR_SUCCESS);
  } else {
    addLogLine("Fehler: Nicht verbunden", COLOR_ERROR);
  }
}

// ==================== ViClean Befehle ====================

void cmdStartProgram(uint8_t gender, uint8_t oscillate, uint8_t rotate,
                     uint8_t pulsate, uint8_t pumpstate, uint8_t nozzlestate,
                     uint8_t temperature) {
  uint8_t bitmask = ((gender + 1) & 0x03) |
                    ((oscillate & 0x01) << 2) |
                    ((rotate & 0x01) << 3) |
                    ((pulsate & 0x01) << 4);

  uint8_t data[] = {bitmask, pumpstate, nozzlestate, temperature};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);
  sendCommand(cmd, "Start Programm");
  programRunning = true;
  displayButtons();
}

void cmdBreakProgram() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM, nullptr, 0);
  sendCommand(cmd, "Stop Programm");
  programRunning = false;
  displayButtons();
}

void cmdSetTemperature(uint8_t temp) {
  uint8_t data[] = {temp};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_TEMP, data, 1);
  char desc[30];
  sprintf(desc, "Temp: %d", temp);
  sendCommand(cmd, desc);
}

void cmdSetWaterPump(uint8_t pressure) {
  uint8_t data[] = {pressure};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_WATER_PUMP, data, 1);
  char desc[30];
  sprintf(desc, "Pumpe: %d", pressure);
  sendCommand(cmd, desc);
}

void cmdSetNozzlePosition(uint8_t position) {
  uint8_t data[] = {position};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_NOZZLE_POSITION, data, 1);
  char desc[30];
  sprintf(desc, "Duese: %d", position);
  sendCommand(cmd, desc);
}

void cmdToggleOscillation() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_OSCILLATION, nullptr, 0);
  sendCommand(cmd, "Toggle Oszillation");
}

void cmdToggleRotation() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_ROTATION, nullptr, 0);
  sendCommand(cmd, "Toggle Rotation");
}

void cmdTogglePulsating() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_PULSATING, nullptr, 0);
  sendCommand(cmd, "Toggle Pulsation");
}

void cmdToggleBuzzer() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_BUZZER, nullptr, 0);
  sendCommand(cmd, "Toggle Buzzer");
}

// ==================== Test-Sequenzen ====================

void runTestCommand() {
  switch(currentTest) {
    case 0:
      cmdToggleBuzzer();
      break;
    case 1:
      cmdSetTemperature(2);
      break;
    case 2:
      cmdSetTemperature(4);
      break;
    case 3:
      cmdSetWaterPump(2);
      break;
    case 4:
      cmdSetWaterPump(4);
      break;
    case 5:
      cmdSetNozzlePosition(2);
      break;
    case 6:
      cmdSetNozzlePosition(4);
      break;
    case 7:
      cmdToggleOscillation();
      break;
    case 8:
      cmdToggleRotation();
      break;
    case 9:
      cmdTogglePulsating();
      break;
    default:
      cmdToggleBuzzer();
      currentTest = -1;
      break;
  }
  currentTest++;
}

// ==================== BLE Callbacks ====================

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  char hexStr[50] = "RX: ";
  int offset = 4;
  for (int i = 0; i < length && i < 10; i++) {
    sprintf(hexStr + offset, "%02X ", pData[i]);
    offset += 3;
  }
  addLogLine(hexStr, COLOR_WARNING);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    updateStatus("Verbunden!", COLOR_SUCCESS);
    displayButtons();
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    programRunning = false;
    updateStatus("Getrennt - Suche...", COLOR_WARNING);
    doScan = true;
    displayButtons();
  }
};

bool connectToServer() {
  addLogLine("Verbinde...", COLOR_INFO);

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  pClient->connect(myDevice);

  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    addLogLine("Service nicht gefunden", COLOR_ERROR);
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristicRX = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
  if (pRemoteCharacteristicRX == nullptr) {
    addLogLine("RX Char nicht gefunden", COLOR_ERROR);
    pClient->disconnect();
    return false;
  }

  pRemoteCharacteristicTX = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);
  if (pRemoteCharacteristicTX == nullptr) {
    addLogLine("TX Char nicht gefunden", COLOR_ERROR);
    pClient->disconnect();
    return false;
  }

  if (pRemoteCharacteristicTX->canNotify()) {
    pRemoteCharacteristicTX->registerForNotify(notifyCallback);
  }

  const char* name = myDevice->getName().c_str();
  const char* addr = myDevice->getAddress().toString().c_str();
  displayDeviceInfo(name[0] ? name : "ViClean", addr);

  return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {

      addLogLine("ViClean gefunden!", COLOR_SUCCESS);
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

// ==================== Button Handler ====================

void checkButtons() {
  unsigned long now = millis();
  if (now - lastButtonPress < buttonDebounce) {
    return;
  }

  // Linker Button (GPIO35) - Test-Befehle
  if (digitalRead(BUTTON_LEFT) == LOW) {
    lastButtonPress = now;
    if (connected) {
      runTestCommand();
    }
  }

  // Rechter Button (GPIO0) - Start/Stop Programm
  if (digitalRead(BUTTON_RIGHT) == LOW) {
    lastButtonPress = now;
    if (connected) {
      if (programRunning) {
        cmdBreakProgram();
      } else {
        // Standard-Programm: Lady, Oszillation, mittlere Stufen
        cmdStartProgram(0, 1, 0, 0, 3, 3, 3);
      }
    }
  }
}

// ==================== Setup ====================

void setup() {
  Serial.begin(115200);
  Serial.println("ViClean T-Display Controller startet...");

  // Buttons initialisieren
  // GPIO35 ist input-only, hat keinen internen Pull-up (externe Pull-ups vorhanden)
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  // Display initialisieren
  initDisplay();

  // BLE initialisieren
  updateStatus("BLE Init...", COLOR_INFO);
  BLEDevice::init("ViClean-Controller");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  updateStatus("Suche ViClean...", COLOR_INFO);
  addLogLine("Scanne BLE-Geraete...");

  pBLEScan->start(5, false);

  displayButtons();
}

// ==================== Loop ====================

void loop() {
  // Buttons prüfen
  checkButtons();

  // Verbindung herstellen
  if (doConnect == true) {
    if (connectToServer()) {
      updateStatus("Verbunden!", COLOR_SUCCESS);
      addLogLine("Bereit fuer Befehle", COLOR_SUCCESS);
      displayButtons();
    } else {
      updateStatus("Verbindung fehlgeschlagen", COLOR_ERROR);
    }
    doConnect = false;
  }

  // Erneut scannen wenn getrennt
  if (!connected && doScan) {
    delay(1000);
    updateStatus("Suche ViClean...", COLOR_INFO);
    addLogLine("Starte neuen Scan...");
    BLEDevice::getScan()->start(0);
  }

  delay(100);
}
