/*
 * Water Flow Sensor - v2.14
 * HYVIBE - Universitas Syiah Kuala
 */

extern int FLOW_SENSOR_PIN;

volatile int flowPulseCount = 0;
unsigned long lastFlowCheck = 0;
const unsigned long FLOW_CHECK_INTERVAL = 1000; // Check setiap 1 detik

void IRAM_ATTR flowPulseCounter() {
  flowPulseCount++;
}

void setupFlow() {
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseCounter, FALLING);
}

bool bacaStatusFlow() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastFlowCheck >= FLOW_CHECK_INTERVAL) {
    // Check jika ada pulse dalam interval terakhir
    bool isFlowing = (flowPulseCount > 0);
    
    // Reset counter
    flowPulseCount = 0;
    lastFlowCheck = currentTime;
    
    return isFlowing;
  }
  
  return (flowPulseCount > 0);
}
