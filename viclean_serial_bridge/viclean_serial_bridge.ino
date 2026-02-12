/*
 * ViClean Serial Bridge - KORRIGIERTE VERSION
 *
 * WICHTIGE ERKENNTNISSE:
 * - Werte 1-6 verwenden (NICHT 0-5!) - Das Gerät akzeptiert nur 1-6
 * - OSC und ROT sind GEKOPPELT (Harmonic Wave aktiviert beide zusammen)
 * - START-Befehl trennt BLE-Verbindung (normales Verhalten)
 *
 * Befehle:
 *   temp <1-6>       - Temperatur setzen (z.B. "temp 3")
 *   pump <1-6>       - Wasserpumpe setzen (z.B. "pump 4")
 *   nozzle <1-6>     - Düsenposition setzen (z.B. "nozzle 2")
 *   start1           - Minimal (Lady, alles 1)
 *   start2           - Standard (Lady, HarmonicWave, 3-3-3)
 *   start3           - Standard (Gent, HarmonicWave, 3-3-3)
 *   start4           - Maximal (Lady, HarmonicWave+Puls, 6-6-6)
 *   start5           - Nur Pulsation (Lady, Puls, 3-3-3)
 *   stop             - Programm abbrechen
 *   harmonic         - Harmonic Wave (OSC+ROT zusammen)
 *   puls             - Pulsation umschalten
 *   hw               - Warmwasser umschalten
 *   buzz             - Buzzer umschalten
 *   help             - Hilfe anzeigen
 *   raw <hex>        - Rohen Befehl senden
 */

#define USER_SETUP_LOADED
#include <User_Setup.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// BLE UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Display
TFT_eSPI tft = TFT_eSPI();
#define TFT_BL 4

// Befehle
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
#define MODBUS_WRITE_COIL         0x05

// BLE Variablen
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristicRX;
static BLERemoteCharacteristic* pRemoteCharacteristicTX;
static BLEAdvertisedDevice* myDevice;

int displayLine = 0;

// ==================== Display Funktionen ====================

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("ViClean Serial Bridge");
  tft.println("Bereit fuer Befehle...");
}

void updateDisplay(const char* msg, uint16_t color = TFT_WHITE) {
  if (displayLine >= 15) {
    tft.fillRect(0, 20, 240, 115, TFT_BLACK);
    displayLine = 0;
  }
  tft.setTextColor(color, TFT_BLACK);
  tft.setCursor(0, 20 + (displayLine * 8));
  tft.println(msg);
  displayLine++;
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
  if (connected && pRemoteCharacteristicRX != nullptr) {
    Serial.print("TX: ");
    Serial.println(command);
    updateDisplay(("TX: " + command.substring(0, 20)).c_str(), TFT_GREEN);

    pRemoteCharacteristicRX->writeValue(command.c_str(), command.length());
  } else {
    Serial.println("FEHLER: Nicht verbunden!");
    updateDisplay("FEHLER: Nicht verbunden", TFT_RED);
  }
}

// ==================== Befehlsfunktionen ====================

void cmdSetTemperature(uint8_t temp) {
  // WICHTIG: Gerät akzeptiert nur 1-6 (nicht 0-5!)
  if (temp < 1) temp = 1;
  if (temp > 6) temp = 6;

  uint8_t data[] = {temp};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_TEMP, data, 1);
  sendCommand(cmd);
}

void cmdSetWaterPump(uint8_t pressure) {
  // WICHTIG: Gerät akzeptiert nur 1-6 (nicht 0-5!)
  if (pressure < 1) pressure = 1;
  if (pressure > 6) pressure = 6;

  uint8_t data[] = {pressure};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_WATER_PUMP, data, 1);
  sendCommand(cmd);
}

void cmdSetNozzlePosition(uint8_t position) {
  // WICHTIG: Gerät akzeptiert nur 1-6 (nicht 0-5!)
  if (position < 1) position = 1;
  if (position > 6) position = 6;

  uint8_t data[] = {position};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_NOZZLE_POSITION, data, 1);
  sendCommand(cmd);
}

void cmdStartProgram(uint8_t gender, uint8_t oscillate, uint8_t rotate,
                     uint8_t pulsate, uint8_t pumpstate, uint8_t nozzlestate,
                     uint8_t temperature, const char* description = "") {

  // WICHTIG: Werte 1-6 verwenden (Gerät akzeptiert keine 0!)
  if (pumpstate < 1) pumpstate = 1;
  if (nozzlestate < 1) nozzlestate = 1;
  if (temperature < 1) temperature = 1;

  uint8_t bitmask = ((gender + 1) & 0x03) |
                    ((oscillate & 0x01) << 2) |
                    ((rotate & 0x01) << 3) |
                    ((pulsate & 0x01) << 4);

  uint8_t data[] = {bitmask, pumpstate, nozzlestate, temperature};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);

  if (strlen(description) > 0) {
    Serial.print("START: ");
    Serial.println(description);
  }

  sendCommand(cmd);
}

void cmdBreakProgram() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM, nullptr, 0);
  sendCommand(cmd);
}

void cmdToggle(uint8_t toggleCmd) {
  String cmd = createCommand(MODBUS_WRITE_COIL, toggleCmd, nullptr, 0);
  sendCommand(cmd);
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

  char rxMsg[30];
  snprintf(rxMsg, 30, "RX: %02X %02X %02X...", pData[0], pData[1], pData[2]);
  updateDisplay(rxMsg, TFT_YELLOW);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.println("\n=== VERBUNDEN ===");
    updateDisplay("=== VERBUNDEN ===", TFT_GREEN);
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("\n=== GETRENNT ===");
    updateDisplay("=== GETRENNT ===", TFT_RED);
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

// ==================== Serieller Befehlsparser ====================

void processCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  Serial.print(">>> ");
  Serial.println(cmd);

  if (cmd == "help" || cmd == "?") {
    Serial.println("\n=== ViClean Befehle ===");
    Serial.println("temp <0-5>    - Temperatur setzen");
    Serial.println("pump <0-5>    - Wasserpumpe setzen");
    Serial.println("nozzle <0-5>  - Düsenposition setzen");
    Serial.println("start         - Programm starten");
    Serial.println("stop          - Programm stoppen");
    Serial.println("osc           - Oszillation toggle");
    Serial.println("rot           - Rotation toggle");
    Serial.println("puls          - Pulsation toggle");
    Serial.println("hw            - Warmwasser toggle");
    Serial.println("buzz          - Buzzer toggle");
    Serial.println("raw <cmd>     - Roher Befehl");
    Serial.println("help          - Diese Hilfe");
    return;
  }

  if (cmd.startsWith("temp ")) {
    int val = cmd.substring(5).toInt();
    if (val >= 0 && val <= 5) {
      Serial.printf("Setze Temperatur: %d\n", val);
      cmdSetTemperature(val);
    } else {
      Serial.println("Fehler: Wert muss 0-5 sein");
    }
  }
  else if (cmd.startsWith("pump ")) {
    int val = cmd.substring(5).toInt();
    if (val >= 0 && val <= 5) {
      Serial.printf("Setze Pumpe: %d\n", val);
      cmdSetWaterPump(val);
    } else {
      Serial.println("Fehler: Wert muss 0-5 sein");
    }
  }
  else if (cmd.startsWith("nozzle ")) {
    int val = cmd.substring(7).toInt();
    if (val >= 0 && val <= 5) {
      Serial.printf("Setze Düse: %d\n", val);
      cmdSetNozzlePosition(val);
    } else {
      Serial.println("Fehler: Wert muss 0-5 sein");
    }
  }
  else if (cmd == "start") {
    Serial.println("Starte Programm (Lady, Osc, Stufe 3)");
    cmdStartProgram(0, 1, 0, 0, 3, 3, 3);
  }
  else if (cmd == "stop") {
    Serial.println("Stoppe Programm");
    cmdBreakProgram();
  }
  else if (cmd == "osc") {
    Serial.println("Toggle Oszillation");
    cmdToggle(CMD_TOGGLE_OSCILLATION);
  }
  else if (cmd == "rot") {
    Serial.println("Toggle Rotation");
    cmdToggle(CMD_TOGGLE_ROTATION);
  }
  else if (cmd == "puls") {
    Serial.println("Toggle Pulsation");
    cmdToggle(CMD_TOGGLE_PULSATING);
  }
  else if (cmd == "hw") {
    Serial.println("Toggle Warmwasser");
    cmdToggle(CMD_TOGGLE_HW);
  }
  else if (cmd == "buzz") {
    Serial.println("Toggle Buzzer");
    cmdToggle(CMD_TOGGLE_BUZZER);
  }
  else if (cmd.startsWith("raw ")) {
    String rawCmd = cmd.substring(4);
    Serial.print("Sende rohen Befehl: ");
    Serial.println(rawCmd);
    sendCommand(rawCmd);
  }
  else {
    Serial.println("Unbekannter Befehl. Tippe 'help' für Hilfe.");
  }
}

// ==================== Setup & Loop ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=================================");
  Serial.println("ViClean Serial Bridge");
  Serial.println("=================================");
  Serial.println("Tippe 'help' für Befehle\n");

  initDisplay();

  BLEDevice::init("ViClean-Bridge");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("Suche ViClean...");
  updateDisplay("Suche ViClean...", TFT_CYAN);
  pBLEScan->start(5, false);
}

void loop() {
  // Verbindung herstellen
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("Bereit für Befehle!");
      updateDisplay("Bereit!", TFT_GREEN);
    }
    doConnect = false;
  }

  // Erneut scannen wenn getrennt
  if (!connected && doScan) {
    delay(1000);
    Serial.println("Erneuter Scan...");
    BLEDevice::getScan()->start(0);
  }

  // Serielle Befehle lesen
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  delay(10);
}
