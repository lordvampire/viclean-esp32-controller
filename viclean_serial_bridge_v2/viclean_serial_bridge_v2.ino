/*
 * ViClean Serial Bridge V2 - KORRIGIERTE VERSION
 *
 * KRITISCHE ERKENNTNISSE aus Reverse Engineering:
 * ✓ Werte müssen 1-6 sein (NICHT 0-5!) - 0 wird ignoriert!
 * ✓ OSC und ROT sind GEKOPPELT ("Harmonic Wave" aktiviert beide)
 * ✓ START-Befehl trennt BLE (normales Verhalten - Gerät startet Programm)
 *
 * TEST-BEFEHLE:
 *   start1    - Minimal (Lady, keine Features, 1-1-1)
 *   start2    - Standard (Lady, HarmonicWave, 3-3-3)
 *   start3    - Gent (Gent, HarmonicWave, 3-3-3)
 *   start4    - Maximal (Lady, HarmonicWave+Puls, 6-6-6)
 *   start5    - Nur Puls (Lady, Pulsation, 3-3-3)
 *   stop      - Programm stoppen
 *
 * EINZELBEFEHLE:
 *   temp <1-6>    - Temperatur
 *   pump <1-6>    - Wasserpumpe
 *   nozzle <1-6>  - Düsenposition
 *   harmonic      - Harmonic Wave (Osc+Rot)
 *   puls          - Pulsation
 *   hw            - Warmwasser
 *   buzz          - Buzzer
 *   help          - Hilfe
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

// ==================== Display ====================

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("ViClean Bridge V2");
  tft.println("KORRIGIERT: Werte 1-6!");
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

// ==================== BLE ====================

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

void sendCommand(String command, const char* description = "") {
  if (connected && pRemoteCharacteristicRX != nullptr) {
    if (strlen(description) > 0) {
      Serial.print("[INFO] ");
      Serial.println(description);
      updateDisplay(description, TFT_CYAN);
    }

    Serial.print("[TX] ");
    Serial.println(command);
    updateDisplay(("TX: " + command.substring(0, 18)).c_str(), TFT_GREEN);

    // WICHTIG: Packet-Splitting für Befehle >20 Bytes (wie App!)
    size_t cmdLen = command.length();

    if (cmdLen > 20) {
      Serial.printf("[SPLIT] Befehl zu lang (%d Bytes), splitte in Pakete...\n", cmdLen);

      // Berechne Anzahl der 20-Byte Blöcke
      int numBlocks = (cmdLen / 20) + 1;

      for (int i = 0; i < numBlocks; i++) {
        // Erstelle 20-Byte Block
        size_t blockStart = i * 20;
        size_t blockSize = min((size_t)20, cmdLen - blockStart);

        // Extrahiere Substring
        String block = command.substring(blockStart, blockStart + blockSize);

        Serial.printf("[PACKET %d/%d] Sende %d Bytes: ", i+1, numBlocks, blockSize);
        Serial.println(block);

        // Sende Paket
        pRemoteCharacteristicRX->writeValue(block.c_str(), blockSize);

        // Warte zwischen Paketen
        if (i < numBlocks - 1) {
          delay(50);  // 50ms zwischen Paketen
        }
      }

      delay(100); // Zusätzlich 100ms nach allen Paketen

    } else {
      // Befehl passt in ein Paket
      pRemoteCharacteristicRX->writeValue(command.c_str(), cmdLen);
      delay(100); // 100ms warten
    }

  } else {
    Serial.println("[FEHLER] Nicht verbunden!");
    updateDisplay("FEHLER: Nicht verbunden", TFT_RED);
  }
}

// ==================== Befehlsfunktionen ====================

void cmdSetTemperature(uint8_t temp) {
  if (temp < 1) temp = 1;  // Min = 1
  if (temp > 5) temp = 5;  // Max = 5 (NICHT 6! Gerät hat nur 5 Stufen)

  uint8_t data[] = {temp};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_TEMP, data, 1);

  char desc[30];
  sprintf(desc, "Temp: %d", temp);
  sendCommand(cmd, desc);
}

void cmdSetWaterPump(uint8_t pressure) {
  if (pressure < 1) pressure = 1;
  if (pressure > 6) pressure = 6;

  uint8_t data[] = {pressure};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_WATER_PUMP, data, 1);

  char desc[30];
  sprintf(desc, "Pumpe: %d", pressure);
  sendCommand(cmd, desc);
}

void cmdSetNozzlePosition(uint8_t position) {
  if (position < 1) position = 1;
  if (position > 6) position = 6;

  uint8_t data[] = {position};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_NOZZLE_POSITION, data, 1);

  char desc[30];
  sprintf(desc, "Duese: %d", position);
  sendCommand(cmd, desc);
}

void cmdStartProgram(uint8_t gender, uint8_t oscillate, uint8_t rotate,
                     uint8_t pulsate, uint8_t pumpstate, uint8_t nozzlestate,
                     uint8_t temperature, const char* description) {

  // WICHTIG: Werte 1-6 erzwingen
  if (pumpstate < 1) pumpstate = 1;
  if (nozzlestate < 1) nozzlestate = 1;
  if (temperature < 1) temperature = 1;

  if (pumpstate > 6) pumpstate = 6;
  if (nozzlestate > 6) nozzlestate = 6;
  if (temperature > 6) temperature = 6;

  uint8_t bitmask = ((gender + 1) & 0x03) |
                    ((oscillate & 0x01) << 2) |
                    ((rotate & 0x01) << 3) |
                    ((pulsate & 0x01) << 4);

  uint8_t data[] = {bitmask, pumpstate, nozzlestate, temperature};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  START-BEFEHL WIRD GESENDET        ║");
  Serial.println("╠════════════════════════════════════╣");
  Serial.printf("║ %s\n", description);
  Serial.printf("║ Gender:    %s\n", gender == 0 ? "Lady" : "Gent");
  Serial.printf("║ Oscillate: %s\n", oscillate ? "ON" : "OFF");
  Serial.printf("║ Rotate:    %s\n", rotate ? "ON" : "OFF");
  Serial.printf("║ Pulsate:   %s\n", pulsate ? "ON" : "OFF");
  Serial.printf("║ Pump:      %d\n", pumpstate);
  Serial.printf("║ Nozzle:    %d\n", nozzlestate);
  Serial.printf("║ Temp:      %d\n", temperature);
  Serial.printf("║ Bitmask:   0x%02X\n", bitmask);
  Serial.println("╚════════════════════════════════════╝");

  sendCommand(cmd, description);

  Serial.println("\n⚠️  HINWEIS: BLE-Verbindung wird gleich");
  Serial.println("           getrennt (normales Verhalten!)");
  Serial.println("           Warte auf Auto-Reconnect...\n");
}

void cmdBreakProgram() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM, nullptr, 0);
  sendCommand(cmd, "STOP Programm");
}

void cmdToggle(uint8_t toggleCmd, const char* name) {
  String cmd = createCommand(MODBUS_WRITE_COIL, toggleCmd, nullptr, 0);
  sendCommand(cmd, name);
}

void cmdToggleHarmonicWave() {
  // WARNUNG: OSC/ROT Toggle funktionieren NICHT einzeln!
  // Sie können nur beim START-Befehl gesetzt werden.
  Serial.println("\n⚠️  WARNUNG: Harmonic Wave kann nur beim START gesetzt werden!");
  Serial.println("   OSC/ROT können während Programm NICHT geändert werden.");
  Serial.println("   Verwende start2/start3/start4 für HarmonicWave.");
}

// ==================== BLE Callbacks ====================

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {

  Serial.print("[RX] ");
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
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║         VERBUNDEN!                 ║");
    Serial.println("╚════════════════════════════════════╝\n");
    updateDisplay("=== VERBUNDEN ===", TFT_GREEN);
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║         GETRENNT                   ║");
    Serial.println("╚════════════════════════════════════╝\n");
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

// ==================== Befehlsparser ====================

void processCommand(String cmd_str) {
  cmd_str.trim();
  String cmd_original = cmd_str;  // Original für RAW-Befehle bewahren!
  cmd_str.toLowerCase();

  Serial.print("\n>>> ");
  Serial.println(cmd_original);  // Original anzeigen (mit Großbuchstaben)

  if (cmd_str == "help" || cmd_str == "?") {
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║    ViClean Befehle V3 (NEU!)          ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.println("║ START-TESTS (setzt zuerst Werte!):    ║");
    Serial.println("║ start1      - Minimal (L,---,1-1-1)    ║");
    Serial.println("║ start2      - Standard (L,HW,3-3-3)    ║");
    Serial.println("║ start3      - Gent (G,HW,3-3-3)        ║");
    Serial.println("║ start4      - Maximal (L,HW+P,6-6-6)   ║");
    Serial.println("║ start5      - Puls (L,P,3-3-3)         ║");
    Serial.println("║ startdirect - START ohne Vorbereitung  ║");
    Serial.println("║                                        ║");
    Serial.println("║ EXPERIMENTELLE TESTS:                  ║");
    Serial.println("║ teststart1  - Nur Bitmask, Rest 0      ║");
    Serial.println("║ teststart2  - Alle Werte = 1           ║");
    Serial.println("║ teststart3  - 5 Parameter-Version      ║");
    Serial.println("║ teststart4  - 7 Parameter einzeln      ║");
    Serial.println("║ teststart5  - Adresse 0145 (HW 3-3-3)  ║");
    Serial.println("║ teststart6  - Adresse 0145 (1-1-1)     ║");
    Serial.println("║                                        ║");
    Serial.println("║ SCHNELL-TESTS:                         ║");
    Serial.println("║ t1-t10      - Einzelne Test-Varianten  ║");
    Serial.println("║ autotest    - Alle Tests automatisch   ║");
    Serial.println("║ stop        - Programm stoppen         ║");
    Serial.println("║                                        ║");
    Serial.println("║ EINZELBEFEHLE (während Programm):      ║");
    Serial.println("║ temp <1-5>    - Temperatur (max 5!)    ║");
    Serial.println("║ pump <1-6>    - Wasserpumpe            ║");
    Serial.println("║ nozzle <1-6>  - Düsenposition          ║");
    Serial.println("║ puls          - Pulsation toggle       ║");
    Serial.println("║ hw            - Warmwasser toggle      ║");
    Serial.println("║ buzz          - Buzzer toggle          ║");
    Serial.println("║                                        ║");
    Serial.println("║ SONSTIGES:                             ║");
    Serial.println("║ raw <cmd>     - Raw-Befehl (auto \\r\\n)  ║");
    Serial.println("║ help          - Diese Hilfe            ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println("\n⚠️  WICHTIG:");
    Serial.println("   - Temp hat nur 5 Stufen (1-5)!");
    Serial.println("   - Pump/Nozzle haben 6 Stufen (1-6)");
    Serial.println("   - OSC/ROT können NUR beim START");
    Serial.println("     gesetzt werden, NICHT während Programm!");
    Serial.println("   - START trennt BLE (normal)");
    return;
  }

  // TEMPERATURE
  if (cmd_str.startsWith("temp ")) {
    int val = cmd_str.substring(5).toInt();
    cmdSetTemperature(val);
  }

  // PUMP
  else if (cmd_str.startsWith("pump ")) {
    int val = cmd_str.substring(5).toInt();
    cmdSetWaterPump(val);
  }

  // NOZZLE
  else if (cmd_str.startsWith("nozzle ")) {
    int val = cmd_str.substring(7).toInt();
    cmdSetNozzlePosition(val);
  }

  // ========== START-VARIANTEN ==========
  // Neue Strategie: Erst Werte setzen, DANN START!

  else if (cmd_str == "start1") {
    Serial.println("\n=== START1: Minimal (1-1-1) ===");
    Serial.println("Setze Werte in APP-REIHENFOLGE (Nozzle->Temp->Pump)...");
    cmdSetNozzlePosition(1);  // 1. NOZZLE (wie App!)
    delay(300);
    cmdSetTemperature(1);     // 2. TEMP
    delay(300);
    cmdSetWaterPump(1);       // 3. PUMP
    delay(300);
    Serial.println("Sende jetzt START...");
    cmdStartProgram(0, 0, 0, 0, 1, 1, 1,
                    "START1: Minimal (Lady, 1-1-1)");
  }

  else if (cmd_str == "start2") {
    Serial.println("\n=== START2: Standard (3-3-3 + HW) ===");
    Serial.println("Setze Werte in APP-REIHENFOLGE (Nozzle->Temp->Pump)...");
    cmdSetNozzlePosition(3);  // 1. NOZZLE (wie App!)
    delay(300);
    cmdSetTemperature(3);     // 2. TEMP
    delay(300);
    cmdSetWaterPump(3);       // 3. PUMP
    delay(300);
    Serial.println("Sende jetzt START...");
    cmdStartProgram(0, 1, 1, 0, 3, 3, 3,
                    "START2: Standard Lady + HarmonicWave");
  }

  else if (cmd_str == "start3") {
    Serial.println("\n=== START3: Gent (3-3-3 + HW) ===");
    Serial.println("Setze Werte in APP-REIHENFOLGE (Nozzle->Temp->Pump)...");
    cmdSetNozzlePosition(3);  // 1. NOZZLE (wie App!)
    delay(300);
    cmdSetTemperature(3);     // 2. TEMP
    delay(300);
    cmdSetWaterPump(3);       // 3. PUMP
    delay(300);
    Serial.println("Sende jetzt START...");
    cmdStartProgram(1, 1, 1, 0, 3, 3, 3,
                    "START3: Standard Gent + HarmonicWave");
  }

  else if (cmd_str == "start4") {
    Serial.println("\n=== START4: MAXIMAL (6-6-6 + HW + Puls) ===");
    Serial.println("Setze Werte in APP-REIHENFOLGE (Nozzle->Temp->Pump)...");
    cmdSetNozzlePosition(6);  // 1. NOZZLE (wie App!)
    delay(300);
    cmdSetTemperature(6);     // 2. TEMP
    delay(300);
    cmdSetWaterPump(6);       // 3. PUMP
    delay(300);
    Serial.println("Sende jetzt START...");
    cmdStartProgram(0, 1, 1, 1, 6, 6, 6,
                    "START4: MAXIMAL (Lady, HW+Puls, 6-6-6)");
  }

  else if (cmd_str == "start5") {
    Serial.println("\n=== START5: Pulsation (3-3-3 + Puls) ===");
    Serial.println("Setze Werte in APP-REIHENFOLGE (Nozzle->Temp->Pump)...");
    cmdSetNozzlePosition(3);  // 1. NOZZLE (wie App!)
    delay(300);
    cmdSetTemperature(3);     // 2. TEMP
    delay(300);
    cmdSetWaterPump(3);       // 3. PUMP
    delay(300);
    Serial.println("Sende jetzt START...");
    cmdStartProgram(0, 0, 0, 1, 3, 3, 3,
                    "START5: Nur Pulsation (Lady, 3-3-3)");
  }

  // Direkt-START ohne vorheriges Setzen (zum Vergleich)
  else if (cmd_str == "startdirect") {
    Serial.println("\n=== START DIREKT (ohne vorher Werte zu setzen) ===");
    cmdStartProgram(0, 1, 1, 0, 3, 3, 3,
                    "DIRECT: Standard ohne Vorbereitung");
  }

  // TEST: Verschiedene Parameter-Kombinationen
  else if (cmd_str == "teststart1") {
    Serial.println("\n=== TEST: Nur Bitmask ohne Werte ===");
    uint8_t bitmask = 0x01;  // Nur gender=0
    uint8_t data[] = {bitmask, 0, 0, 0};
    String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);
    sendCommand(cmd, "TEST: Bitmask+0");
  }

  else if (cmd_str == "teststart2") {
    Serial.println("\n=== TEST: Alle Werte = 1 ===");
    uint8_t data[] = {1, 1, 1, 1};
    String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);
    sendCommand(cmd, "TEST: Alle 1");
  }

  else if (cmd_str == "teststart3") {
    Serial.println("\n=== TEST: 5 Parameter wie TEST-Funktion ===");
    uint8_t data[] = {1, 1, 1, 1, 3};
    String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 5);
    sendCommand(cmd, "TEST: 5 Parameter");
  }

  else if (cmd_str == "teststart4") {
    Serial.println("\n=== TEST: Ohne Bitmask, alle einzeln ===");
    uint8_t data[] = {0, 0, 0, 0, 3, 3, 3};  // gender, osc, rot, puls, pump, nozzle, temp
    String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 7);
    sendCommand(cmd, "TEST: 7 Parameter einzeln");
  }

  // TEST mit Geräte-Adresse 0145 statt FFFF
  else if (cmd_str == "teststart5") {
    Serial.println("\n=== TEST: Mit Adresse 0145 statt FFFF ===");
    // Manuell Befehl bauen mit Adresse 0145
    String cmd = ":0145058E0D0303030F\r\n";  // Adresse 0145, Standard Lady+HW
    // LRC muss neu berechnet werden!
    String datapart = "0145058E0D030303";
    uint8_t lrc = calculateLRC(datapart.c_str(), datapart.length());
    char lrcHex[3];
    sprintf(lrcHex, "%02X", lrc);
    cmd = ":0145058E0D030303";
    cmd += lrcHex;
    cmd += "\r\n";
    sendCommand(cmd, "TEST: Adresse 0145");
  }

  else if (cmd_str == "teststart6") {
    Serial.println("\n=== TEST: Mit Adresse 0145 + alle Werte = 1 ===");
    String datapart = "0145058E01010101";
    uint8_t lrc = calculateLRC(datapart.c_str(), datapart.length());
    char lrcHex[3];
    sprintf(lrcHex, "%02X", lrc);
    String cmd = ":";
    cmd += datapart;
    cmd += lrcHex;
    cmd += "\r\n";
    sendCommand(cmd, "TEST: Adresse 0145 + 1-1-1");
  }

  // AUTOMATISCHE TEST-SUITE
  else if (cmd_str == "autotest") {
    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   AUTOMATISCHE START-BEFEHL TESTS      ║");
    Serial.println("║   Testet verschiedene Kombinationen    ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();

    // Array mit Test-Befehlen
    struct TestCmd {
      const char* name;
      uint8_t params[7];
      int paramCount;
    };

    TestCmd tests[] = {
      {"4P: Alle 0", {0x00, 0x00, 0x00, 0x00}, 4},
      {"4P: Alle 1", {0x01, 0x01, 0x01, 0x01}, 4},
      {"4P: Standard (0D-3-3-3)", {0x0D, 0x03, 0x03, 0x03}, 4},
      {"4P: Nur Gender (01-0-0-0)", {0x01, 0x00, 0x00, 0x00}, 4},
      {"4P: Nur Pump (00-3-0-0)", {0x00, 0x03, 0x00, 0x00}, 4},
      {"5P: TEST-Funktion (1-1-1-1-3)", {0x01, 0x01, 0x01, 0x01, 0x03}, 5},
      {"5P: Alle 1", {0x01, 0x01, 0x01, 0x01, 0x01}, 5},
      {"5P: Standard+1 (0D-3-3-3-1)", {0x0D, 0x03, 0x03, 0x03, 0x01}, 5},
      {"3P: Ohne Bitmask (3-3-3)", {0x03, 0x03, 0x03}, 3},
      {"6P: Mit führ. 0 (0-0D-3-3-3)", {0x00, 0x0D, 0x03, 0x03, 0x03}, 5}
    };

    int testCount = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < testCount; i++) {
      Serial.println();
      Serial.printf("═══ Test %d/%d: %s ═══\n", i+1, testCount, tests[i].name);

      // Befehl erstellen
      uint8_t data[7];
      for (int j = 0; j < tests[i].paramCount; j++) {
        data[j] = tests[i].params[j];
      }

      String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, tests[i].paramCount);

      // Parameter anzeigen
      Serial.print("Parameter: ");
      for (int j = 0; j < tests[i].paramCount; j++) {
        Serial.printf("%02X ", tests[i].params[j]);
      }
      Serial.println();

      sendCommand(cmd, tests[i].name);

      // Warten auf Antwort
      delay(2000);

      Serial.println("Drücke Enter für nächsten Test...");

      // Warte auf Enter
      while (Serial.available() == 0) {
        delay(100);
      }
      // Leere Buffer
      while (Serial.available() > 0) {
        Serial.read();
      }
    }

    Serial.println();
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   ALLE TESTS ABGESCHLOSSEN!            ║");
    Serial.println("╚════════════════════════════════════════╝");
  }

  // Einzelne manuelle RAW-Tests
  else if (cmd_str == "t1") { sendCommand(createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, (uint8_t[]){0x00,0x00,0x00,0x00}, 4), "T1: Alle 0"); }
  else if (cmd_str == "t2") { sendCommand(createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, (uint8_t[]){0x01,0x01,0x01,0x01}, 4), "T2: Alle 1"); }
  else if (cmd_str == "t3") { sendCommand(createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, (uint8_t[]){0x0D,0x03,0x03,0x03}, 4), "T3: Standard"); }
  else if (cmd_str == "t4") { sendCommand(createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, (uint8_t[]){0x01,0x00,0x00,0x00}, 4), "T4: Nur Gender"); }
  else if (cmd_str == "t5") { sendCommand(createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, (uint8_t[]){0x01,0x01,0x01,0x01,0x03}, 5), "T5: 5 Param"); }
  else if (cmd_str == "t6") { sendCommand(createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, (uint8_t[]){0x03,0x03,0x03}, 3), "T6: 3 Param"); }
  else if (cmd_str == "t7") { sendCommand(createCommand(0x06, CMD_START_PROGRAM, (uint8_t[]){0x0D,0x03,0x03,0x03}, 4), "T7: Write Reg"); }
  else if (cmd_str == "t8") { sendCommand(createCommand(MODBUS_WRITE_COIL, 0x70, (uint8_t[]){0x03}, 1), "T8: START_CW_NOZZLE"); }
  else if (cmd_str == "t9") { sendCommand(createCommand(MODBUS_WRITE_COIL, 0x71, (uint8_t[]){0x03}, 1), "T9: START_CCW_NOZZLE"); }
  else if (cmd_str == "t10") { sendCommand(createCommand(MODBUS_WRITE_COIL, 0x72, (uint8_t[]){0x03}, 1), "T10: START_CW_WIGGLE"); }

  // STOP
  else if (cmd_str == "stop") {
    cmdBreakProgram();
  }

  // HARMONIC WAVE (Osc + Rot zusammen!)
  else if (cmd_str == "harmonic") {
    cmdToggleHarmonicWave();
  }

  // PULSATION
  else if (cmd_str == "puls") {
    cmdToggle(CMD_TOGGLE_PULSATING, "Toggle Pulsation");
  }

  // WARMWASSER
  else if (cmd_str == "hw") {
    cmdToggle(CMD_TOGGLE_HW, "Toggle Warmwasser");
  }

  // BUZZER
  else if (cmd_str == "buzz") {
    cmdToggle(CMD_TOGGLE_BUZZER, "Toggle Buzzer");
  }

  // WARNUNG für einzelne OSC/ROT
  else if (cmd_str == "osc" || cmd_str == "rot") {
    Serial.println("\n⚠️  WARNUNG: OSC/ROT einzeln werden ignoriert!");
    Serial.println("   Verwende 'harmonic' für Osc+Rot zusammen");
  }

  // RAW - Original-String verwenden (mit Großbuchstaben!)
  else if (cmd_str.startsWith("raw ")) {
    String rawCmd = cmd_original.substring(4);  // Original verwenden!
    rawCmd.trim();  // Leerzeichen entfernen

    // \r\n automatisch anhängen wenn nicht vorhanden
    if (!rawCmd.endsWith("\r\n")) {
      rawCmd += "\r\n";
    }

    Serial.print("Sende RAW (mit \\r\\n): ");
    Serial.println(rawCmd);
    sendCommand(rawCmd, "RAW-Befehl");
  }

  else {
    Serial.println("Unbekannter Befehl. Tippe 'help'");
  }
}

// ==================== Setup & Loop ====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║   ViClean Serial Bridge V2             ║");
  Serial.println("║   KORRIGIERTE VERSION                  ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ Wichtige Änderungen:                   ║");
  Serial.println("║ ✓ Werte 1-6 (nicht 0-5!)               ║");
  Serial.println("║ ✓ HarmonicWave = Osc+Rot zusammen      ║");
  Serial.println("║ ✓ 5 START-Varianten zum Testen         ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  initDisplay();

  BLEDevice::init("ViClean-Bridge-V2");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("Suche ViClean...");
  updateDisplay("Suche ViClean...", TFT_CYAN);
  pBLEScan->start(5, false);

  Serial.println("\nTippe 'help' für Befehle\n");
}

void loop() {
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("Bereit für Befehle!");
      Serial.println("Tippe 'help' für Hilfe\n");
      updateDisplay("Bereit!", TFT_GREEN);
    }
    doConnect = false;
  }

  if (!connected && doScan) {
    delay(1000);
    Serial.println("Erneuter Scan...");
    BLEDevice::getScan()->start(0);
  }

  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  delay(10);
}
