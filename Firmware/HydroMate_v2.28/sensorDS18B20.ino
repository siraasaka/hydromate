#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 27   

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

void setupDS18B20() {
  ds18b20.begin();
}

float bacaSuhu() {
  ds18b20.requestTemperatures();
  delay(100); // Tunggu konversi selesai (DS18B20 butuh ~94ms untuk 12-bit)
  
  float temp = ds18b20.getTempCByIndex(0);
  
  // RETRY jika invalid (1x retry)
  if (temp < -55 || temp > 125 || isnan(temp)) {
    delay(50);
    ds18b20.requestTemperatures();
    delay(100);
    temp = ds18b20.getTempCByIndex(0);
    
    if (temp >= -55 && temp <= 125 && !isnan(temp)) {
    }
  }
  
  static bool warningShown = false;
  
  // FAILSAFE setelah retry
  if (temp < -55 || temp > 125 || isnan(temp)) {
    if (!warningShown) {
      warningShown = true;
    }
    return 25.0;
  }
  
  // Reset warning jika sensor OK
  if (warningShown) {
    warningShown = false;
  }
  
  return temp;
}