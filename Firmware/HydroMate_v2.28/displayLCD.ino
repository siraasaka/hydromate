/*
 * displayLCD.ino - LCD I2C Display Handler
 * HydroMate v2.28
 * 
 * LCD 16x2 I2C - Simple 2-Mode Display
 * 
 * AUTO-DETECT FEATURE:
 * - Kalau LCD terpasang (I2C address 0x27) → Auto enable
 * - Kalau LCD tidak terpasang → Auto disable (no I2C error spam)
 * - No need manual config!
 * 
 * Mode 0: Sensor Data (pH, PPM, Suhu, Mode)
 * Mode 1: System Status (Pompa, WiFi)
 */

#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Wire.h>

// LCD I2C (Address 0x27, 16 cols, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// LCD status
bool lcdAvailable = false;  // Auto-detected saat setup

// Display state
int displayMode = 0;       // 0=Sensor, 1=Status
unsigned long lastDisplayUpdate = 0;
const long displayInterval = 3000; // Rotate setiap 3 detik

// External variables
extern float ph, tds, temp;
extern Mode modeSekarang;
extern bool pompaUtamaRunning;
extern bool wifiConnected;

// ========================================
// SETUP LCD (Auto-detect)
// ========================================
void setupLCD() {
  // Try to detect LCD di I2C address 0x27
  Wire.begin();
  Wire.beginTransmission(0x27);
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    // LCD detected!
    lcdAvailable = true;
    lcd.init();
    lcd.backlight();
    
    // Splash screen
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" HydroMate v2.28");
    lcd.setCursor(0, 1);
    lcd.print(" Initializing...");
    delay(2000);
    lcd.clear();
  } else {
    // LCD tidak terpasang - silent mode (no spam error)
    lcdAvailable = false;
  }
}

// ========================================
// UPDATE LCD (Called from loop)
// ========================================
void updateLCD() {
  if (!lcdAvailable) return; // Skip jika LCD tidak ada
  
  unsigned long now = millis();
  
  // Rotate display setiap 3 detik
  if (now - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = now;
    displayMode = (displayMode + 1) % 2; // 2 modes: 0=Sensor, 1=Status
    
    // DEBUG: Print mode change
    Serial.print("🖥️  LCD Mode: ");
    Serial.println(displayMode == 0 ? "SENSOR" : "STATUS");
    
    lcd.clear();
    
    if (displayMode == 0) {
      displaySensorData();
    } else {
      displaySystemStatus();
    }
  }
}

// ========================================
// DISPLAY MODE 0: SENSOR DATA
// ========================================
void displaySensorData() {
  if (!lcdAvailable) return;
  
  // Line 1: pH | PPM
  lcd.setCursor(0, 0);
  lcd.print("pH:");
  lcd.print(ph, 1);
  
  lcd.setCursor(9, 0);
  lcd.print("PPM:");
  lcd.print((int)tds);
  
  // Line 2: Suhu | Mode
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temp, 1);
  lcd.print("C");
  
  lcd.setCursor(10, 1);
  if (modeSekarang == AUTO) {
    lcd.print("AUTO");
  } else {
    lcd.print("MANUAL");
  }
}

// ========================================
// DISPLAY MODE 1: SYSTEM STATUS
// ========================================
void displaySystemStatus() {
  if (!lcdAvailable) return;
  
  // Line 1: Pompa Status
  lcd.setCursor(0, 0);
  lcd.print("Pompa:");
  lcd.print(pompaUtamaRunning ? "ON " : "OFF");
  
  // Line 2: WiFi Status
  lcd.setCursor(0, 1);
  lcd.print("WiFi:");
  lcd.print(wifiConnected ? "OK" : "OFF");
}

// ========================================
// DISPLAY NOTIFICATION (Temporary)
// ========================================
void displayNotification(String line1, String line2, int duration) {
  if (!lcdAvailable) return;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  delay(duration);
  lcd.clear();
  
  // Reset timer agar langsung update setelah notif
  lastDisplayUpdate = millis() - displayInterval;
}

// ========================================
// DISPLAY ERROR
// ========================================
void displayError(String errorMsg) {
  if (!lcdAvailable) return;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ERROR!");
  lcd.setCursor(0, 1);
  
  if (errorMsg.length() > 16) {
    errorMsg = errorMsg.substring(0, 16);
  }
  lcd.print(errorMsg);
  delay(3000);
  lcd.clear();
}
