#include "BluetoothSerial.h"

int  LIGHT_SENSOR_PIN = 34;           // sensing LED connected to IO34
int  MAGNET_PIN = 14;                 // pin that controls the magnet
int  LIGHT_THRESHOLD = 0;             // what threshold to hit before we trigger drop
int  LIGHT_THRESHOLD_WAIT_MS = 1500;  // how long to wait at threshold before we trigger the drop, in milliseconds
bool REALTIME_ENABLED = false;
int  DROP_OVERRIDE = 0;
int  DROP_OVERRIDE_TIMEOUT = 2000;

unsigned long dark_detected_timestamp = 0;                   // when we started to detect dark
unsigned long drop_override_timestamp = 0;  
int DROP = LOW;
bool dropped = false;

BluetoothSerial SerialBT;

// TODO: 
//   [ ] commands
//     [x] drop - drops key
//     [ ] threshold n - sets threshold to go below to trigger
//     [ ] wait n - sets how long to wait below threshold before triggering
//     [ ] print - print current reading from sensor
//     [x] realtime - print current reading out on console in realtime
//   [x] implement threshold before dropping
//   [ ] implement eeprom usage for storing threshold and wait
//   [x] after triggering, wait some ammount of time (2s) and then turn back on

void setup() {
  Serial.begin(115200);
  Serial.println("Painting bottom dropper by kevinc");
  printHelp();
  SerialBT.begin("ExitPaint"); //Bluetooth device name

  pinMode(MAGNET_PIN, OUTPUT); 
}

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  drop        - triggers dropping the magnet manually");
  Serial.println("  threshold N - set threshold to be used to detect light");
  Serial.println("  wait N      - set time in milliseconds to wait while at or below threshold before triggering");
  Serial.println("  print       - print last detected light value");
  Serial.println("  realtime    - print out light values in realtime");
}

void handleMessage(String msg) {
  Serial.print("got ");
  Serial.print(msg);
  Serial.println(" command");
  
  if (msg == "enable") {
  }
  else if (msg == "realtime") {
    REALTIME_ENABLED = !REALTIME_ENABLED;
  }
  else if (msg == "drop") {
    DROP_OVERRIDE = 2;
  } else {
    Serial.print("unknown command: ");
    Serial.println(msg);
  }
}

void readAnyBluetoothMessage() {
  if (!SerialBT.available()) {
    return;
  }

  // read and handle message from bluetooth
  String str = SerialBT.readStringUntil('\n');
  handleMessage(str);
}

void readAnySerialMessage() {
  if (!Serial.available()) {
    return;
  }

  // read and handle message from serial
  String str = Serial.readStringUntil('\n');
  handleMessage(str);
  
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

  if (REALTIME_ENABLED) {
    Serial.println(ls);
  }

  // Check of drop override is in place
  if (DROP_OVERRIDE > 0) {
    digitalWrite(MAGNET_PIN, LOW);

    if (drop_override_timestamp == 0) {
      if (DROP_OVERRIDE == 1) {
        Serial.println("Dark detected.  Dropping now!");
      } else if (DROP_OVERRIDE == 2) {
        Serial.println("Dark override.  Dropping now!");
      }
      drop_override_timestamp = millis();
    } else if (millis() - drop_override_timestamp > DROP_OVERRIDE_TIMEOUT) {
      Serial.println("turning off drop override");
      DROP_OVERRIDE = 0;
      drop_override_timestamp = 0;
    }
  } else {
    if (ls <= LIGHT_THRESHOLD) {
      if (dark_detected_timestamp == 0) {
        dark_detected_timestamp = millis();
      }
      
      if (millis() - dark_detected_timestamp > LIGHT_THRESHOLD_WAIT_MS) {
          DROP_OVERRIDE = 1;
      } 
    } else {
      dark_detected_timestamp = 0;
      digitalWrite(MAGNET_PIN, HIGH);
    }
  } 
 
  delay(50);
}
