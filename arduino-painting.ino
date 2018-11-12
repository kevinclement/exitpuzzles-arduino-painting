#include "BluetoothSerial.h"

int LIGHT_SENSOR_PIN = 34;               // sensing LED connected to IO34
int MAGNET_PIN = 14;
int LIGHT_THRESHOLD = 0;             // what threshold to hit before we trigger drop
int LIGHT_THRESHOLD_WAIT_MS = 1500;  // how long to wait at threshold before we trigger the drop, in milliseconds

unsigned long darkOn = 0; // when we started to detect dark
bool dropped = false;

BluetoothSerial SerialBT;

// TODO: 
//   [ ] commands
//     [ ] drop - drops key
//     [ ] threshold n - sets threshold to go below to trigger
//     [ ] wait n - sets how long to wait below threshold before triggering
//     [ ] print - print current reading from sensor
//     [ ] realtime - print current reading out on console in realtime
//   [ ] implement threshold before dropping
//   [ ] implement eeprom usage for storing threshold and wait
//   [ ] after triggering, wait some ammount of time (2s) and then turn back on

void setup() {
  Serial.begin(115200);
  Serial.println("Painting bottom dropper by kevinc");
  SerialBT.begin("ExitPaint"); //Bluetooth device name

  pinMode(MAGNET_PIN, OUTPUT); 
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

  // read value coming from light sensor
  int ls = analogRead(LIGHT_SENSOR_PIN);

  Serial.println(ls);  
  
  if (ls <= LIGHT_THRESHOLD) {
    if (darkOn == 0) {
      darkOn = millis();
    } else if (millis() - darkOn > LIGHT_THRESHOLD_WAIT_MS) {
      if (!dropped) {
        Serial.println("Dark passed threshold.  Dropping now!");
        dropped = true;
      }
      
      digitalWrite(MAGNET_PIN, LOW);
    } else {
      digitalWrite(MAGNET_PIN, HIGH);
    }
  } else {
    darkOn = 0;
    dropped = false;
    digitalWrite(MAGNET_PIN, HIGH);
  }
 
  delay(50);
}
