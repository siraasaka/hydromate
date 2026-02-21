/*
 * HydroMate v2.28 - Main Pump Cycle Control
 * HYVIBE - Universitas Syiah Kuala
 * 
 * Changes from v2.27:
 * 1. NEW: Main pump cycle mode (15min/hr, 30min/hr, always on)
 * 2. NEW: Override pompa utama saat koreksi pH/PPM
 * 3. NEW: Siklus resume (tidak reset) setelah override selesai
 * 4. NEW: Firebase node /controls/main_pump_mode (0=15min, 1=30min, 2=alwaysOn)
 * 5. NEW: EEPROM backup main_pump_mode untuk offline fallback
 * 6. FIX: Koreksi PPM pakai pasangan A+B penuh sebelum cek ulang
 */

#include <Wire.h>
#include <EEPROM.h>

// ====== MODE ======
enum Mode { MANUAL, AUTO };
Mode modeSekarang = MANUAL;

// ====== TARGET CONFIG (GLOBAL) ======
// Default values - akan di-load dari EEPROM saat boot
float targetMinPPM = 800; 
float targetMaxPPM = 1200;
float targetMinPH  = 6.0;
float targetMaxPH  = 7.0;
float targetMinSuhu = 24.0;  
float targetMaxSuhu = 28.0;

// ====== PIN DEFINITIONS ======
#define POMPA_NUTRISI_A 4
#define POMPA_NUTRISI_B 2
#define POMPA_PH_UP 5
#define POMPA_PH_DOWN 15
#define POMPA_UTAMA 19

// ====== SENSOR & CALIBRATION VARIABLES ======
int TDS_PIN = 34;
int PH_PIN = 35;
int FLOW_SENSOR_PIN = 25;
float kValue = 1.0;
float aref = 3.3;
float adcRange = 4095.0;
float temperature = 25.0;

// pH Calibration values (2-point)
float phNeutralVoltage = 2.7;  // Default voltage untuk pH 7.0 (V)
float phAcidVoltage = 3.3;     // Default voltage untuk pH 4.0 (V)
bool phCalibrated = false;
float calibration_value = 21.34 + 1.47;  // Failsafe formula

// LCD & Sensor readings (for LCD display)
float ph = 0.0;
float tds = 0.0;
float temp = 25.0;
bool pompaUtamaRunning = false;
bool firebaseConnected = false;

// ====== FUNCTION PROTOTYPES ======
// Firebase
void setupFirebase();
void loopFirebase();
void kirimDataFirebase(float temp, float ph, float tds, bool isPumpOn);
void checkCalibrationCommand();

// LCD
void setupLCD();
void updateLCD();
void displayNotification(String line1, String line2, int duration);
void displayError(String errorMsg);

// Pumps
void setPompaNutrisiA(bool on);
void setPompaNutrisiB(bool on);
void setPompaPhUp(bool on);
void setPompaPhDown(bool on);
void setPompaUtama(bool on);

void setupDS18B20();
void setupPH();
void setupTDS();
void setupFlow();
float bacaSuhu();
float bacaPH();
float bacaTDS(float suhu);
bool bacaStatusFlow();

// External status flags dari firebaseHandler
extern bool wifiConnected;
extern bool offlineMode;
extern bool ntpSynced;

// ====== AUTO CONFIG ======
unsigned long lastPumpAction = 0;
const unsigned long PUMP_COOLDOWN = 30000;  // 30 detik
const unsigned long PUMP_DURATION = 3000;   // 3 detik

// ====== MAIN PUMP CYCLE ======
// Mode: 0 = 15min ON / 60min cycle, 1 = 30min ON / 60min cycle, 2 = always ON
uint8_t mainPumpMode = 2;  // Default: always ON (simpan di EEPROM addr 20)

// Siklus timer - menggunakan offset dalam siklus 60 menit
// Saat override aktif, timer tidak di-reset → resume setelah override selesai
unsigned long cycleStartTime = 0;  // Kapan siklus 60 menit dimulai
bool isOverrideActive = false;     // Flag: sedang override karena koreksi
unsigned long overrideElapsed = 0; // Waktu yg sudah terpakai saat override mulai

// ====== CACHE ======
float suhuTerakhir = -999;
float phTerakhir   = -999;
float tdsTerakhir  = -999;
bool statusPompaTerakhir = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize EEPROM
  EEPROM.begin(512);
  
  // Load mainPumpMode dari EEPROM (addr 20), validasi range
  uint8_t savedMode = EEPROM.read(20);
  if (savedMode <= 2) mainPumpMode = savedMode;
  cycleStartTime = millis(); // Mulai siklus dari boot

  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║    HydroMate v2.26                     ║");
  Serial.println("║    WiFiManager Integration             ║");
  Serial.println("║    + Auto WiFi Setup                   ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Setup LCD I2C
  setupLCD();

  // Setup pins
  pinMode(POMPA_NUTRISI_A, OUTPUT);
  pinMode(POMPA_NUTRISI_B, OUTPUT);
  pinMode(POMPA_PH_UP, OUTPUT);
  pinMode(POMPA_PH_DOWN, OUTPUT);
  pinMode(POMPA_UTAMA, OUTPUT);
  
  // Pastikan semua mati di awal
  digitalWrite(POMPA_NUTRISI_A, LOW);
  digitalWrite(POMPA_NUTRISI_B, LOW);
  digitalWrite(POMPA_PH_UP, LOW);
  digitalWrite(POMPA_PH_DOWN, LOW);
  digitalWrite(POMPA_UTAMA, LOW);

  // 1. Jalankan Firebase Setup
  setupFirebase();

  // 2. Inisialisasi Sensor
  setupDS18B20();
  setupPH();
  setupTDS(); 
  setupFlow();
}

void loop() {
  // 1. Update Firebase Loop PERTAMA (Agar status wifiConnected ter-update)
  loopFirebase();

  // 2. Cek perintah kalibrasi
  checkCalibrationCommand();
  
  // 3. Baca Sensor
  float suhu = bacaSuhu();
  float ph   = bacaPH();
  float tds  = bacaTDS(suhu); 
  bool isPumpOn = bacaStatusFlow();

  // 4. Update Firebase (Berdasarkan variabel wifiConnected yang terbaru)
  static unsigned long lastFirebaseUpdate = 0;
  if (millis() - lastFirebaseUpdate > 2000) {
    if (abs(tds - tdsTerakhir) > 5 || abs(ph - phTerakhir) > 0.05 || 
        abs(suhu - suhuTerakhir) > 0.5 || isPumpOn != statusPompaTerakhir) {
      
      kirimDataFirebase(suhu, ph, tds, isPumpOn);
      
      tdsTerakhir = tds;
      phTerakhir = ph;
      suhuTerakhir = suhu;
      statusPompaTerakhir = isPumpOn;
    }
    lastFirebaseUpdate = millis();
  }

  // 5. Logika Otomatis
  if (modeSekarang == AUTO) {
    autoControl(tds, ph);
  }

  // 6. Update LCD Display
  updateLCD();

  // 7. Update global variables untuk LCD
  ::ph = ph;
  ::tds = tds;
  ::temp = suhu;
  ::pompaUtamaRunning = isPumpOn;

  // Delay untuk kurangi CPU load & panas
  // 100ms = 10x loop per detik (cukup responsif, hemat daya)
  delay(100);
}

// ====== MAIN PUMP CYCLE CONTROL ======
// Kembalikan true jika pompa utama seharusnya ON berdasarkan siklus
bool shouldMainPumpBeOn() {
  if (mainPumpMode == 2) return true; // Always ON

  unsigned long onDuration = (mainPumpMode == 0) ? 15UL * 60000 : 30UL * 60000; // 15 atau 30 menit
  const unsigned long cycleDuration = 60UL * 60000; // 60 menit

  unsigned long elapsed = (millis() - cycleStartTime) % cycleDuration;
  return (elapsed < onDuration);
}

void autoControl(float tds, float ph) {
    unsigned long currentTime = millis();

    // Tentukan apakah perlu koreksi
    bool needCorrection = (tds < targetMinPPM) || (ph < targetMinPH) || (ph > targetMaxPH);

    // --- Logika Override & Siklus Pompa Utama ---
    if (needCorrection) {
        // Override: paksa pompa utama ON
        if (!isOverrideActive) {
            isOverrideActive = true;
            // Simpan posisi elapsed dalam siklus saat override mulai
            if (mainPumpMode != 2) {
                overrideElapsed = (millis() - cycleStartTime) % (60UL * 60000);
            }
        }
        if (digitalRead(POMPA_UTAMA) == LOW) setPompaUtama(true);
    } else {
        // Override selesai → resume siklus dari posisi terakhir
        if (isOverrideActive) {
            isOverrideActive = false;
            if (mainPumpMode != 2) {
                // Adjust cycleStartTime supaya elapsed tetap sama
                cycleStartTime = millis() - overrideElapsed;
            }
        }
        // Kontrol pompa utama sesuai siklus
        bool shouldBeOn = shouldMainPumpBeOn();
        if (shouldBeOn && digitalRead(POMPA_UTAMA) == LOW)  setPompaUtama(true);
        if (!shouldBeOn && digitalRead(POMPA_UTAMA) == HIGH) setPompaUtama(false);
    }

    // --- Koreksi hanya jika cooldown selesai ---
    if (currentTime - lastPumpAction < PUMP_COOLDOWN) return;

    // Priority 1: Nutrisi (dosing A+B sebagai pasangan penuh)
    if (tds < targetMinPPM) {
        // Pompa A
        setPompaNutrisiA(true);
        delay(PUMP_DURATION);
        setPompaNutrisiA(false);

        delay(1000); // Jeda A → B

        // Pompa B
        setPompaNutrisiB(true);
        delay(PUMP_DURATION);
        setPompaNutrisiB(false);

        lastPumpAction = millis();
        return;
    }

    // Priority 2: pH
    if (ph < targetMinPH) {
        setPompaPhUp(true);
        delay(PUMP_DURATION);
        setPompaPhUp(false);
        lastPumpAction = millis();
    } else if (ph > targetMaxPH) {
        setPompaPhDown(true);
        delay(PUMP_DURATION);
        setPompaPhDown(false);
        lastPumpAction = millis();
    }
}
