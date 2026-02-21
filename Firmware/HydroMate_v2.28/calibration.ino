/*
 * Unified Calibration System - v2.26
 * UPGRADE:
 * - TDS kalibrasi otomatis (input PPM real)
 * - Auto-upload ke Firebase setelah kalibrasi
 * - Remote calibration via Firebase (polling /calibration/command)
 * - WiFiManager integration (WiFi reset command)
 */

// Function prototypes dari sensor files
void calibratePH_Neutral();
void calibratePH_Acid();
void resetPH();
void getPhStatus();

void calibrateTDS_Auto();      // User-facing function (prompt input)
void calibrateTDS_Auto_WithPPM(float ppm); // Internal function (dengan parameter)
void calibrateTDS_Manual();    // Kalibrasi manual (K langsung)
void resetTDS();
void getTdsStatus();

// External function untuk WiFi reset
extern void resetWiFiSettings(); // NEW

// External variables
extern float phNeutralVoltage, phAcidVoltage;
extern bool phCalibrated;
extern float kValue;
extern float temperature;

// ========== CHECK COMMAND DARI SERIAL ==========
void checkCalibrationCommand() {
  if (!Serial.available()) return;
  
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toUpperCase();
  
  // ENTER → Tampilkan menu
  if (command.length() == 0) {
    showCalibrationMenu();
    return;
  }
  
  // ========== pH COMMANDS ==========
  if (command == "CAL7") {
    delay(2000);
    calibratePH_Neutral();
    
  } else if (command == "CAL4") {
    delay(2000);
    calibratePH_Acid();
    
  } else if (command == "PHCLEAR") {
    resetPH();
    
  // ========== TDS COMMANDS (UPDATED) ==========
  } else if (command == "TDSK") {
    // Kalibrasi otomatis (default)
    calibrateTDS_Auto();
    
  } else if (command == "TDSKM") {
    // Kalibrasi manual (K langsung) - untuk advanced user
    calibrateTDS_Manual();
    
  } else if (command == "RESETTDS") {
    resetTDS();
    
  // ========== INFO COMMAND ==========
  } else if (command == "INFO") {
    showSystemInfo();
    
  // ========== WiFi RESET COMMAND (NEW) ==========
  } else if (command == "WIFIRESET") {
    
    // Tunggu konfirmasi
    unsigned long timeout = millis() + 10000; // 10 detik
    while (millis() < timeout) {
      if (Serial.available()) {
        String confirm = Serial.readStringUntil('\n');
        confirm.trim();
        confirm.toUpperCase();
        
        if (confirm == "YES") {
          resetWiFiSettings();
          return;
        } else {
          return;
        }
      }
    }
    
  // ========== UNKNOWN COMMAND ==========
  } else {
  }
}

// ========== TAMPILKAN MENU KALIBRASI ==========
void showCalibrationMenu() {
}

// ========== TAMPILKAN INFO SEMUA SENSOR ==========
void showSystemInfo() {
  
  // pH Status
  getPhStatus();
  
  
  // TDS Status
  getTdsStatus();
  
  
  // Temperature
  
}
