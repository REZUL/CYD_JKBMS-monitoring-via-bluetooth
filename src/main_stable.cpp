#include <Arduino.h>
#include <NimBLEDevice.h>
#include <TFT_eSPI.h>
#include <XPT2046_Bitbang.h>

//********************************************
// Display Setup
//********************************************
TFT_eSPI tft = TFT_eSPI();
// ----------------------------
// Touch Screen pins
// ----------------------------
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);
// Colors
#define COLOR_BG        0x0000  // Black
#define COLOR_HEADER    0x001F  // Blue
#define COLOR_TEXT      0xFFFF  // White
#define COLOR_VALUE     0x07E0  // Green
#define COLOR_WARN      0xFD20  // Orange
#define COLOR_ERROR     0xF800  // Red
#define COLOR_LABEL     0x7BEF  // Gray

int currentPage = 0;
unsigned long lastDisplayUpdate = 0;
const int DISPLAY_UPDATE_INTERVAL = 1000; // Update display every 1 second
bool dataReceived = false; // Whether the first data from BMS has been received

//********************************************
// JKBMS Class Definition
//********************************************
class JKBMS {
public:
  JKBMS(const std::string& mac) : targetMAC(mac) {}

  // BLE Components
  NimBLERemoteCharacteristic* pChr = nullptr;
  NimBLEAdvertisedDevice* advDevice = nullptr;
  bool doConnect = false;
  bool connected = false;
  uint32_t lastNotifyTime = 0;
  std::string targetMAC;

  // Data Processing
  byte receivedBytes[320];
  int frame = 0;
  bool received_start = false;
  bool received_complete = false;
  bool new_data = false;
  int ignoreNotifyCount = 0;

  // BMS Data Fields
  float cellVoltage[16] = { 0 };
  float Average_Cell_Voltage = 0;
  float Delta_Cell_Voltage = 0;
  float Battery_Voltage = 0;
  float Battery_Power = 0;
  float Charge_Current = 0;
  float Battery_T1 = 0;
  float Battery_T2 = 0;
  float MOS_Temp = 0;
  int Percent_Remain = 0;
  float Capacity_Remain = 0;
  float Nominal_Capacity = 0;
  bool Charge = false;
  bool Discharge = false;
  bool Balance = false;

  // Methods
  bool connectToServer();
  void parseData();
  void writeRegister(uint8_t address, uint32_t value, uint8_t length);
  void handleNotification(uint8_t* pData, size_t length);

private:
  uint8_t crc(const uint8_t data[], uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) crc += data[i];
    return crc;
  }
};

//********************************************
// Global Variables and Callbacks
//********************************************
JKBMS jkBmsDevices[] = {
  JKBMS("c8:47:80:21:20:b7"),  // Insert your BMS MAC address
};

const int bmsDeviceCount = sizeof(jkBmsDevices) / sizeof(jkBmsDevices[0]);
NimBLEScan* pScan;
unsigned long lastScanTime = 0;

class ClientCallbacks : public NimBLEClientCallbacks {
  JKBMS* bms;
public:
  ClientCallbacks(JKBMS* bmsInstance) : bms(bmsInstance) {}

  void onConnect(NimBLEClient* pClient) {
    Serial.printf("Підключено до %s\n", bms->targetMAC.c_str());
    bms->connected = true;
  }

  void onDisconnect(NimBLEClient* pClient, int reason) {
    Serial.printf("%s відключено, причина: %d\n", bms->targetMAC.c_str(), reason);
    bms->connected = false;
    bms->doConnect = false;
  }
};

class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.printf("Знайдено BLE пристрій: %s\n", advertisedDevice->toString().c_str());
    for (int i = 0; i < bmsDeviceCount; i++) {
      if (advertisedDevice->getAddress().toString() == jkBmsDevices[i].targetMAC && 
          !jkBmsDevices[i].connected && !jkBmsDevices[i].doConnect) {
        Serial.printf("Знайдено цільовий пристрій: %s\n", jkBmsDevices[i].targetMAC.c_str());
        jkBmsDevices[i].advDevice = advertisedDevice;
        jkBmsDevices[i].doConnect = true;
        NimBLEDevice::getScan()->stop();
      }
    }
  }
} scanCallbacks;

void notifyCB(NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t length, bool isNotify) {
  for (int i = 0; i < bmsDeviceCount; i++) {
    if (jkBmsDevices[i].pChr == pChr) {
      jkBmsDevices[i].handleNotification(pData, length);
      break;
    }
  }
}

//********************************************
// Display Functions
//********************************************
void drawHeader() {
  tft.fillRect(0, 0, 240, 30, COLOR_HEADER);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.print("JKBMS Monitor");
  
  // Connection status
  if (jkBmsDevices[0].connected) {
    tft.fillCircle(220, 15, 6, COLOR_VALUE);
  } else {
    tft.fillCircle(220, 15, 6, COLOR_ERROR);
  }
}

void displayMainPage(JKBMS& bms) {
  tft.fillRect(0, 30, 240, 290, COLOR_BG);
  
  int y = 40;
  tft.setTextSize(1);
  
  // Battery Voltage
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Voltage:");
  tft.setTextColor(COLOR_VALUE);
  tft.setCursor(120, y);
  tft.printf("%.2f V", bms.Battery_Voltage);
  y += 20;
  
  // Current
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Current:");
  tft.setTextColor(bms.Charge_Current >= 0 ? COLOR_VALUE : COLOR_WARN);
  tft.setCursor(120, y);
  tft.printf("%.2f A", bms.Charge_Current);
  y += 20;
  
  // Power
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Power:");
  tft.setTextColor(COLOR_VALUE);
  tft.setCursor(120, y);
  tft.printf("%.2f W", bms.Battery_Power);
  y += 25;
  
  // Charge
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Charge:");
  tft.setTextColor(COLOR_VALUE);
  tft.setTextSize(2);
  tft.setCursor(120, y - 5);
  tft.printf("%d%%", bms.Percent_Remain);
  tft.setTextSize(1);
  y += 25;
  
  // Charge progress bar
  int barWidth = 200;
  int barHeight = 15;
  int barX = 20;
  tft.drawRect(barX, y, barWidth, barHeight, COLOR_TEXT);
  int fillWidth = (barWidth - 4) * bms.Percent_Remain / 100;
  uint16_t barColor = bms.Percent_Remain > 20 ? COLOR_VALUE : COLOR_ERROR;
  tft.fillRect(barX + 2, y + 2, fillWidth, barHeight - 4, barColor);
  y += 25;
  
  // Capacity
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Capacity:");
  tft.setTextColor(COLOR_VALUE);
  tft.setCursor(120, y);
  tft.printf("%.2f Ah", bms.Capacity_Remain);
  y += 20;
  
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Nominal:");
  tft.setTextColor(COLOR_VALUE);
  tft.setCursor(120, y);
  tft.printf("%.2f Ah", bms.Nominal_Capacity);
  y += 25;
  
  // Temperatures
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Temperature T1:");
  tft.setTextColor(bms.Battery_T1 < 45 ? COLOR_VALUE : COLOR_WARN);
  tft.setCursor(150, y);
  tft.printf("%.1fC", bms.Battery_T1);
  y += 20;
  
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Temperature T2:");
  tft.setTextColor(bms.Battery_T2 < 45 ? COLOR_VALUE : COLOR_WARN);
  tft.setCursor(150, y);
  tft.printf("%.1fC", bms.Battery_T2);
  y += 20;
  
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("MOS Temp:");
  tft.setTextColor(bms.MOS_Temp < 60 ? COLOR_VALUE : COLOR_WARN);
  tft.setCursor(150, y);
  tft.printf("%.1fC", bms.MOS_Temp);
  y += 25;
  
  // Statuses
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Status:");
  y += 18;
  
  tft.setCursor(20, y);
  tft.setTextColor(bms.Charge ? COLOR_VALUE : COLOR_LABEL);
  tft.print("Charge");
  
  tft.setCursor(90, y);
  tft.setTextColor(bms.Discharge ? COLOR_VALUE : COLOR_LABEL);
  tft.print("Discharge");
  
  tft.setCursor(170, y);
  tft.setTextColor(bms.Balance ? COLOR_VALUE : COLOR_LABEL);
  tft.print("Balance");
}

void displayCellsPage(JKBMS& bms) {
  tft.fillRect(0, 30, 240, 290, COLOR_BG);
  
  int y = 40;
  tft.setTextSize(1);
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(60, y);
  tft.print("Cells voltage:");
  y += 20;
  
  int cellCount = 0;
  for (int i = 0; i < 16; i++) {
    if (bms.cellVoltage[i] > 0) cellCount++;
  }
  
  // Two columns: left (1-8) and right (9-16)
  int leftCol = 5;   // X position of the left column
  int rightCol = 125; // X position of the right column
  int yLeft = y;
  int yRight = y;
  
  for (int i = 0; i < cellCount && i < 16; i++) {
    int col = (i < 8) ? leftCol : rightCol;
    int& currentY = (i < 8) ? yLeft : yRight;
    
    // Determining color based on voltage
    float voltage = bms.cellVoltage[i];
    uint16_t color = COLOR_VALUE;
    if (voltage < 3.0 || voltage > 3.6) {
      color = COLOR_ERROR;
    } else if (voltage < 3.2 || voltage > 3.5) {
      color = COLOR_WARN;
    }
    
    tft.setTextColor(COLOR_LABEL);
    tft.setCursor(col, currentY);
    tft.printf("Cell %02d:", i + 1);
    
    tft.setTextColor(color);
    tft.setCursor(col + 55, currentY);
    tft.printf("%.3fV", voltage);
    
    currentY += 16;
  }
  
  // Average and delta at the bottom
  y = 280;
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Average:");
  tft.setTextColor(COLOR_VALUE);
  tft.setCursor(70, y);
  tft.printf("%.3fV", bms.Average_Cell_Voltage);
  
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(130, y);
  tft.print("Delta:");
  tft.setTextColor(bms.Delta_Cell_Voltage < 0.05 ? COLOR_VALUE : COLOR_WARN);
  tft.setCursor(180, y);
  tft.printf("%.3fV", bms.Delta_Cell_Voltage);
}

void displayConnectionStatus() {
  tft.fillRect(0, 30, 240, 290, COLOR_BG);
  
  int y = 80;
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(30, y);
  tft.print("JKBMS Monitor");
  
  y += 40;
  tft.setTextSize(1);
  
  // Scanning status
  if (!jkBmsDevices[0].connected && !jkBmsDevices[0].doConnect) {
    tft.setTextColor(COLOR_WARN);
    tft.setCursor(20, y);
    tft.print("Searching device...");
    
    // Animated indicator
    int dotCount = (millis() / 500) % 4;
    tft.setCursor(150, y);
    for (int i = 0; i < dotCount; i++) {
      tft.print(".");
    }
    
  } else if (jkBmsDevices[0].doConnect && !jkBmsDevices[0].connected) {
    tft.setTextColor(COLOR_VALUE);
    tft.setCursor(20, y);
    tft.print("Device found!");
    y += 20;
    tft.setTextColor(COLOR_WARN);
    tft.setCursor(20, y);
    tft.print("Connecting...");
    
  } else if (jkBmsDevices[0].connected && !dataReceived) {
    tft.setTextColor(COLOR_VALUE);
    tft.setCursor(20, y);
    tft.print("Connected!");
    y += 20;
    tft.setTextColor(COLOR_WARN);
    tft.setCursor(20, y);
    tft.print("Waiting for data...");
    
    // Animated indicator
    int dotCount = (millis() / 500) % 4;
    tft.setCursor(160, y);
    for (int i = 0; i < dotCount; i++) {
      tft.print(".");
    }
  }
  
  // MAC address
  y += 40;
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(20, y);
  tft.print("MAC:");
  y += 15;
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(20, y);
  tft.print(jkBmsDevices[0].targetMAC.c_str());
  
  // Connection indicator
  y += 40;
  int indicatorX = 110;
  int indicatorY = y;
  int radius = 20;
  
  if (jkBmsDevices[0].connected) {
    tft.fillCircle(indicatorX, indicatorY, radius, COLOR_VALUE);
    tft.setTextColor(COLOR_BG);
    tft.setCursor(indicatorX - 5, indicatorY - 5);
    tft.print("OK");
  } else {
    tft.drawCircle(indicatorX, indicatorY, radius, COLOR_WARN);
    tft.setTextColor(COLOR_WARN);
    tft.setCursor(indicatorX - 6, indicatorY - 5);
    tft.print("...");
  }
  
  // Hint at the bottom
  y = 280;
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(20, y);
  tft.print("Waiting...");
}

void displayStatusPage() {
  tft.fillRect(0, 30, 240, 290, COLOR_BG);
  
  int y = 50;
  tft.setTextSize(1);
  
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(60, y);
  tft.print("Status BMS");
  y += 30;
  
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("MAC Address:");
  tft.setTextColor(COLOR_VALUE);
  tft.setCursor(10, y + 15);
  tft.print(jkBmsDevices[0].targetMAC.c_str());
  y += 40;
  
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Connection status:");
  tft.setTextColor(jkBmsDevices[0].connected ? COLOR_VALUE : COLOR_ERROR);
  tft.setCursor(10, y + 15);
  tft.print(jkBmsDevices[0].connected ? "Connected" : "Disconnected");
  y += 50;
  
  if (jkBmsDevices[0].connected) {
    unsigned long timeSinceNotify = (millis() - jkBmsDevices[0].lastNotifyTime) / 1000;
    tft.setTextColor(COLOR_LABEL);
    tft.setCursor(10, y);
    tft.print("Last data:");
    tft.setTextColor(COLOR_VALUE);
    tft.setCursor(10, y + 15);
    tft.printf("%lu sec ago", timeSinceNotify);
  }
  
  y += 60;
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y);
  tft.print("Touch the screen");
  tft.setCursor(10, y + 15);
  tft.print("to switch pages");
}

void updateDisplay() {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  lastDisplayUpdate = millis();
  
  drawHeader();
  
  // If data has not yet been received - show the connection screen
  if (!dataReceived) {
    displayConnectionStatus();
    return;
  }
  
  // Otherwise show regular pages
  switch (currentPage) {
    case 0:
      displayMainPage(jkBmsDevices[0]);
      break;
    case 1:
      displayCellsPage(jkBmsDevices[0]);
      break;
    case 2:
      displayStatusPage();
      break;
  }
}

void checkTouch() {
    if (!dataReceived) return;// Prohibit page switching until data is received
    currentPage = (currentPage + 1) % 3;
    delay(200); // Debounce
    updateDisplay();
}

//********************************************
// JKBMS Method Implementation
//********************************************
bool JKBMS::connectToServer() {
  Serial.printf("Attempting to connect to %s...\n", targetMAC.c_str());
  NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());

  if (!pClient) {
    pClient = NimBLEDevice::createClient();
    Serial.println("Created new client.");
    pClient->setClientCallbacks(new ClientCallbacks(this), true);
    pClient->setConnectionParams(12, 12, 0, 150);
    pClient->setConnectTimeout(5);
  }

  if (!pClient->connect(advDevice)) {
    Serial.printf("Failed to connect to %s\n", targetMAC.c_str());
    return false;
  }

  Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

  NimBLERemoteService* pSvc = pClient->getService("ffe0");
  if (pSvc) {
    pChr = pSvc->getCharacteristic("ffe1");
    if (pChr && pChr->canNotify()) {
      if (pChr->subscribe(true, notifyCB)) {
        Serial.println("Subscribed to notifications");
        delay(500);
        writeRegister(0x97, 0x00000000, 0x00);
        delay(500);
        writeRegister(0x96, 0x00000000, 0x00);
        return true;
      }
    }
  }
  Serial.println("Service or characteristic not found.");
  return false;
}

void JKBMS::handleNotification(uint8_t* pData, size_t length) {
  lastNotifyTime = millis();

  if (ignoreNotifyCount > 0) {
    ignoreNotifyCount--;
    return;
  }

  // Check the start of the data frame
  if (pData[0] == 0x55 && pData[1] == 0xAA && pData[2] == 0xEB && pData[3] == 0x90) {
    frame = 0;
    received_start = true;
    received_complete = false;

    for (int i = 0; i < length; i++) {
      receivedBytes[frame++] = pData[i];
    }
  } else if (received_start && !received_complete) {
    for (int i = 0; i < length; i++) {
      receivedBytes[frame++] = pData[i];
      if (frame >= 300) {
        received_complete = true;
        received_start = false;
        new_data = true;
        switch (receivedBytes[4]) {  // Use receivedBytes[4] instead of pData[4]
          case 0x01:
            Serial.printf("Data in the fourth byte with settings: 0x%02X\n", receivedBytes[4]);
            break;
          case 0x02:
            Serial.printf("Data in the fourth byte with cell status: 0x%02X\n", receivedBytes[4]);
            parseData();
            break;
          case 0x03:
            Serial.printf("Data in the fourth byte with BMS information: 0x%02X\n", receivedBytes[4]);
            break;
          default:
            Serial.printf("Data in the fourth byte with unknown type: 0x%02X\n", receivedBytes[4]);
            break;
        }
        break;
      }
    }
  }
}

void JKBMS::writeRegister(uint8_t address, uint32_t value, uint8_t length) {
  uint8_t frame[20] = { 0xAA, 0x55, 0x90, 0xEB, address, length };

  frame[6] = value >> 0;
  frame[7] = value >> 8;
  frame[8] = value >> 16;
  frame[9] = value >> 24;

  frame[19] = crc(frame, 19);

  if (pChr) {
    pChr->writeValue(frame, sizeof(frame));
  }
}

void JKBMS::parseData() {
  Serial.println("Parsing data...");
  new_data = false;
  ignoreNotifyCount = 10;

  dataReceived = true;  // Set the flag that data has been received

  for (int j = 0, i = 7; i < 38; j++, i += 2) {
    cellVoltage[j] = ((receivedBytes[i] << 8 | receivedBytes[i - 1]) * 0.001);
  }

  Average_Cell_Voltage = (((int)receivedBytes[75] << 8 | receivedBytes[74]) * 0.001);
  Delta_Cell_Voltage = (((int)receivedBytes[77] << 8 | receivedBytes[76]) * 0.001);

  if (receivedBytes[145] == 0xFF) {
    MOS_Temp = ((0xFF << 24 | 0xFF << 16 | receivedBytes[145] << 8 | receivedBytes[144]) * 0.1);
  } else {
    MOS_Temp = ((receivedBytes[145] << 8 | receivedBytes[144]) * 0.1);
  }

  Battery_Voltage = ((receivedBytes[153] << 24 | receivedBytes[152] << 16 | receivedBytes[151] << 8 | receivedBytes[150]) * 0.001);
  Charge_Current = ((receivedBytes[161] << 24 | receivedBytes[160] << 16 | receivedBytes[159] << 8 | receivedBytes[158]) * 0.001);
  Battery_Power = Battery_Voltage * Charge_Current;

  if (receivedBytes[163] == 0xFF) {
    Battery_T1 = ((0xFF << 24 | 0xFF << 16 | receivedBytes[163] << 8 | receivedBytes[162]) * 0.1);
  } else {
    Battery_T1 = ((receivedBytes[163] << 8 | receivedBytes[162]) * 0.1);
  }

  if (receivedBytes[165] == 0xFF) {
    Battery_T2 = ((0xFF << 24 | 0xFF << 16 | receivedBytes[165] << 8 | receivedBytes[164]) * 0.1);
  } else {
    Battery_T2 = ((receivedBytes[165] << 8 | receivedBytes[164]) * 0.1);
  }

  Percent_Remain = (receivedBytes[173]);
  Capacity_Remain = ((receivedBytes[177] << 24 | receivedBytes[176] << 16 | receivedBytes[175] << 8 | receivedBytes[174]) * 0.001);
  Nominal_Capacity = ((receivedBytes[181] << 24 | receivedBytes[180] << 16 | receivedBytes[179] << 8 | receivedBytes[178]) * 0.001);

  Charge = (receivedBytes[198] > 0);
  Discharge = (receivedBytes[199] > 0);
  Balance = (receivedBytes[201] > 0);

  Serial.println("Data parsed successfully");
}

//********************************************
// Main Program
//********************************************
void setup() {
  Serial.begin(115200);
  delay(1000);

  ts.begin();
  
  // Display initialization
  tft.init();
  tft.setRotation(0); // Portrait orientation
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.print("JKBMS");
  tft.setCursor(20, 130);
  tft.print("Monitor");
  tft.setTextSize(1);
  tft.setCursor(20, 160);
  tft.print("Initializing...");
  
  uint16_t calData[5] = { 275, 3620, 264, 3532, 2 };
  tft.setTouch(calData);
  
  Serial.println("\n\nJKBMS Monitor");
  Serial.println("Initializing...");

  NimBLEDevice::init("JKBMS-Client");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->setActiveScan(true);
  
  Serial.println("Dun! Starting BMS scan...");
  
  delay(2000);
  tft.fillScreen(COLOR_BG);
  drawHeader();
  displayStatusPage();
}

void loop() {
  int connectedCount = 0;
  TouchPoint p = ts.getTouch();
  for (int i = 0; i < bmsDeviceCount; i++) {
    if (jkBmsDevices[i].doConnect && !jkBmsDevices[i].connected) {
      if (jkBmsDevices[i].connectToServer()) {
        Serial.printf("✓ %s connected successfully\n", jkBmsDevices[i].targetMAC.c_str());
      }
      jkBmsDevices[i].doConnect = false;
    }

    if (jkBmsDevices[i].connected) {
      connectedCount++;
      if (millis() - jkBmsDevices[i].lastNotifyTime > 20000) {
        Serial.printf("✗ %s connection timeout\n", jkBmsDevices[i].targetMAC.c_str());
        NimBLEClient* pClient = NimBLEDevice::getClientByPeerAddress(jkBmsDevices[i].advDevice->getAddress());
        if (pClient) pClient->disconnect();
      }
    }
  }

  if (connectedCount < bmsDeviceCount && (millis() - lastScanTime >= 10000)) {
    Serial.println("🔍 Starting scan...");
    pScan->start(5000, false);
    lastScanTime = millis();
  }

  updateDisplay();
  if (p.zRaw > 0) checkTouch();
  delay(100);
}