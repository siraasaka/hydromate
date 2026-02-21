#include <WiFi.h>
#include <WiFiManager.h> // NEW: WiFiManager library
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <time.h>
#include <EEPROM.h>

// --- KONSTANTA ---
// WiFi credentials dihapus, diganti WiFiManager
#define FIREBASE_HOST "hino-snc-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "SFKx1nhLPrXpUj13lq9GMXb0tPvk7Lw1cwpNQ06M"

// WiFiManager AP Settings
#define AP_NAME "HydroMate-Setup"           // Nama WiFi AP
#define AP_PASSWORD "hydromate123"          // Password AP (min 8 karakter)

// --- OBJECTS ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- GLOBAL VARIABLES ---
bool wifiConnected = false;
bool offlineMode = false;
bool ntpSynced = false;
String currentDate = "offline";

// Alert & History Tracking
bool lastPompaUtamaError = false;
String lastphCondition = "";
String lastSuhuCondition = ""; // NEW: Track suhu condition
bool lastTdsLow = false;

struct DailyLog {
  float sum_ph = 0, sum_tds = 0, sum_suhu = 0;
  int sample_count = 0;
};
DailyLog todayLog;
unsigned long lastHistoryWrite = 0;
const unsigned long HISTORY_WRITE_INTERVAL = 300000; 
bool firstDataSent = false;

// WiFi & Firebase Recovery
unsigned long lastWiFiRetry = 0;
unsigned long lastFirebaseRetry = 0;
int wifiRetryCounter = 0;

// Calibration Sync (2-Way)
unsigned long lastCalibrationSync = 0;
const unsigned long CALIBRATION_SYNC_INTERVAL = 10000; // Check setiap 10 detik

// --- EXTERNAL VARIABLES ---
extern Mode modeSekarang;
extern float targetMinPPM, targetMaxPPM, targetMinPH, targetMaxPH;
extern float targetMinSuhu, targetMaxSuhu; // NEW
extern float phNeutralVoltage, phAcidVoltage, kValue;
extern bool phCalibrated;
extern bool firebaseConnected; // NEW: untuk LCD status
extern uint8_t mainPumpMode;   // Main pump cycle mode
extern unsigned long cycleStartTime;

// --- EXTERNAL FUNCTIONS (dari calibration.ino) ---
extern void calibratePH_Neutral();
extern void calibratePH_Acid();
extern void resetPH();
extern void calibrateTDS_Auto_WithPPM(float ppm); // Updated: dengan parameter PPM
extern void calibrateTDS_Manual();
extern void resetTDS();

// --- HELPER FUNCTIONS ---
String getTimeString() {
  struct tm tinfo;
  if (!getLocalTime(&tinfo)) return "00:00:00";
  char s[9];
  strftime(s, sizeof(s), "%H:%M:%S", &tinfo);
  return String(s);
}

String getDateString() {
  struct tm tinfo;
  if (!getLocalTime(&tinfo)) return "offline";
  char s[11];
  strftime(s, sizeof(s), "%Y-%m-%d", &tinfo);
  return String(s);
}

// Reset WiFi credentials (untuk re-config)
void resetWiFiSettings() {
  WiFiManager wm;
  wm.resetSettings();
  delay(3000);
  ESP.restart();
}

// --- PUMP CONTROL ---
void setPompaUtama(bool on) {
  digitalWrite(POMPA_UTAMA, on ? HIGH : LOW);
  if (Firebase.ready()) Firebase.RTDB.setBool(&fbdo, "/controls/pompa_utama", on);
}
void setPompaNutrisiA(bool on) {
  digitalWrite(POMPA_NUTRISI_A, on ? HIGH : LOW);
  if (Firebase.ready()) Firebase.RTDB.setBool(&fbdo, "/controls/pompa_nutrisi_a", on);
}
void setPompaNutrisiB(bool on) {
  digitalWrite(POMPA_NUTRISI_B, on ? HIGH : LOW);
  if (Firebase.ready()) Firebase.RTDB.setBool(&fbdo, "/controls/pompa_nutrisi_b", on);
}
void setPompaPhUp(bool on) {
  digitalWrite(POMPA_PH_UP, on ? HIGH : LOW);
  if (Firebase.ready()) Firebase.RTDB.setBool(&fbdo, "/controls/pompa_ph_up", on);
}
void setPompaPhDown(bool on) {
  digitalWrite(POMPA_PH_DOWN, on ? HIGH : LOW);
  if (Firebase.ready()) Firebase.RTDB.setBool(&fbdo, "/controls/pompa_ph_down", on);
}

// --- FUNGSI KALIBRASI ---
void uploadCalibrationData() {
  if (!Firebase.ready()) {
    return;
  }
  
  FirebaseJson j;
  j.add("ph_neutral_v", phNeutralVoltage);
  j.add("ph_acid_v", phAcidVoltage);
  j.add("ph_is_calibrated", phCalibrated);
  j.add("tds_k_value", kValue);
  
  if (Firebase.RTDB.updateNode(&fbdo, "/calibration", &j)) {
  } else {
  }
}

// --- REMOTE CALIBRATION VIA FIREBASE ---
void handleRemoteCalibration() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 5000) return; // Check setiap 5 detik
  lastCheck = millis();
  
  if (!Firebase.ready()) return;
  
  // Check jika ada command di Firebase
  if (Firebase.RTDB.getString(&fbdo, "/calibration/command")) {
    String cmd = fbdo.stringData();
    
    if (cmd == "" || cmd == "null") return; // Tidak ada command
    
    
    // Execute command
    if (cmd == "CAL7") {
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "CALIBRATING");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "Kalibrasi pH 7.0...");
      
      delay(2000);
      calibratePH_Neutral();
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "SUCCESS");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                              "pH 7.0 berhasil: " + String(phNeutralVoltage, 2) + "V");
      
    } else if (cmd == "CAL4") {
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "CALIBRATING");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "Kalibrasi pH 4.0...");
      
      delay(2000);
      calibratePH_Acid();
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "SUCCESS");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                              "Kalibrasi 2-titik selesai!");
      
    } else if (cmd == "PHCLEAR") {
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "CALIBRATING");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "Reset pH...");
      
      resetPH();
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "SUCCESS");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "pH direset ke default");
      
    } else if (cmd == "TDSK") {
      
      // Update status: SEDANG KALIBRASI
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "CALIBRATING");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "Membaca target PPM...");
      
      // Baca target PPM dari Firebase (dengan retry)
      float targetPPM = 0;
      bool readSuccess = false;
      
      for (int retry = 0; retry < 3; retry++) {
        if (Firebase.RTDB.getFloat(&fbdo, "/calibration/tds_target_ppm")) {
          targetPPM = fbdo.floatData();
          readSuccess = true;
          break;
        }
        delay(500); // Tunggu sebentar sebelum retry
      }
      
      if (readSuccess) {
        
        if (targetPPM >= 50 && targetPPM <= 5000) {
          // Update status
          Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                                  "Membaca sensor (30 detik)...");
          
          // Eksekusi kalibrasi dengan target PPM
          calibrateTDS_Auto_WithPPM(targetPPM);
          
          // Update status: SUKSES
          Firebase.RTDB.setString(&fbdo, "/calibration/status", "SUCCESS");
          Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                                  "Kalibrasi berhasil! K-Value: " + String(kValue, 3));
          
        } else {
          
          // Update status: GAGAL
          Firebase.RTDB.setString(&fbdo, "/calibration/status", "FAILED");
          Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                                  "Target PPM invalid (harus 50-5000 ppm)");
        }
      } else {
        
        // Update status: GAGAL
        Firebase.RTDB.setString(&fbdo, "/calibration/status", "FAILED");
        Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                                "Target PPM tidak ditemukan. Set /calibration/tds_target_ppm");
      }
      
    } else if (cmd == "RESETTDS") {
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "CALIBRATING");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "Reset TDS...");
      
      resetTDS();
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "SUCCESS");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", "K-Value direset ke 1.0");
      
    } else {
      
      Firebase.RTDB.setString(&fbdo, "/calibration/status", "FAILED");
      Firebase.RTDB.setString(&fbdo, "/calibration/status_message", 
                              "Command tidak dikenal: " + cmd);
    }
    
    // Clear command setelah dieksekusi
    Firebase.RTDB.setString(&fbdo, "/calibration/command", "");
  }
}

// --- CORE LOGIC ---
void setupFirebase() {
  // WiFiManager - Auto-connect atau buat AP untuk config
  WiFiManager wm;
  
  // Enable debug output
  wm.setDebugOutput(true);
  
  // Set timeout 300 detik (5 menit) - lebih lama untuk testing
  wm.setConfigPortalTimeout(300);
  
  // Custom AP settings
  // wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  
  Serial.println("\n📶 WiFiManager Starting...");
  Serial.println("📍 AP Name: " + String(AP_NAME));
  Serial.println("📍 AP Password: " + String(AP_PASSWORD));
  Serial.println("⏱️  Portal timeout: 300 seconds");
  Serial.println("🔍 Checking saved WiFi credentials...\n");
  
  // OPTIONAL: Force AP mode untuk testing
  // Uncomment line berikut untuk PAKSA buat AP (ignore saved WiFi)
  // wm.resetSettings(); // ← FORCE RESET (hapus setelah testing!)
  
  // Auto-connect ke WiFi tersimpan, atau buat AP jika gagal
  if (!wm.autoConnect(AP_NAME, AP_PASSWORD)) {
    Serial.println("❌ Failed to connect and timeout");
    Serial.println("🔄 Restarting ESP32 in 3 seconds...");
    delay(3000);
    ESP.restart();
  }
  
  // Berhasil connect - Print WiFi info
  Serial.println("\n✅ WiFi Connected!");
  Serial.println("📡 SSID: " + WiFi.SSID());
  Serial.println("🌐 IP: " + WiFi.localIP().toString());
  Serial.println("📶 RSSI: " + String(WiFi.RSSI()) + " dBm\n");
  
  // WiFi light sleep mode untuk hemat daya (kurangi panas)
  // WIFI_PS_NONE = no sleep (panas tinggi, stabilitas max)
  // WIFI_PS_MIN_MODEM = light sleep (hemat daya, stabilitas OK)
  // WIFI_PS_MAX_MODEM = max sleep (hemat max, bisa lag)
  WiFi.setSleep(WIFI_PS_MIN_MODEM); // ← Light sleep (recommended)
  
  // Firebase config
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  // Auto reconnect WiFi
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  Firebase.begin(&config, &auth);
  configTime(25200, 0, "id.pool.ntp.org", "pool.ntp.org");
  
}

void handleControlPolling() {
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < 2000) return;
  lastPoll = millis();

  if (Firebase.ready()) {
    if (Firebase.RTDB.getJSON(&fbdo, "/controls")) {
      FirebaseJson &j = fbdo.jsonObject();
      FirebaseJsonData r;
      if (j.get(r, "pompa_nutrisi_a")) digitalWrite(POMPA_NUTRISI_A, r.boolValue ? HIGH : LOW);
      if (j.get(r, "pompa_nutrisi_b")) digitalWrite(POMPA_NUTRISI_B, r.boolValue ? HIGH : LOW);
      if (j.get(r, "pompa_ph_up"))     digitalWrite(POMPA_PH_UP, r.boolValue ? HIGH : LOW);
      if (j.get(r, "pompa_ph_down"))   digitalWrite(POMPA_PH_DOWN, r.boolValue ? HIGH : LOW);
      if (j.get(r, "pompa_utama"))     digitalWrite(POMPA_UTAMA, r.boolValue ? HIGH : LOW);
    }
    
    if (Firebase.RTDB.getString(&fbdo, "/controls/mode")) {
      modeSekarang = (fbdo.stringData() == "AUTO") ? AUTO : MANUAL;
    }

    // Baca main pump mode
    if (Firebase.RTDB.getInt(&fbdo, "/controls/main_pump_mode")) {
      int newMode = fbdo.intData();
      if (newMode >= 0 && newMode <= 2 && newMode != mainPumpMode) {
        mainPumpMode = (uint8_t)newMode;
        EEPROM.write(20, mainPumpMode);
        EEPROM.commit();
        cycleStartTime = millis(); // Reset siklus saat mode berubah
      }
    }

    if (Firebase.RTDB.getJSON(&fbdo, "/config")) {
       FirebaseJson &j = fbdo.jsonObject();
       FirebaseJsonData r;
       if (j.get(r, "target_ph_min")) targetMinPH = r.floatValue;
       if (j.get(r, "target_ph_max")) targetMaxPH = r.floatValue;
       if (j.get(r, "target_ppm_min")) targetMinPPM = r.floatValue;
       if (j.get(r, "target_ppm_max")) targetMaxPPM = r.floatValue;
       if (j.get(r, "target_suhu_min")) targetMinSuhu = r.floatValue; // NEW
       if (j.get(r, "target_suhu_max")) targetMaxSuhu = r.floatValue; // NEW
    }
  }
}

void loopFirebase() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  offlineMode = !wifiConnected;

  // WiFi Self-Healing - WiFiManager will auto-reconnect
  if (!wifiConnected) {
    if (millis() - lastWiFiRetry > 30000) { // Check setiap 30 detik
      lastWiFiRetry = millis();
      wifiRetryCounter++;
      
      // Setelah 5x gagal, restart ESP32
      if (wifiRetryCounter > 5) {
        delay(3000);
        ESP.restart();
      }
    }
  } else {
    wifiRetryCounter = 0;
    
    // Firebase Self-Healing
    if (!Firebase.ready() && (millis() - lastFirebaseRetry > 7000)) {
        Firebase.reconnectWiFi(true);
        lastFirebaseRetry = millis();
    }
  }

  time_t now = time(nullptr);
  struct tm* tinfo = localtime(&now);
  ntpSynced = (tinfo->tm_year > 120); 

  handleControlPolling();
  handleRemoteCalibration(); // Check remote calibration command
  syncCalibrationFromFirebase(); // NEW: 2-way sync calibration

  // Update firebaseConnected status untuk LCD
  firebaseConnected = Firebase.ready();

  static unsigned long lastHb = 0;
  if (millis() - lastHb > 30000) {
    if (Firebase.ready()) {
      Firebase.RTDB.setString(&fbdo, "/device_status/last_seen", getTimeString());
      Firebase.RTDB.setBool(&fbdo, "/device_status/is_online", wifiConnected);
    }
    lastHb = millis();
  }
}

void kirimDataFirebase(float temp, float ph, float tds, bool isPumpOn) {
  if (!Firebase.ready()) {
    return;
  }

  // 1. Monitoring Dasar
  FirebaseJson mon;
  mon.add("ph", ph); 
  mon.add("ppm", tds); 
  mon.add("suhu", temp);
  mon.add("flow_status", isPumpOn); 
  mon.add("last_update", getTimeString());
  
  if (!Firebase.RTDB.updateNode(&fbdo, "/monitoring", &mon)) {
  }

  // 2. Alert TDS Low
  bool tdsLow = (tds < targetMinPPM);
  if (tdsLow != lastTdsLow) {
    if (Firebase.RTDB.setBool(&fbdo, "/alerts/tds_low", tdsLow)) {
      lastTdsLow = tdsLow;
    }
  }

  // 3. Alert Pompa Utama Error (Perintah vs Realita)
  bool pompaError = (digitalRead(POMPA_UTAMA) == HIGH && !isPumpOn);
  if (pompaError != lastPompaUtamaError) {
    if (Firebase.RTDB.setBool(&fbdo, "/alerts/pompa_utama_error", pompaError)) {
      lastPompaUtamaError = pompaError;
    }
  }

  // 4. Alert pH Condition
  String phCond = (ph < targetMinPH) ? "LOW" : (ph > targetMaxPH) ? "HIGH" : "NORMAL";
  if (phCond != lastphCondition) {
    if (Firebase.RTDB.setString(&fbdo, "/alerts/ph_condition", phCond)) {
      lastphCondition = phCond;
    }
  }

  // 5. Alert Suhu Condition (NEW)
  String suhuCond = (temp < targetMinSuhu) ? "LOW" : (temp > targetMaxSuhu) ? "HIGH" : "NORMAL";
  if (suhuCond != lastSuhuCondition) {
    if (Firebase.RTDB.setString(&fbdo, "/alerts/suhu_condition", suhuCond)) {
      lastSuhuCondition = suhuCond;
    }
  }

  // 6. History (Termasuk Suhu)
  if (ntpSynced) {
    String today = getDateString();
    if (today != currentDate && today != "offline") {
      currentDate = today; 
      todayLog = {0,0,0,0}; 
      firstDataSent = false;
    }
    
    todayLog.sum_ph += ph; 
    todayLog.sum_tds += tds; 
    todayLog.sum_suhu += temp;
    todayLog.sample_count++;

    unsigned long ms = millis();
    if (!firstDataSent || (ms - lastHistoryWrite >= HISTORY_WRITE_INTERVAL)) {
      if (todayLog.sample_count > 0) {
        FirebaseJson hist;
        hist.add("avg_ph", todayLog.sum_ph / todayLog.sample_count);
        hist.add("avg_tds", todayLog.sum_tds / todayLog.sample_count);
        hist.add("avg_suhu", todayLog.sum_suhu / todayLog.sample_count);
        
        if (Firebase.RTDB.updateNode(&fbdo, "/history/" + today, &hist)) {
          lastHistoryWrite = ms; 
          firstDataSent = true;
        }
      }
    }
  }
}

// ========================================
// SYNC CALIBRATION FROM FIREBASE (2-WAY)
// ========================================
void syncCalibrationFromFirebase() {
  if (!Firebase.ready()) return;
  
  unsigned long now = millis();
  if (now - lastCalibrationSync < CALIBRATION_SYNC_INTERVAL) return;
  lastCalibrationSync = now;
  
  // Read calibration values dari Firebase
  float fbPhNeutral = 0, fbPhAcid = 0, fbKValue = 0;
  bool fbPhCalibrated = false;
  bool hasChanges = false;
  
  // Get pH Neutral Voltage
  if (Firebase.RTDB.getFloat(&fbdo, "/calibration/ph_neutral_v")) {
    fbPhNeutral = fbdo.floatData();
    if (abs(fbPhNeutral - phNeutralVoltage) > 0.01) { // Threshold 0.01V
      phNeutralVoltage = fbPhNeutral;
      EEPROM.put(0, phNeutralVoltage);
      hasChanges = true;
    }
  }
  
  // Get pH Acid Voltage
  if (Firebase.RTDB.getFloat(&fbdo, "/calibration/ph_acid_v")) {
    fbPhAcid = fbdo.floatData();
    if (abs(fbPhAcid - phAcidVoltage) > 0.01) {
      phAcidVoltage = fbPhAcid;
      EEPROM.put(4, phAcidVoltage);
      hasChanges = true;
    }
  }
  
  // Get pH Calibrated Status
  if (Firebase.RTDB.getBool(&fbdo, "/calibration/ph_is_calibrated")) {
    fbPhCalibrated = fbdo.boolData();
    if (fbPhCalibrated != phCalibrated) {
      phCalibrated = fbPhCalibrated;
      EEPROM.put(8, phCalibrated);
      hasChanges = true;
    }
  }
  
  // Get TDS K-Value
  if (Firebase.RTDB.getFloat(&fbdo, "/calibration/tds_k_value")) {
    fbKValue = fbdo.floatData();
    if (abs(fbKValue - kValue) > 0.01) {
      kValue = fbKValue;
      EEPROM.put(16, kValue);
      hasChanges = true;
    }
  }
  
  // Commit ke EEPROM jika ada perubahan
  if (hasChanges) {
    EEPROM.commit();
  }
}
