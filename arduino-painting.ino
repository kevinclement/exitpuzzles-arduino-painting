#include "BluetoothSerial.h"

int sense = A6;       // sensing LED connected to analog5
int val   = 0;        // variable to store the value read from sense

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  Serial.println("Painting bottom dropper by kevinc");
  SerialBT.begin("ExitPaint"); //Bluetooth device name

  pinMode(14, OUTPUT); 
}


void readAnyBluetoothMessage() { 
  if (!SerialBT.available()) {
    return;
  }

  String str = SerialBT.readStringUntil('\n');

  if (str == "enable") {
    Serial.println("got enabled command");
  }
  else if (str == "disable") {
    Serial.println("got disabled command");
  } else {
    Serial.print("unknown command: ");
    Serial.println(str);
  }
}

void readAnySerialMessage() {
  if (!Serial.available()) {
    return;
  }

  String str = Serial.readStringUntil('\n');
  Serial.print("Serial command: ");
  Serial.println(str);

  // send it to bluetooth device
  SerialBT.print(str);
}

void loop() {
  // read bluetooth messages
  readAnyBluetoothMessage();

  // read serial messages
  readAnySerialMessage();

//  val = analogRead(sense);
//  Serial.println(val);
//  if (val == 0) {
//    digitalWrite(14, LOW);   
//  } else {
//    digitalWrite(14, HIGH);   
//    
//  }
  
  delay(50);
}
