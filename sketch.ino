#define NUM_CELLS 4

const int cellPins[NUM_CELLS] = {34, 35, 32, 33};

const float ADC_MAX = 4095.0;
const float V_MIN = 3.0;   // empty cell voltage
const float V_MAX = 4.2;   // full cell voltage

float cellVoltages[NUM_CELLS];
float previousImbalance = 0.0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
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

void loop() {
  updateAllCells();

  float imbalance = calculateImbalance();
  int weak = findWeakestCell();
  int strong = findStrongestCell();

  float avgVoltage = 0;
  for (int i = 0; i < NUM_CELLS; i++) avgVoltage += cellVoltages[i];
  avgVoltage /= NUM_CELLS;
  float avgSoC = estimateSoC(avgVoltage);

  float threshold = getAdaptiveThreshold(avgSoC);

  String trend;
  if (imbalance > previousImbalance) trend = "INCREASING";
  else if (imbalance < previousImbalance) trend = "DECREASING";
  else trend = "STABLE";

  Serial.println("---- BMS Status ----");
  for (int i = 0; i < NUM_CELLS; i++) {
    Serial.print("Cell "); Serial.print(i + 1); Serial.print(": ");
    Serial.print(cellVoltages[i]); Serial.println(" V");
  }
  Serial.print("Weakest: Cell "); Serial.println(weak + 1);
  Serial.print("Strongest: Cell "); Serial.println(strong + 1);
  Serial.print("Imbalance: "); Serial.print(imbalance, 3); Serial.println(" V");
  Serial.print("Trend: "); Serial.println(trend);
  Serial.print("Adaptive Threshold: "); Serial.println(threshold, 3);
  if (imbalance > threshold) {
    Serial.println("WARNING: Imbalance exceeds adaptive threshold!");
  }

  previousImbalance = imbalance;
  delay(1000);
}
