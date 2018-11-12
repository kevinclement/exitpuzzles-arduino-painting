int sense = A6;        // sensing LED connected to analog5
int val   = 0;        // variable to store the value read from sense

void setup() {
  Serial.begin(115200);
  Serial.println("lightsense");
  Serial.println(sense);

  pinMode(14, OUTPUT); 
  // TODO: arguments to allow for dumping of sensing values
}

void loop() {
  // Serial.printf("ESP32 Chip ID = %04X",(uint16_t)(chipid>>32));//print High 2 bytes

  val = analogRead(sense);
  Serial.println(val);
  if (val == 0) {
    digitalWrite(14, LOW);   
  } else {
    digitalWrite(14, HIGH);   
    
  }
  

  delay(50);
}
