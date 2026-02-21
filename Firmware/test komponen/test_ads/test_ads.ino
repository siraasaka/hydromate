#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define ONE_WIRE_BUS 25
#define TDS_CHANNEL 0

#define EEPROM_SIZE 32
#define EEPROM_ADDR 0   // simpan calibrationFactor

Adafruit_ADS1115 ads;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

float calibrationFactor = 1.0;

// ===== TABEL PPM vs SUHU (LARUTAN KALIBRASI) =====
float getTargetPPM(float temp) {
  if (temp <= 20) return 1251;
  if (temp <= 21) return 1277;
  if (temp <= 22) return 1303;
  if (temp <= 23) return 1339;
  if (temp <= 24) return 1356;
  if (temp <= 25) return 1382;
  if (temp <= 26) return 1408;
  if (temp <= 27) return 1436;
  if (temp <= 28) return 1461;
  if (temp <= 29) return 1476;
  return 1515; // >=30
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  ads.begin(0x48);
  ads.setGain(GAIN_ONE);

  tempSensor.begin();

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, calibrationFactor);
  if (isnan(calibrationFactor) || calibrationFactor <= 0) {
    calibrationFactor = 1.0;
  }

  Serial.println("SYSTEM READY");
}

float readTDS(float temperature) {
  int16_t raw = ads.readADC_SingleEnded(TDS_CHANNEL);
  float voltage = raw * 0.1875 / 1000.0;

  float compCoeff = 1.0 + 0.02 * (temperature - 25.0);
  float compVoltage = voltage / compCoeff;

  float ec = (133.42 * pow(compVoltage, 3)
            - 255.86 * pow(compVoltage, 2)
            + 857.39 * compVoltage) * 0.5;

  return (ec * 0.5) * calibrationFactor;
}

void loop() {
  tempSensor.requestTemperatures();
  float temperature = tempSensor.getTempCByIndex(0);

  float tds = readTDS(temperature);
  float targetPPM = getTargetPPM(temperature);

  Serial.print("Suhu: ");
  Serial.print(temperature, 2);
  Serial.print(" C | TDS: ");
  Serial.print(tds, 0);
  Serial.print(" ppm | Target Kalibrasi: ");
  Serial.print(targetPPM);
  Serial.println(" ppm");

  // ===== AUTO KALIBRASI (ketik 'c' di Serial Monitor) =====
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'c') {
      calibrationFactor = targetPPM / tds;
      EEPROM.put(EEPROM_ADDR, calibrationFactor);
      EEPROM.commit();

      Serial.print("KALIBRASI OK | Faktor: ");
      Serial.println(calibrationFactor, 4);
    }
  }

  delay(1000);
}
