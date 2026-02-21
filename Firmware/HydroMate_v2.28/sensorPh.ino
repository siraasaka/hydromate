/*
 * pH Sensor - v2.20
 * - 2-point calibration (pH 7.0 & pH 4.0)
 * - DFRobot linear formula
 * - ADC filtering (10 samples, buang 2 atas + 2 bawah)
 * - AUTO UPLOAD ke Firebase setelah kalibrasi
 */

extern int PH_PIN;
extern float phNeutralVoltage;
extern float phAcidVoltage;
extern bool phCalibrated;
extern float calibration_value;
extern float aref;
extern float adcRange;

// Function prototype untuk upload Firebase
void uploadCalibrationData();

// Buffer untuk filtering
unsigned long int avgval;
int buffer_arr[10], tempValue; // Renamed: temp → tempValue (avoid conflict)

// ========== SETUP pH ==========
void setupPH() {
  pinMode(PH_PIN, INPUT);
  
  // Load calibration dari EEPROM
  EEPROM.get(0, phNeutralVoltage);
  EEPROM.get(4, phAcidVoltage);
  EEPROM.get(8, phCalibrated);
  
  // Validasi data EEPROM
  if (isnan(phNeutralVoltage) || phNeutralVoltage < 0.5 || phNeutralVoltage > 3.5) {
    phNeutralVoltage = 2.7;
  }
  if (isnan(phAcidVoltage) || phAcidVoltage < 0.5 || phAcidVoltage > 3.5) {
    phAcidVoltage = 3.3;
  }
  
  if (phCalibrated) {
  }
}

// ========== FILTER ADC - 10 SAMPLES ==========
float readFilteredVoltage() {
  // Baca 10 samples
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(PH_PIN);
    delay(30);
  }

  // Bubble sort
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        tempValue = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = tempValue;
      }
    }
  }

  // Ambil 6 nilai tengah (buang 2 atas + 2 bawah)
  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }

  float adcAvg = avgval / 6.0;
  return adcAvg * aref / adcRange; // Return dalam VOLT
}

// ========== BACA pH ==========
float bacaPH() {
  float volt = readFilteredVoltage();
  float phValue;

  if (phCalibrated) {
    // === DFROBOT 2-POINT LINEAR CALIBRATION ===
    float slope = (7.0 - 4.0) / (phNeutralVoltage - phAcidVoltage);
    float intercept = 7.0 - slope * phNeutralVoltage;
    phValue = slope * volt + intercept;
  } else {
    // Fallback formula (jika belum kalibrasi)
    phValue = -5.70 * volt + calibration_value;
  }

  // Clamp ke range valid
  if (phValue < 0) phValue = 0;
  if (phValue > 14) phValue = 14;
  
  return phValue;
}

// ========== GET RAW VOLTAGE (untuk display/debug) ==========
float getPhVoltage() {
  return readFilteredVoltage();
}

// ========== KALIBRASI pH 7.0 ==========
void calibratePH_Neutral() {
  phNeutralVoltage = readFilteredVoltage();
  phCalibrated = true;
  
  EEPROM.put(0, phNeutralVoltage);
  EEPROM.commit();
  
  
  // AUTO UPLOAD ke Firebase
  uploadCalibrationData();
}

// ========== KALIBRASI pH 4.0 ==========
void calibratePH_Acid() {
  phAcidVoltage = readFilteredVoltage();
  phCalibrated = true;
  
  EEPROM.put(4, phAcidVoltage);
  EEPROM.put(8, phCalibrated);
  EEPROM.commit();
  
  
  // AUTO UPLOAD ke Firebase
  uploadCalibrationData();
}

// ========== RESET KALIBRASI pH ==========
void resetPH() {
  phCalibrated = false;
  phNeutralVoltage = 2.7;
  phAcidVoltage = 3.3;
  
  EEPROM.put(0, phNeutralVoltage);
  EEPROM.put(4, phAcidVoltage);
  EEPROM.put(8, phCalibrated);
  EEPROM.commit();
  
  
  // AUTO UPLOAD ke Firebase
  uploadCalibrationData();
}

// ========== GET STATUS pH (untuk INFO command) ==========
void getPhStatus() {
}
