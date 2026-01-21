#include <Wire.h>
#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DFRobot_ADS1115.h>
#include <GravityTDS.h>

// ADS1115
DFRobot_ADS1115 ads(&Wire);

// Gravity TDS
GravityTDS gravityTds;

// DS18B20
#define ONE_WIRE_BUS 25
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// Preferences (flash)
Preferences prefs;

// Pin ADS channel
const uint8_t TDS_ADC_CHANNEL = 0; 

float temperatureC = 25.0;
float calibFactor   = 1.0;  // kalo library pake offset/buffer nanti kita scale

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // init ADS1115
  ads.init();
  ads.setAddr_ADS1115(0x48);
  ads.setMode(eADS_MODE_SINGLE);
  ads.setRate(eRATE_128);
  ads.setPga(eADS_PGA_4_096V);

  // init DS18B20
  ds18b20.begin();

  // init TDS
  gravityTds.begin();

  // load kalibrasi
  prefs.begin("tds", false);
  calibFactor = prefs.getFloat("factor", 1.0);

  Serial.println("READY. ketik CAL:<ppm>");
  Serial.print("calib: "); Serial.println(calibFactor, 6);
}

void loop(){
  // suhu
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);
  if (t > -40 && t < 125) temperatureC = t;

  // baca ADS1115
  int16_t raw = ads.readADC_SingleEnded(TDS_ADC_CHANNEL);
  float volts = raw * (4.096 / 32768.0);

  // set ke library
  gravityTds.setVoltage(volts);
  gravityTds.setTemperature(temperatureC);

  gravityTds.update();
  float tds = gravityTds.getTdsValue();
  tds *= calibFactor;

  Serial.print("T:"); Serial.print(temperatureC,1);
  Serial.print("C V:"); Serial.print(volts,4);
  Serial.print(" PPM:"); Serial.println(tds,1);

  // parsing kalibrasi
  if(Serial.available()){
    String s = Serial.readStringUntil('\n');
    s.trim();
    if(s.startsWith("CAL:")){
      float refppm = s.substring(4).toFloat();
      if(refppm > 0 && tds > 0){
        calibFactor = refppm / tds;
        prefs.putFloat("factor", calibFactor);
        Serial.print("NEW CAL: "); Serial.println(calibFactor,6);
      }
    }
  }

  delay(1000);
}
