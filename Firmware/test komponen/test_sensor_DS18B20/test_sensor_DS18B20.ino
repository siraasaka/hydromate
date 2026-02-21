#include <OneWire.h>
#include <DallasTemperature.h>

// Tentukan pin GPIO tempat kabel data sensor terhubung
const int oneWireBus = 27;     

// Inisialisasi OneWire untuk berkomunikasi dengan sensor
OneWire oneWire(oneWireBus);

// Berikan referensi OneWire ke DallasTemperature
DallasTemperature sensors(&oneWire);

void setup(void) {
  // Mulai komunikasi serial untuk menampilkan hasil
  Serial.begin(115200);
  
  // Mulai pustaka DallasTemperature
  sensors.begin();
  
  Serial.println("Membaca suhu dari sensor DS18B20...");
}

void loop(void) { 
  // Perintahkan semua sensor di bus untuk melakukan konversi suhu
  sensors.requestTemperatures(); 
  
  // Baca suhu dalam Celcius
  float temperatureC = sensors.getTempCByIndex(0);
  
  // Baca suhu dalam Fahrenheit
  float temperatureF = sensors.getTempFByIndex(0);

  // Cek jika data suhu valid
  if (temperatureC != DEVICE_DISCONNECTED_C) {
    Serial.print("Suhu: ");
    Serial.print(temperatureC);
    Serial.print(" °C");
    Serial.print("  |  ");
    Serial.print(temperatureF);
    Serial.println(" °F");
  } else {
    Serial.println("Gagal membaca suhu dari sensor!");
  }
  
  // Tunggu 2 detik sebelum pembacaan berikutnya
  delay(2000);
}