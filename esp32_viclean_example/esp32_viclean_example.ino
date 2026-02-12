/*
 * ViClean DuschWC ESP32 BLE Controller
 *
 * Dieses Beispiel zeigt, wie man das ViClean DuschWC über BLE steuert
 *
 * Benötigte Hardware:
 *   - ESP32 Development Board
 *
 * Benötigte Bibliotheken:
 *   - ESP32 BLE Arduino (in Board-Manager enthalten)
 *
 * Verwendung:
 *   1. Code auf ESP32 hochladen
 *   2. Seriellen Monitor öffnen (115200 Baud)
 *   3. ESP32 sucht nach ViClean-Geräten
 *   4. Verbindet automatisch und sendet Testbefehle
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ViClean NUS (Nordic UART Service) UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Write
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Notify

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
#define CMD_NOZZLE_MAINTENANCE    0x82

// Modbus-Befehle
#define MODBUS_READ_REGISTER   0x03
#define MODBUS_WRITE_COIL      0x05
#define MODBUS_WRITE_REGISTER  0x06

// Globale Variablen
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristicRX;
static BLERemoteCharacteristic* pRemoteCharacteristicTX;
static BLEAdvertisedDevice* myDevice;

/**
 * Berechnet die LRC (Longitudinal Redundancy Check) Checksum
 */
uint8_t calculateLRC(const char* data, size_t length) {
  uint8_t lrc = 0;
  for (size_t i = 0; i < length; i++) {
    lrc ^= (uint8_t)data[i];
  }
  return lrc;
}

/**
 * Erstellt einen vollständigen ViClean-Befehl
 *
 * @param modbusCmd Modbus-Befehlscode (0x03, 0x05, 0x06)
 * @param subCmd ViClean-Subbefehl
 * @param data Datenbytes (kann NULL sein)
 * @param dataLen Anzahl der Datenbytes
 * @return Vollständiger Befehl als String
 */
String createCommand(uint8_t modbusCmd, uint8_t subCmd, uint8_t* data, size_t dataLen) {
  String command = ":FFFF";
  char hex[3];

  // Modbus-Befehl
  sprintf(hex, "%02X", modbusCmd);
  command += hex;

  // Subbefehl
  sprintf(hex, "%02X", subCmd);
  command += hex;

  // Daten anhängen
  for (size_t i = 0; i < dataLen; i++) {
    sprintf(hex, "%02X", data[i]);
    command += hex;
  }

  // LRC berechnen (auf ASCII-Bytes, ohne ':')
  String commandForLRC = command.substring(1); // ':' entfernen
  uint8_t lrc = calculateLRC(commandForLRC.c_str(), commandForLRC.length());

  // LRC anhängen
  sprintf(hex, "%02X", lrc);
  command += hex;

  // CRLF anhängen
  command += "\r\n";

  return command;
}

/**
 * Sendet einen Befehl an das ViClean-Gerät
 */
void sendCommand(String command) {
  if (connected && pRemoteCharacteristicRX != nullptr) {
    Serial.print("Sende Befehl: ");
    Serial.print(command);

    pRemoteCharacteristicRX->writeValue(command.c_str(), command.length());

    Serial.println(" -> Gesendet!");
  } else {
    Serial.println("Fehler: Nicht verbunden!");
  }
}

// ==================== Befehlsfunktionen ====================

/**
 * Startet ein Waschprogramm mit allen Parametern
 */
void cmdStartProgram(uint8_t gender, uint8_t oscillate, uint8_t rotate,
                     uint8_t pulsate, uint8_t pumpstate, uint8_t nozzlestate,
                     uint8_t temperature) {
  // Bitmask berechnen
  uint8_t bitmask = ((gender + 1) & 0x03) |
                    ((oscillate & 0x01) << 2) |
                    ((rotate & 0x01) << 3) |
                    ((pulsate & 0x01) << 4);

  uint8_t data[] = {bitmask, pumpstate, nozzlestate, temperature};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_START_PROGRAM, data, 4);
  sendCommand(cmd);
}

/**
 * Bricht das aktuelle Programm ab
 */
void cmdBreakProgram() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_BREAK_PROGRAM, nullptr, 0);
  sendCommand(cmd);
}

/**
 * Setzt die Düsenposition (0-5)
 */
void cmdSetNozzlePosition(uint8_t position) {
  uint8_t data[] = {position};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_NOZZLE_POSITION, data, 1);
  sendCommand(cmd);
}

/**
 * Setzt die Temperatur (0-5)
 */
void cmdSetTemperature(uint8_t temp) {
  uint8_t data[] = {temp};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_TEMP, data, 1);
  sendCommand(cmd);
}

/**
 * Setzt den Wasserdruck (0-5)
 */
void cmdSetWaterPump(uint8_t pressure) {
  uint8_t data[] = {pressure};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_SET_WATER_PUMP, data, 1);
  sendCommand(cmd);
}

/**
 * Schaltet Oszillation um
 */
void cmdToggleOscillation() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_OSCILLATION, nullptr, 0);
  sendCommand(cmd);
}

/**
 * Schaltet Rotation um
 */
void cmdToggleRotation() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_ROTATION, nullptr, 0);
  sendCommand(cmd);
}

/**
 * Schaltet Pulsation um
 */
void cmdTogglePulsating() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_PULSATING, nullptr, 0);
  sendCommand(cmd);
}

/**
 * Schaltet Warmwasser um
 */
void cmdToggleHW() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_HW, nullptr, 0);
  sendCommand(cmd);
}

/**
 * Schaltet den Buzzer um
 */
void cmdToggleBuzzer() {
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_TOGGLE_BUZZER, nullptr, 0);
  sendCommand(cmd);
}

/**
 * Aktiviert den Düsen-Wartungsmodus
 */
void cmdNozzleMaintenance() {
  uint8_t data[] = {0x01, 0x00};
  String cmd = createCommand(MODBUS_WRITE_COIL, CMD_NOZZLE_MAINTENANCE, data, 2);
  sendCommand(cmd);
}

// ==================== BLE Callbacks ====================

/**
 * Callback für empfangene Benachrichtigungen
 */
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  Serial.print("Benachrichtigung empfangen: ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();
}

/**
 * BLE Client-Callback-Klasse
 */
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("Verbunden!");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Verbindung getrennt!");
  }
};

/**
 * Verbindet zu einem ViClean-Gerät
 */
bool connectToServer() {
  Serial.print("Verbinde zu Gerät: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Client erstellt");

  pClient->setClientCallbacks(new MyClientCallback());

  // Verbindung herstellen
  pClient->connect(myDevice);
  Serial.println(" - Verbunden zum Server");

  // NUS Service suchen
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.print("Fehler: NUS Service nicht gefunden: ");
    Serial.println(SERVICE_UUID);
    pClient->disconnect();
    return false;
  }
  Serial.println(" - NUS Service gefunden");

  // RX Characteristic (zum Schreiben)
  pRemoteCharacteristicRX = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
  if (pRemoteCharacteristicRX == nullptr) {
    Serial.print("Fehler: RX Characteristic nicht gefunden: ");
    Serial.println(CHARACTERISTIC_UUID_RX);
    pClient->disconnect();
    return false;
  }
  Serial.println(" - RX Characteristic gefunden");

  // TX Characteristic (für Benachrichtigungen)
  pRemoteCharacteristicTX = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_TX);
  if (pRemoteCharacteristicTX == nullptr) {
    Serial.print("Fehler: TX Characteristic nicht gefunden: ");
    Serial.println(CHARACTERISTIC_UUID_TX);
    pClient->disconnect();
    return false;
  }
  Serial.println(" - TX Characteristic gefunden");

  // Benachrichtigungen aktivieren
  if (pRemoteCharacteristicTX->canNotify()) {
    pRemoteCharacteristicTX->registerForNotify(notifyCallback);
    Serial.println(" - Benachrichtigungen aktiviert");
  }

  connected = true;
  return true;
}

/**
 * Scan-Callback-Klasse
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE-Gerät gefunden: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Prüfen ob es ein NUS-Gerät ist
    if (advertisedDevice.haveServiceUUID() &&
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {

      Serial.println("ViClean-Gerät mit NUS gefunden!");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

// ==================== Setup & Loop ====================

void setup() {
  Serial.begin(115200);
  Serial.println("ViClean ESP32 BLE Controller startet...");

  BLEDevice::init("");

  // BLE-Scan konfigurieren
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  Serial.println("Scanne nach ViClean-Geräten...");
}

void loop() {
  // Verbindung herstellen, wenn Gerät gefunden wurde
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("Erfolgreich verbunden!");

      // Warte kurz, dann sende Testbefehle
      delay(2000);

      // ==================== TESTBEISPIELE ====================

      Serial.println("\n=== Teste Befehle ===\n");

      // Beispiel 1: Temperatur setzen
      Serial.println("Test 1: Temperatur auf Stufe 3 setzen");
      cmdSetTemperature(3);
      delay(1000);

      // Beispiel 2: Wasserdruck setzen
      Serial.println("Test 2: Wasserdruck auf Stufe 2 setzen");
      cmdSetWaterPump(2);
      delay(1000);

      // Beispiel 3: Programm starten
      Serial.println("Test 3: Programm starten (Lady, Oszillation)");
      cmdStartProgram(
        0,  // gender: 0=Lady
        1,  // oscillate: On
        0,  // rotate: Off
        0,  // pulsate: Off
        3,  // pumpstate: Stufe 3
        3,  // nozzlestate: Stufe 3
        3   // temperature: Stufe 3
      );
      delay(5000);

      // Beispiel 4: Programm abbrechen
      Serial.println("Test 4: Programm abbrechen");
      cmdBreakProgram();
      delay(1000);

      Serial.println("\n=== Tests abgeschlossen ===\n");

    } else {
      Serial.println("Verbindung fehlgeschlagen!");
    }
    doConnect = false;
  }

  // Wenn Verbindung getrennt wurde, erneut scannen
  if (!connected && doScan) {
    Serial.println("Starte erneuten Scan...");
    BLEDevice::getScan()->start(0);
  }

  delay(1000);
}

// ==================== Serielle Konsolen-Steuerung (Optional) ====================

/*
 * Optional: Befehle über den Seriellen Monitor senden
 *
 * Füge dies in die loop() Funktion ein:
 *
 * if (Serial.available() > 0) {
 *   char cmd = Serial.read();
 *   switch(cmd) {
 *     case '1': cmdSetTemperature(3); break;
 *     case '2': cmdSetWaterPump(3); break;
 *     case '3': cmdToggleOscillation(); break;
 *     case '4': cmdToggleRotation(); break;
 *     case '5': cmdTogglePulsating(); break;
 *     case 's': cmdStartProgram(0, 1, 0, 1, 3, 3, 3); break;
 *     case 'b': cmdBreakProgram(); break;
 *     default: Serial.println("Unbekannter Befehl"); break;
 *   }
 * }
 */
