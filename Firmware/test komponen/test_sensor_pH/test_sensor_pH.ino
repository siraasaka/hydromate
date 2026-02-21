/*
 * ============================================
 * KODE SENSOR pH & KALIBRASI - HydroMate v2.18
 * ============================================
 * Extract dari proyek lengkap untuk keperluan kalibrasi
 */

// ========== VARIABEL GLOBAL ==========
#include <EEPROM.h>

// Pin sensor pH
#define PH_PIN 34  // Sesuaikan dengan pin Anda

// Variabel kalibrasi pH
float phNeutralVoltage = 2.7;  // Default voltage untuk pH 7.0 (V)
float phAcidVoltage = 3.3;     // Default voltage untuk pH 4.0 (V)
bool phCalibrated = false;
float calibration_value = 21.34 + 1.47;  //faktor kalibrasi 1 titik (failsafe)
unsigned long int avgval;
int buffer_arr[10], temp;

// Konstanta ADC ESP32
const float adcRange = 4095.0;    // 12-bit ADC
const float aref = 3.3;           // Reference voltage 3.3V


// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  pinMode(PH_PIN, INPUT);
  
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Load calibration dari EEPROM
  EEPROM.get(0, phNeutralVoltage);
  EEPROM.get(4, phAcidVoltage);
  EEPROM.get(8, phCalibrated);
  
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║    pH SENSOR CALIBRATION SYSTEM       ║");
  Serial.println("║         HydroMate v2.18               ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
  
  showStatus();
  Serial.println("\nTekan ENTER untuk melihat menu kalibrasi");
}


// ========== LOOP UTAMA ==========
void loop() {
  // Baca pH setiap 1 detik
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 1000) {
    float ph = bacaPH();
    Serial.printf("pH: %.1f | Voltage: %.2f V\n", ph, getVoltage());
    lastRead = millis();
  }
  
  // Cek command kalibrasi
  checkCalibrationCommand();
}

// ========== Baca Volatage ==========
float readFilteredVoltage() {
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(PH_PIN);
    delay(30);
  }

  // sort
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  avgval = 0;
  for (int i = 2; i < 8; i++)
    avgval += buffer_arr[i];

  float adcAvg = avgval / 6.0;
  return adcAvg * aref / adcRange; // VOLT
}


// ========== FUNGSI BACA pH ==========
float bacaPH() {
  float volt = readFilteredVoltage();
  float phValue;

  if (phCalibrated) {
    // === DFROBOT 2-POINT LINEAR CALIBRATION ===
    float slope = (7.0 - 4.0) / (phNeutralVoltage - phAcidVoltage);
    float intercept = 7.0 - slope * phNeutralVoltage;
    phValue = slope * volt + intercept;
  } else {
    // fallback (sementara)
    phValue = -5.70 * volt + calibration_value;
  }

  if (phValue < 0) phValue = 0;
  if (phValue > 14) phValue = 14;
  return phValue;
}


// ========== FUNGSI BACA VOLTAGE ==========
float getVoltage() {
  return readFilteredVoltage(); // V
}


// ========== PROSES COMMAND KALIBRASI ==========
void processPhCommand(String command) {
  if (command == "CAL7") {
    // Kalibrasi pH 7.0
    phNeutralVoltage = readFilteredVoltage();
    
    EEPROM.put(0, phNeutralVoltage);
    EEPROM.commit();
    
    Serial.printf("✅ pH 7.0 dikalibrasi: %.1f V\n", phNeutralVoltage);
    
  } else if (command == "CAL4") {
    // Kalibrasi pH 4.0
    phAcidVoltage = readFilteredVoltage();
    phCalibrated = true;
    
    EEPROM.put(4, phAcidVoltage);
    EEPROM.put(8, phCalibrated);
    EEPROM.commit();
    
    Serial.printf("✅ pH 4.0 dikalibrasi: %.1f V\n", phAcidVoltage);
    
  } else if (command == "PHCLEAR") {
    // Reset kalibrasi ke default
    phCalibrated = false;
    phNeutralVoltage = 2.7;
    phAcidVoltage = 3.3;
    
    EEPROM.put(0, phNeutralVoltage);
    EEPROM.put(4, phAcidVoltage);
    EEPROM.put(8, phCalibrated);
    EEPROM.commit();
    
    Serial.println("✅ Kalibrasi pH dihapus (reset ke default)");
  }
  
  showStatus();
}


// ========== CEK COMMAND DARI SERIAL ==========
void checkCalibrationCommand() {
  if (!Serial.available()) return;
  
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toUpperCase();
  
  if (command.length() == 0) {
    // Tampilkan menu
    showMenu();
    return;
  }
  
  // Proses command pH
  if (command == "CAL7" || command == "CAL4" || command == "PHCLEAR") {
    processPhCommand(command);
  }
  else if (command == "STATUS") {
    showStatus();
  }
  else {
    Serial.println("❌ Command tidak dikenal. Tekan ENTER untuk menu.");
  }
}


// ========== TAMPILKAN MENU ==========
void showMenu() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║       MENU KALIBRASI pH v2.18         ║");
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.println("║ Command:                              ║");
  Serial.println("║   CAL7     - Kalibrasi pH 7.0         ║");
  Serial.println("║   CAL4     - Kalibrasi pH 4.0         ║");
  Serial.println("║   PHCLEAR  - Hapus kalibrasi          ║");
  Serial.println("║   STATUS   - Lihat status kalibrasi   ║");
  Serial.println("╚═══════════════════════════════════════╝\n");
}


// ========== TAMPILKAN STATUS ==========
void showStatus() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║      STATUS KALIBRASI pH v2.18        ║");
  Serial.println("╠═══════════════════════════════════════╣");
  Serial.printf("║ Status: %s                        ║\n", phCalibrated ? "TERKALIBRASI ✅" : "BELUM KALIBRASI ❌");
  Serial.printf("║ pH 7.0 Voltage:  %-8.2f V          ║\n", phNeutralVoltage);
  Serial.printf("║ pH 4.0 Voltage:  %-8.2f V          ║\n", phAcidVoltage);
  Serial.println("╚═══════════════════════════════════════╝\n");
}


/*
 * ============================================
 * CARA KALIBRASI:
 * ============================================
 * 
 * 1. Upload kode ini ke ESP32
 * 2. Buka Serial Monitor (115200 baud)
 * 3. Siapkan larutan buffer pH 7.0 dan pH 4.0
 * 
 * LANGKAH KALIBRASI:
 * 
 * Step 1: Kalibrasi pH 7.0
 *   - Celupkan probe ke larutan buffer pH 7.0
 *   - Tunggu pembacaan stabil (±30 detik)
 *   - Ketik: CAL7
 *   - Tekan Enter
 * 
 * Step 2: Kalibrasi pH 4.0
 *   - Bilas probe dengan air DI
 *   - Celupkan ke larutan buffer pH 4.0
 *   - Tunggu pembacaan stabil (±30 detik)
 *   - Ketik: CAL4
 *   - Tekan Enter
 * 
 * Step 3: Verifikasi
 *   - Ketik: STATUS
 *   - Check apakah "TERKALIBRASI ✅"
 *   - Test dengan larutan buffer lain
 * 
 * RESET KALIBRASI:
 *   - Ketik: PHCLEAR
 *   - Kalibrasi akan kembali ke default
 * 
 * TIPS:
 *   - Selalu bilas probe sebelum pindah larutan
 *   - Gunakan air DI (Deionized) untuk bilas
 *   - Kalibrasi ulang setiap 1-2 minggu
 *   - Simpan probe dalam larutan storage (pH 4.0)
 * 
 * ============================================
 */
