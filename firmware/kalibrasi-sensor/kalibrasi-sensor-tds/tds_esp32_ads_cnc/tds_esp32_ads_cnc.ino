/*
  ESP32 + ADS1115 — TDS Meter
  - I2C default ESP32: SDA=21, SCL=22
  - ADS1115 addr = 0x48 
  - Library yang dipakai:
      Adafruit ADS1X15
  Catatan kalibrasi:
    - Set K_CELL sesuai probe (umum ~1.0). Kalibrasi dengan larutan TDS/EC.
    - TDS_FACTOR 0.5–0.7 (meter komersial sering ~0.5).
*/

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// ===== ADS1115 =====
Adafruit_ADS1115 ads; 

// ===== Parameter Sensor & Konversi =====
const float TDS_FACTOR   = 0.5;   // set ini agar mendekati meter komersial (0.5..0.7) – tweak saat kalibrasi
const float K_CELL       = 1.00;  // cell constant (~1.0), tweak saat kalibrasi EC
float waterTempC         = 25.0;  // ganti dengan pembacaan suhu aktual kalau ada

// Sampling/Filter
const uint16_t NUM_SAMPLES     = 32;    // sample untuk dirata-rata
const uint16_t SAMPLE_DELAY_US = 800;   // jeda antar-sample (microseconds)

// ===== Baca rata-rata tegangan dari ADS1115 channel A0 (single-ended) =====
float readADS_Volts_A0() {
  long acc = 0;
  for (uint16_t i = 0; i < NUM_SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(0); // A0
    acc += raw;
    delayMicroseconds(SAMPLE_DELAY_US);
  }
  float avgRaw = (float)acc / NUM_SAMPLES;
  return ads.computeVolts((int16_t)avgRaw); // volt
}

// ===== Konversi tegangan -> EC -> TDS (ppm) =====
/*
  Rumus polinomial umum (DFRobot-style) untuk tegangan (Vref 3.3–5V ke range probe):
    ec (mS/cm) = (133.42*V^3 - 255.86*V^2 + 857.39*V) * K_CELL
  Lalu kompensasi suhu ke 25°C:
    ec25 = ec / (1 + 0.02*(T-25))
  TDS (ppm) = ec25 * TDS_FACTOR * 1000
*/
float voltageToTDS(float v, float tempC) {
  if (v < 0) v = 0;
  // batasi & amankan kisaran
  if (v > 3.3) v = 3.3;

  float ec = (133.42f * v * v * v - 255.86f * v * v + 857.39f * v); // mS/cm
  ec *= K_CELL;

  // kompensasi suhu (linear 2%/°C)
  float ec25 = ec / (1.0f + 0.02f * (tempC - 25.0f));

  float tds = ec25 * TDS_FACTOR * 1000.0f; // ppm
  if (tds < 0) tds = 0;
  return tds;
}

void setup() {
  Wire.begin();             
  Serial.begin(115200);

  Serial.println("Init...");

  // ADS1115
  if (!ads.begin()) {
    Serial.println("ADS1115 not found!");
    while (1) delay(10);
  }
  // Gain = ±4.096V (1 bit = 125uV). Probe TDS umumnya < 2V, aman.
  ads.setGain(GAIN_ONE);

  Serial.println("Init ADS1115... OK");
  delay(600);
}

void loop() {
  // 1) baca tegangan rata-rata dari ADS
  float v = readADS_Volts_A0();

  // 2) konversi ke PPM (TDS)
  float ppm = voltageToTDS(v, waterTempC);

  // 4) kirim ke Serial
  Serial.print("V:"); Serial.println(v, 4);
  Serial.print(",PPM:"); Serial.println(ppm, 1);
  Serial.print(",Temp:"); Serial.println(waterTempC, 1);

  delay(500);
}