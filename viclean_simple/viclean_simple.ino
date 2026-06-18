/*
 * ViClean Simple - Minimal UI mit 2 Programmen
 *
 * Hardware: LilyGO T-Display v1.1
 *
 * Buttons:
 *   GPIO 35 (links)  - Wechsel zwischen M/W
 *   GPIO 0  (rechts) - START / STOP
 *
 * Programme:
 *   M (Men)   - Gent, HarmonicWave, 3-3-3
 *   W (Woman) - Lady, HarmonicWave, 3-3-3
 */

#define USER_SETUP_LOADED
#include <User_Setup.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// === Hardware Pins ===
#define TFT_BL 4
#define BUTTON_SELECT 35  // Links: Wechsel M/W
#define BUTTON_ACTION 0   // Rechts: START/STOP

// === BLE UUIDs ===
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// === Command Codes ===
#define CMD_START_PROGRAM         0x8E
#define CMD_BREAK_PROGRAM         0x8F
#define MODBUS_WRITE_COIL         0x05

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
uint8_t selectedProgram = 0;  // 0=Lady(W), 1=Men(M)
bool programRunning = false;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 300;

// Flags für Button-Events (gesetzt in ISR, verarbeitet in loop)
volatile bool selectButtonPressed = false;
volatile bool actionButtonPressed = false;

// ==================== Display ====================

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("ViClean Simple");
  tft.setCursor(10, 30);
  tft.setTextSize(1);
  tft.println("Verbinde...");
}

void drawUI() {
  tft.fillScreen(TFT_BLACK);

  // Titel oben
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 5);
  tft.print("ViClean");

  // Status rechts oben
  tft.setCursor(180, 5);
  if (connected) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("CONN");
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("----");
  }

  // === Große Buchstaben ===
  int centerX = 120;
  int yPos = 50;

  // W (Woman/Lady)
  tft.setTextSize(6);
  if (selectedProgram == 0) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawRect(30, 35, 80, 60, TFT_GREEN);  // Rahmen
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  }
  tft.setCursor(50, 50);
  tft.print("W");

  // M (Men)
  if (selectedProgram == 1) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawRect(130, 35, 80, 60, TFT_GREEN);  // Rahmen
  } else {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  }
  tft.setCursor(150, 50);
  tft.print("M");

  // Status unten
  tft.setTextSize(2);
  tft.setCursor(60, 110);
  if (programRunning) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.print("RUNNING");
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("STOPPED");
  }

  // Button-Beschriftung rechts (neben den physischen Tasten)
  // Oberer Button (GPIO 35) = SELECT
  // Unterer Button (GPIO 0) = START/STOP
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(205, 30);
  tft.print("SEL");
  tft.setCursor(205, 115);
  if (programRunning) {
    tft.print("STOP");
  } else {
    tft.print("GO");
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

  // Packet-Splitting für Befehle >20 Bytes
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
  uint8_t gender = program;  // 0=Lady, 1=Gent
  uint8_t oscillate = 1;     // HarmonicWave ON
  uint8_t rotate = 1;        // HarmonicWave ON
  uint8_t pulsate = 0;       // Puls OFF
  uint8_t pump = 3;          // Stufe 3
  uint8_t nozzle = 3;        // Stufe 3
  uint8_t temp = 3;          // Stufe 3

  uint8_t bitmask = ((gender + 1) & 0x03) |
                    ((oscillate & 0x01) << 2) |
                    ((rotate & 0x01) << 3) |
                    ((pulsate & 0x01) << 4);

  uint8_t data[] = {bitmask, pump, nozzle, temp};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);

  Serial.print("START ");
  Serial.println(program == 0 ? "LADY" : "GENT");

  sendCommand(cmd);
  programRunning = true;
  drawUI();
}

void stopProgram() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM, nullptr, 0);
  Serial.println("STOP");
  sendCommand(cmd);
  programRunning = false;
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
    drawUI();
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    programRunning = false;  // Nach Disconnect ist Programm gestoppt
    Serial.println("GETRENNT");
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

// ==================== Button Handling ====================

void IRAM_ATTR buttonSelectISR() {
  unsigned long now = millis();
  if (now - lastButtonPress > DEBOUNCE_MS) {
    selectButtonPressed = true;  // Nur Flag setzen!
    lastButtonPress = now;
  }
}

void IRAM_ATTR buttonActionISR() {
  unsigned long now = millis();
  if (now - lastButtonPress > DEBOUNCE_MS) {
    actionButtonPressed = true;  // Nur Flag setzen!
    lastButtonPress = now;
  }
}

// Button-Events verarbeiten (wird in loop() aufgerufen)
void handleButtonEvents() {
  // SELECT Button: Programm wechseln
  if (selectButtonPressed) {
    selectButtonPressed = false;
    if (!programRunning) {  // Nur wenn Programm nicht läuft
      selectedProgram = 1 - selectedProgram;  // Toggle 0<->1
      Serial.print("Gewählt: ");
      Serial.println(selectedProgram == 0 ? "LADY" : "GENT");
      drawUI();
    }
  }

  // ACTION Button: START/STOP
  if (actionButtonPressed) {
    actionButtonPressed = false;
    if (connected) {
      if (programRunning) {
        stopProgram();
      } else {
        startProgram(selectedProgram);
      }
    }
  }
}

// ==================== Setup & Loop ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ViClean Simple ===");

  // Buttons
  pinMode(BUTTON_SELECT, INPUT);  // GPIO35 ist input-only
  pinMode(BUTTON_ACTION, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT), buttonSelectISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_ACTION), buttonActionISR, FALLING);

  // Display
  initDisplay();

  // BLE
  BLEDevice::init("ViClean-Simple");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("Suche ViClean...");
  pBLEScan->start(5, false);
}

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

  delay(10);
}
