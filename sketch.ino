#define BLYNK_TEMPLATE_ID "TMPL34GyZz_Hq"
#define BLYNK_TEMPLATE_NAME "BMS Project"
#define BLYNK_AUTH_TOKEN "hdVtCXC80R5P0RtBUipQ0ool_t-PUnkk"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

char wifiSsid[] = "Wokwi-GUEST";
char wifiPass[] = "";

#define NUM_CELLS 4
#define WINDOW_SIZE 5

const int cellPins[NUM_CELLS] = {34, 35, 32, 33};
const int RELAY_LED_PIN = 25;
const int BUTTON_PIN = 26;

const float ADC_MAX = 4095.0;
const float V_MIN = 3.0;
const float V_MAX = 4.2;

float cellVoltages[NUM_CELLS];
float previousImbalance = 0.0;

const float TRIP_THRESHOLD = 0.15;
const float RESET_THRESHOLD = 0.10;
const unsigned long DEBOUNCE_MS = 50;
const unsigned long RECOVERY_MS = 5000;
const int FROZEN_LIMIT = 5;
const float MAX_JUMP = 0.5;

enum RelayState { NORMAL, FAULT_PENDING, TRIPPED, RECOVERING };
RelayState relayState = NORMAL;
unsigned long stateChangeTimer = 0;

float lastCellVoltage[NUM_CELLS];
int frozenCount[NUM_CELLS];
float voltageWindow[NUM_CELLS][WINDOW_SIZE];
int windowIndex[NUM_CELLS];
bool windowFilled[NUM_CELLS];

const unsigned long PAGE_ROTATE_MS = 3000;
const unsigned long LCD_REFRESH_MS = 300;
unsigned long lastPageSwitch = 0;
unsigned long lastLcdRefresh = 0;
int currentPage = 0;
bool wasFaultScreen = false;

float lastShownWeakVoltage = -1;
float lastShownStrongVoltage = -1;
float lastShownImbalance = -1;
String lastShownRelayState = "";
float lastShownSoC = -1;
String lastShownTrend = "";

enum FaultState { FS_NORMAL, FS_DEGRADED, FS_FAILSAFE, FS_SHUTDOWN };
FaultState faultState = FS_NORMAL;

enum FaultSource { FAULT_NONE, FAULT_BATTERY, FAULT_ADC, FAULT_RELAY, FAULT_COMM };
FaultSource currentFaultSource = FAULT_NONE;

unsigned long failsafeTimer = 0;
const unsigned long VERIFY_MS = 2000;

unsigned long failsafeEntryTimestamps[3] = {0, 0, 0};
int failsafeEntryIndex = 0;
const unsigned long FLAP_WINDOW_MS = 30000;

bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
bool simulatedFaultActive = false;
FaultSource simulatedFaultType = FAULT_COMM;
unsigned long simulatedFaultStart = 0;
const unsigned long SIM_FAULT_DURATION = 3000;

unsigned long buttonHoldStart = 0;
bool buttonHeld = false;

enum WifiState { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED_STATE };
WifiState wifiState = WIFI_DISCONNECTED;
unsigned long wifiStateTimer = 0;
const unsigned long WIFI_CONNECT_TIMEOUT = 8000;
const unsigned long WIFI_RETRY_INTERVAL = 5000;
bool blynkReady = false;

struct QueuedEvent {
  int pin;
  String value;
};
#define QUEUE_SIZE 20
QueuedEvent eventQueue[QUEUE_SIZE];
int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

String lastSentCellString = "";
int lastSentWeak = -1;
int lastSentStrong = -1;
float lastSentImbalance = -999;
String lastSentRelayStr = "";
String lastSentFaultStr = "";
int lastSentRSSI = -999;
int lastSentQueueDepth = -1;
unsigned long lastRSSICheck = 0;
const unsigned long RSSI_CHECK_INTERVAL = 2000;

int faultCount = 0;
String faultHistory[3] = {"none", "none", "none"};
int faultHistoryIndex = 0;
unsigned long lastAnalyticsSend = 0;
const unsigned long ANALYTICS_INTERVAL = 5000;

String faultStateToString(FaultState s) {
  if (s == FS_NORMAL) return "NORMAL";
  if (s == FS_DEGRADED) return "DEGRADED";
  if (s == FS_FAILSAFE) return "FAILSAFE";
  return "SHUTDOWN";
}

String faultSourceToString(FaultSource f) {
  if (f == FAULT_NONE) return "NONE";
  if (f == FAULT_BATTERY) return "BATTERY";
  if (f == FAULT_ADC) return "ADC";
  if (f == FAULT_RELAY) return "RELAY";
  return "COMM";
}

String relayStateToString() {
  if (relayState == NORMAL) return "NORMAL";
  if (relayState == FAULT_PENDING) return "PENDING";
  if (relayState == TRIPPED) return "TRIPPED";
  if (relayState == RECOVERING) return "RECOVER";
  return "?";
}

int calculateRiskScore(String trend, float imbalance, float threshold, float avgSoC) {
  int score = 0;
  if (trend == "INCREASING") score += 20;
  if (imbalance > threshold) score += 20;
  if (avgSoC < 20.0) score += 20;
  if (faultState == FS_FAILSAFE) score += 20;
  if (faultState == FS_SHUTDOWN) score += 40;
  int faultContribution = faultCount * 5;
  if (faultContribution > 20) faultContribution = 20;
  score += faultContribution;
  if (score > 100) score = 100;
  return score;
}

String generateRecommendation(int riskScore) {
  if (faultState == FS_SHUTDOWN) {
    return "URGENT: SHUTDOWN active - manual inspection required.";
  }
  if (riskScore >= 70) {
    return "High risk - inspect " + faultSourceToString(currentFaultSource) + " subsystem soon.";
  }
  if (riskScore >= 40) {
    return "Moderate risk - monitor imbalance trend closely.";
  }
  return "System healthy - no action needed.";
}

String getUptimeString() {
  unsigned long secs = millis() / 1000;
  unsigned long h = secs / 3600;
  unsigned long m = (secs % 3600) / 60;
  unsigned long s = secs % 60;
  return String(h) + "h " + String(m) + "m " + String(s) + "s";
}

void recordFaultHistoryEntry() {
  faultCount++;
  String entry = faultSourceToString(currentFaultSource) + "@" + String(millis() / 1000) + "s";
  faultHistory[faultHistoryIndex] = entry;
  faultHistoryIndex = (faultHistoryIndex + 1) % 3;
}

void logTransition(FaultState prev, FaultState next, FaultSource source) {
  Serial.println("[FAULT-LOG] time=" + String(millis()) +
                  "ms prevState=" + faultStateToString(prev) +
                  " newState=" + faultStateToString(next) +
                  " faultSource=" + faultSourceToString(source));
}

void enqueueEvent(int pin, String value) {
  if (queueCount >= QUEUE_SIZE) {
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount--;
  }
  eventQueue[queueTail].pin = pin;
  eventQueue[queueTail].value = value;
  queueTail = (queueTail + 1) % QUEUE_SIZE;
  queueCount++;
}

void sendOrQueue(int pin, String value) {
  if (wifiState == WIFI_CONNECTED_STATE && blynkReady && Blynk.connected()) {
    Blynk.virtualWrite(pin, value);
  } else {
    enqueueEvent(pin, value);
  }
}

void flushOneQueuedEvent() {
  if (queueCount > 0 && wifiState == WIFI_CONNECTED_STATE && blynkReady && Blynk.connected()) {
    QueuedEvent e = eventQueue[queueHead];
    Blynk.virtualWrite(e.pin, e.value);
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount--;
  }
}

void updateWifiStateMachine() {
  unsigned long now = millis();
  switch (wifiState) {
    case WIFI_DISCONNECTED:
      if (now - wifiStateTimer >= WIFI_RETRY_INTERVAL || wifiStateTimer == 0) {
        Serial.println("[WIFI] Attempting connection...");
        WiFi.begin(wifiSsid, wifiPass);
        wifiStateTimer = now;
        wifiState = WIFI_CONNECTING;
      }
      break;
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WIFI] Connected! IP: " + WiFi.localIP().toString());
        wifiState = WIFI_CONNECTED_STATE;
        Blynk.config(BLYNK_AUTH_TOKEN);
        Blynk.connect(1000);
        blynkReady = true;
      } else if (now - wifiStateTimer >= WIFI_CONNECT_TIMEOUT) {
        Serial.println("[WIFI] Connection attempt timed out, will retry.");
        wifiState = WIFI_DISCONNECTED;
        wifiStateTimer = now;
      }
      break;
    case WIFI_CONNECTED_STATE:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Connection lost!");
        wifiState = WIFI_DISCONNECTED;
        wifiStateTimer = now;
        blynkReady = false;
      }
      break;
  }
}

void updateTelemetry(int weak, int strong, float imbalance, float avgSoC, String trend, float threshold) {
  String cellStr = String(cellVoltages[0], 2) + "," + String(cellVoltages[1], 2) + "," +
                    String(cellVoltages[2], 2) + "," + String(cellVoltages[3], 2);
  if (cellStr != lastSentCellString) {
    sendOrQueue(0, cellStr);
    lastSentCellString = cellStr;
  }
  if (weak != lastSentWeak) {
    sendOrQueue(1, String(weak + 1));
    lastSentWeak = weak;
  }
  if (strong != lastSentStrong) {
    sendOrQueue(2, String(strong + 1));
    lastSentStrong = strong;
  }
  if (abs(imbalance - lastSentImbalance) > 0.01) {
    sendOrQueue(3, String(imbalance, 3));
    lastSentImbalance = imbalance;
  }
  String relayStr = relayStateToString();
  if (relayStr != lastSentRelayStr) {
    sendOrQueue(4, relayStr);
    lastSentRelayStr = relayStr;
  }
  String faultStr = faultStateToString(faultState) + "-" + faultSourceToString(currentFaultSource);
  if (faultStr != lastSentFaultStr) {
    sendOrQueue(5, faultStr);
    lastSentFaultStr = faultStr;
  }

  unsigned long now = millis();
  if (now - lastRSSICheck >= RSSI_CHECK_INTERVAL) {
    lastRSSICheck = now;
    if (wifiState == WIFI_CONNECTED_STATE) {
      int rssi = WiFi.RSSI();
      if (abs(rssi - lastSentRSSI) > 3) {
        sendOrQueue(6, String(rssi));
        lastSentRSSI = rssi;
      }
    }
  }

  if (queueCount != lastSentQueueDepth) {
    sendOrQueue(7, String(queueCount));
    lastSentQueueDepth = queueCount;
  }

  // Task 6: Analytics - always queued (not sent directly), so flushOneQueuedEvent()
  // paces them out one per loop cycle instead of bursting 6 at once, which Blynk
  // was silently dropping some of when sent simultaneously
  if (now - lastAnalyticsSend >= ANALYTICS_INTERVAL) {
    lastAnalyticsSend = now;
    int risk = calculateRiskScore(trend, imbalance, threshold, avgSoC);
    enqueueEvent(8, String(risk));
    enqueueEvent(9, String(faultCount));
    enqueueEvent(10, getUptimeString());
    enqueueEvent(11, generateRecommendation(risk));
    enqueueEvent(12, String(avgSoC, 1));
    String histStr = faultHistory[0] + ";" + faultHistory[1] + ";" + faultHistory[2];
    enqueueEvent(13, histStr);
  }
}

void handleButton(bool anomaly) {
  bool reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (lastButtonState == HIGH && reading == LOW) {
    buttonHoldStart = now;
    buttonHeld = true;
  }
  if (lastButtonState == LOW && reading == HIGH) {
    if (buttonHeld && (now - buttonHoldStart) < 3000) {
      simulatedFaultActive = true;
      simulatedFaultStart = now;
      simulatedFaultType = (simulatedFaultType == FAULT_COMM) ? FAULT_RELAY : FAULT_COMM;
      Serial.println("[BUTTON] Short press - simulating " + faultSourceToString(simulatedFaultType) + " fault for 3s");
    }
    buttonHeld = false;
  }
  if (buttonHeld && faultState == FS_SHUTDOWN && (now - buttonHoldStart >= 3000)) {
    Serial.println("[BUTTON] Long hold detected - manual reset from SHUTDOWN");
    faultState = FS_NORMAL;
    currentFaultSource = FAULT_NONE;
    failsafeEntryIndex = 0;
    for (int i = 0; i < 3; i++) failsafeEntryTimestamps[i] = 0;
    buttonHeld = false;
  }
  if (simulatedFaultActive && (now - simulatedFaultStart >= SIM_FAULT_DURATION)) {
    simulatedFaultActive = false;
  }
  lastButtonState = reading;
}

bool checkFailsafeFlapping() {
  unsigned long now = millis();
  int recentCount = 0;
  for (int i = 0; i < 3; i++) {
    if (failsafeEntryTimestamps[i] != 0 && (now - failsafeEntryTimestamps[i]) <= FLAP_WINDOW_MS) {
      recentCount++;
    }
  }
  return recentCount >= 3;
}

void recordFailsafeEntry() {
  failsafeEntryTimestamps[failsafeEntryIndex] = millis();
  failsafeEntryIndex = (failsafeEntryIndex + 1) % 3;
}

void updateFaultStateMachine(bool anomaly, float imbalance) {
  FaultState prevState = faultState;
  FaultSource source = FAULT_NONE;

  if (simulatedFaultActive) {
    source = simulatedFaultType;
  } else if (anomaly) {
    source = FAULT_ADC;
  } else if (imbalance > TRIP_THRESHOLD) {
    source = FAULT_BATTERY;
  }

  switch (faultState) {
    case FS_NORMAL:
      if (relayState == FAULT_PENDING || (source != FAULT_NONE && relayState == NORMAL)) {
        faultState = FS_DEGRADED;
        currentFaultSource = source;
      } else if (relayState == TRIPPED) {
        faultState = FS_FAILSAFE;
        currentFaultSource = source;
        recordFailsafeEntry();
        recordFaultHistoryEntry();
      }
      break;
    case FS_DEGRADED:
      if (relayState == TRIPPED) {
        faultState = FS_FAILSAFE;
        currentFaultSource = source;
        recordFailsafeEntry();
        recordFaultHistoryEntry();
      } else if (relayState == NORMAL && source == FAULT_NONE) {
        faultState = FS_NORMAL;
        currentFaultSource = FAULT_NONE;
      } else {
        currentFaultSource = source;
      }
      break;
    case FS_FAILSAFE:
      if (checkFailsafeFlapping()) {
        faultState = FS_SHUTDOWN;
        currentFaultSource = source;
      } else if (relayState == NORMAL && !anomaly && !simulatedFaultActive) {
        if (failsafeTimer == 0) {
          failsafeTimer = millis();
        } else if (millis() - failsafeTimer >= VERIFY_MS) {
          faultState = FS_NORMAL;
          currentFaultSource = FAULT_NONE;
          failsafeTimer = 0;
        }
      } else {
        failsafeTimer = 0;
      }
      break;
    case FS_SHUTDOWN:
      break;
  }

  if (faultState != prevState) {
    logTransition(prevState, faultState, currentFaultSource);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(RELAY_LED_PIN, OUTPUT);
  digitalWrite(RELAY_LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  for (int i = 0; i < NUM_CELLS; i++) {
    lastCellVoltage[i] = 0;
    frozenCount[i] = 0;
    windowIndex[i] = 0;
    windowFilled[i] = false;
    for (int j = 0; j < WINDOW_SIZE; j++) {
      voltageWindow[i][j] = 0;
    }
  }
}

float readCellVoltage(int pin) {
  int raw = analogRead(pin);
  return V_MIN + (raw / ADC_MAX) * (V_MAX - V_MIN);
}

void updateAllCells() {
  for (int i = 0; i < NUM_CELLS; i++) {
    cellVoltages[i] = readCellVoltage(cellPins[i]);
  }
}

int findWeakestCell() {
  int idx = 0;
  for (int i = 1; i < NUM_CELLS; i++) {
    if (cellVoltages[i] < cellVoltages[idx]) idx = i;
  }
  return idx;
}

int findStrongestCell() {
  int idx = 0;
  for (int i = 1; i < NUM_CELLS; i++) {
    if (cellVoltages[i] > cellVoltages[idx]) idx = i;
  }
  return idx;
}

float calculateImbalance() {
  return cellVoltages[findStrongestCell()] - cellVoltages[findWeakestCell()];
}

float estimateSoC(float voltage) {
  float soc = (voltage - V_MIN) / (V_MAX - V_MIN) * 100.0;
  if (soc < 0) soc = 0;
  if (soc > 100) soc = 100;
  return soc;
}

float getAdaptiveThreshold(float avgSoC) {
  if (avgSoC < 20.0) return 0.03;
  else if (avgSoC < 50.0) return 0.05;
  else return 0.08;
}

float getWindowAverage(int i) {
  int count = windowFilled[i] ? WINDOW_SIZE : windowIndex[i];
  if (count == 0) return cellVoltages[i];
  float sum = 0;
  for (int j = 0; j < count; j++) {
    sum += voltageWindow[i][j];
  }
  return sum / count;
}

void updateWindow(int i, float value) {
  voltageWindow[i][windowIndex[i]] = value;
  windowIndex[i] = (windowIndex[i] + 1) % WINDOW_SIZE;
  if (windowIndex[i] == 0) windowFilled[i] = true;
}

bool detectAnomalies() {
  bool anomaly = false;
  for (int i = 0; i < NUM_CELLS; i++) {
    float baseline = getWindowAverage(i);
    if (abs(cellVoltages[i] - lastCellVoltage[i]) < 0.001) {
      frozenCount[i]++;
    } else {
      frozenCount[i] = 0;
    }
    if (frozenCount[i] > FROZEN_LIMIT) {
      anomaly = true;
    }
    float deviation = abs(cellVoltages[i] - baseline);
    if (deviation > MAX_JUMP) {
      anomaly = true;
    }
    if (cellVoltages[i] < 2.5 || cellVoltages[i] > 4.3) {
      anomaly = true;
    }
    lastCellVoltage[i] = cellVoltages[i];
    updateWindow(i, cellVoltages[i]);
  }
  return anomaly;
}

void updateRelay(float imbalance, bool anomaly) {
  unsigned long now = millis();
  bool faultCondition = (imbalance > TRIP_THRESHOLD) || anomaly;

  switch (relayState) {
    case NORMAL:
      if (faultCondition) {
        relayState = FAULT_PENDING;
        stateChangeTimer = now;
      }
      break;
    case FAULT_PENDING:
      if (!faultCondition) {
        relayState = NORMAL;
      } else if (now - stateChangeTimer >= DEBOUNCE_MS) {
        relayState = TRIPPED;
        digitalWrite(RELAY_LED_PIN, HIGH);
      }
      break;
    case TRIPPED:
      if (imbalance < RESET_THRESHOLD && !anomaly) {
        relayState = RECOVERING;
        stateChangeTimer = now;
      }
      break;
    case RECOVERING:
      if (imbalance > RESET_THRESHOLD || anomaly) {
        relayState = TRIPPED;
      } else if (now - stateChangeTimer >= RECOVERY_MS) {
        relayState = NORMAL;
        digitalWrite(RELAY_LED_PIN, LOW);
      }
      break;
  }
}

void printPadded(int col, int row, String text, int width) {
  lcd.setCursor(col, row);
  while (text.length() < width) {
    text += " ";
  }
  lcd.print(text);
}

void showFaultScreen() {
  if (!wasFaultScreen) {
    lcd.clear();
    wasFaultScreen = true;
  }
  printPadded(0, 0, faultStateToString(faultState) + " " + faultSourceToString(currentFaultSource), 16);
  printPadded(0, 1, "Relay: " + relayStateToString(), 16);
}

void showBatteryStatusPage(int weak, int strong, float imbalance) {
  if (cellVoltages[weak] != lastShownWeakVoltage) {
    printPadded(0, 0, "Weak:" + String(cellVoltages[weak], 2) + "V", 8);
    lastShownWeakVoltage = cellVoltages[weak];
  }
  if (cellVoltages[strong] != lastShownStrongVoltage) {
    printPadded(8, 0, "Str:" + String(cellVoltages[strong], 2) + "V", 8);
    lastShownStrongVoltage = cellVoltages[strong];
  }
  if (imbalance != lastShownImbalance) {
    printPadded(0, 1, "Imbal:" + String(imbalance, 3) + "V", 16);
    lastShownImbalance = imbalance;
  }
}

void showSystemStatePage(float avgSoC) {
  String relayStr = relayStateToString();
  if (relayStr != lastShownRelayState) {
    printPadded(0, 0, "Relay: " + relayStr, 16);
    lastShownRelayState = relayStr;
  }
  if (avgSoC != lastShownSoC) {
    printPadded(0, 1, "SoC: " + String(avgSoC, 1) + "%", 16);
    lastShownSoC = avgSoC;
  }
}

void showTelemetryPage(String trend, float threshold) {
  if (trend != lastShownTrend) {
    printPadded(0, 0, "Trend: " + trend, 16);
    lastShownTrend = trend;
  }
  printPadded(0, 1, "Thresh:" + String(threshold, 3) + "V", 16);
}

void updateLCD(int weak, int strong, float imbalance, float avgSoC, String trend, float threshold) {
  unsigned long now = millis();
  if (faultState == FS_FAILSAFE || faultState == FS_SHUTDOWN) {
    showFaultScreen();
    return;
  }
  wasFaultScreen = false;

  if (now - lastPageSwitch >= PAGE_ROTATE_MS) {
    currentPage = (currentPage + 1) % 3;
    lastPageSwitch = now;
    lcd.clear();
    lastShownWeakVoltage = -1;
    lastShownStrongVoltage = -1;
    lastShownImbalance = -1;
    lastShownRelayState = "";
    lastShownSoC = -1;
    lastShownTrend = "";
  }

  if (now - lastLcdRefresh >= LCD_REFRESH_MS) {
    lastLcdRefresh = now;
    if (currentPage == 0) {
      showBatteryStatusPage(weak, strong, imbalance);
    } else if (currentPage == 1) {
      showSystemStatePage(avgSoC);
    } else {
      showTelemetryPage(trend, threshold);
    }
  }
}

void loop() {
  updateWifiStateMachine();
  if (wifiState == WIFI_CONNECTED_STATE && blynkReady) {
    Blynk.run();
  }

  updateAllCells();

  float imbalance = calculateImbalance();
  int weak = findWeakestCell();
  int strong = findStrongestCell();

  float avgVoltage = 0;
  for (int i = 0; i < NUM_CELLS; i++) {
    avgVoltage += cellVoltages[i];
  }
  avgVoltage /= NUM_CELLS;
  float avgSoC = estimateSoC(avgVoltage);
  float threshold = getAdaptiveThreshold(avgSoC);

  String trend;
  if (imbalance > previousImbalance) {
    trend = "INCREASING";
  } else if (imbalance < previousImbalance) {
    trend = "DECREASING";
  } else {
    trend = "STABLE";
  }

  bool anomaly = detectAnomalies();
  updateRelay(imbalance, anomaly);
  handleButton(anomaly);
  updateFaultStateMachine(anomaly, imbalance);
  updateLCD(weak, strong, imbalance, avgSoC, trend, threshold);
  updateTelemetry(weak, strong, imbalance, avgSoC, trend, threshold);
  flushOneQueuedEvent();

  Serial.println("---- BMS Status ----");
  Serial.print("WiFi State: ");
  Serial.println(wifiState == WIFI_CONNECTED_STATE ? "CONNECTED" : (wifiState == WIFI_CONNECTING ? "CONNECTING" : "DISCONNECTED"));
  Serial.print("Imbalance: ");
  Serial.print(imbalance, 3);
  Serial.println(" V");
  Serial.print("Relay State: ");
  Serial.println(relayStateToString());
  Serial.print("Fault State: ");
  Serial.print(faultStateToString(faultState));
  Serial.print(" | Source: ");
  Serial.println(faultSourceToString(currentFaultSource));
  Serial.print("Fault Count: ");
  Serial.println(faultCount);

  previousImbalance = imbalance;
  delay(300);
}
