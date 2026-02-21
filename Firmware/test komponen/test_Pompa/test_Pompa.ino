// --- PIN MOSFET POMPA ---
#define POMPA_A 2
#define POMPA_B 4
#define POMPA_C 5
#define POMPA_D 18


void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  pinMode(POMPA_A, OUTPUT);
  pinMode(POMPA_B, OUTPUT);
  pinMode(POMPA_C, OUTPUT);
  pinMode(POMPA_D, OUTPUT);

  digitalWrite(POMPA_A, LOW);
  digitalWrite(POMPA_B, LOW);
  digitalWrite(POMPA_C, LOW);
  digitalWrite(POMPA_D, LOW);

  Serial.println("=== HydroMate ===");
  Serial.println("mode test pompa");

}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Menyalakan Pompa A");
  digitalWrite(POMPA_A, HIGH);
  delay(20000);
  Serial.println("Mematikan Pompa A");
  digitalWrite(POMPA_A, LOW);
  Serial.println("Menyalakan Pompa B");
  digitalWrite(POMPA_B, HIGH);
  delay(20000);
  Serial.println("Mematikan Pompa B");
  digitalWrite(POMPA_B, LOW);
  Serial.println("Menyalakan Pompa C");
  digitalWrite(POMPA_C, HIGH);
  delay(20000);
  Serial.println("Mematikan Pompa C");
  digitalWrite(POMPA_C, LOW);
  Serial.println("Menyalakan Pompa D");
  digitalWrite(POMPA_D, HIGH);
  delay(20000);
  Serial.println("Mematikan Pompa D");
  digitalWrite(POMPA_D, LOW);
  delay(20000);

  Serial.println("Hidupin smua Pompa");
  digitalWrite(POMPA_A, HIGH);
  digitalWrite(POMPA_B, HIGH);
  digitalWrite(POMPA_C, HIGH);
  digitalWrite(POMPA_D, HIGH);
  delay(20000);
  digitalWrite(POMPA_A, LOW);
  digitalWrite(POMPA_B, LOW);
  digitalWrite(POMPA_C, LOW);
  digitalWrite(POMPA_D, LOW);
  delay(20000);
}
