/*
 * TDS Sensor - v2.20
 * - KALIBRASI OTOMATIS: Input PPM real → hitung K otomatis
 * - ADC filtering (10 samples, buang 2 atas + 2 bawah)
 * - AUTO UPLOAD ke Firebase setelah kalibrasi
 */

#include <EEPROM.h>

// Konfigurasi
extern int TDS_PIN;
int ADDR_K_VALUE = 16; 
extern float kValue;
extern float aref;
extern float adcRange;
extern float temperature;

// Function prototype untuk upload Firebase
void uploadCalibrationData();

// Buffer untuk filtering
int tds_buffer[10];
int tds_temp;

// ========== SETUP TDS ==========
void setupTDS() {
  pinMode(TDS_PIN, INPUT);
  
  // Load kValue dari EEPROM
  EEPROM.get(ADDR_K_VALUE, kValue);
  
  // Validasi kValue
  if (isnan(kValue) || kValue < 0.25 || kValue > 4.0) {
    kValue = 1.0;
  }
  
}

// ========== FILTER ADC - 10 SAMPLES ==========
float readFilteredTdsVoltage() {
  // Baca 10 samples
  for (int i = 0; i < 10; i++) {
    tds_buffer[i] = analogRead(TDS_PIN);
    delay(30);
  }

  // Bubble sort
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (tds_buffer[i] > tds_buffer[j]) {
        tds_temp = tds_buffer[i];
        tds_buffer[i] = tds_buffer[j];
        tds_buffer[j] = tds_temp;
      }
    }
  }

  // Ambil 6 nilai tengah (buang 2 atas + 2 bawah)
  unsigned long sum = 0;
  for (int i = 2; i < 8; i++) {
    sum += tds_buffer[i];
  }

  float adcAvg = sum / 6.0;
  return adcAvg * aref / adcRange; // Return dalam VOLT
}

// ========== BACA TDS (RAW - tanpa K) ==========
// Untuk kalibrasi internal
float readRawTDS(float suhu) {
  float voltage = readFilteredTdsVoltage();
  
  // Rumus Polinomial DFRobot
  float ecValue = (133.42 * pow(voltage, 3) - 255.86 * pow(voltage, 2) + 857.39 * voltage);
  
  // Kompensasi temperatur
  float ecValue25 = ecValue / (1.0 + 0.02 * (suhu - 25.0)); 
  
  // Return TDS base (sebelum dikalikan K)
  return ecValue25 * 0.5;
}

// ========== BACA TDS (FINAL - dengan K) ==========
float bacaTDS(float suhu) {
  // FAILSAFE: Jika suhu tidak valid, gunakan 25°C
  if (suhu < -55 || suhu > 125 || isnan(suhu)) {
    temperature = 25.0;
  } else {
    temperature = suhu;
  }
  
  float tdsBase = readRawTDS(temperature);
  return tdsBase * kValue;
}

// ========== GET RAW VOLTAGE (untuk display/debug) ==========
float getTdsVoltage() {
  return readFilteredTdsVoltage();
}

// ========== KALIBRASI TDS OTOMATIS ==========
void calibrateTDS_Auto() {
  calibrateTDS_Auto_WithPPM(0); // 0 = prompt user untuk input
}

// Internal function dengan parameter PPM (untuk remote calibration)
void calibrateTDS_Auto_WithPPM(float realPPM) {
  bool isRemote = (realPPM > 0);
  
  if (!isRemote) {
    // LOCAL: Prompt user input
    
    // Tunggu input dari serial
    while (!Serial.available()) { 
      delay(10); 
    }
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    realPPM = input.toFloat();
  } else {
    // REMOTE: Gunakan PPM dari Firebase
  }
  
  if (realPPM < 50 || realPPM > 5000) {
    return;
  }
  
  delay(3000); // Tunggu stabilisasi
  
  // Baca TDS base (tanpa K) dengan suhu saat ini
  float tdsBase = readRawTDS(temperature);
  
  // Hitung K otomatis
  float newK = realPPM / tdsBase;
  
  // Validasi K hasil perhitungan
  if (newK < 0.5 || newK > 2.0) {
    return;
  }
  
  // Save K-Value
  kValue = newK;
  EEPROM.put(ADDR_K_VALUE, kValue);
  EEPROM.commit();
  
  // Display hasil
  
  // AUTO UPLOAD ke Firebase
  uploadCalibrationData();
}

// ========== KALIBRASI TDS MANUAL (K langsung) ==========
void calibrateTDS_Manual() {
  
  // Tunggu input dari serial
  while (!Serial.available()) { 
    delay(10); 
  }
  
  String input = Serial.readStringUntil('\n');
  input.trim();
  float newK = input.toFloat();
  
  if (newK >= 0.5 && newK <= 2.0) {
    kValue = newK;
    EEPROM.put(ADDR_K_VALUE, kValue);
    EEPROM.commit();
    
    
    // AUTO UPLOAD ke Firebase
    uploadCalibrationData();
  } else {
  }
}

// ========== RESET TDS ==========
void resetTDS() {
  kValue = 1.0;
  EEPROM.put(ADDR_K_VALUE, kValue);
  EEPROM.commit();
  
  
  // AUTO UPLOAD ke Firebase
  uploadCalibrationData();
}

// ========== GET STATUS TDS (untuk INFO command) ==========
void getTdsStatus() {
}
