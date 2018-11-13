#include "BluetoothSerial.h"
#include "EEPROM.h"

bool ENABLED = false;                  // default to disabled.  let bluetooth turn it on
int  LIGHT_SENSOR_PIN = 34;            // sensing LED connected to IO34
int  MAGNET_PIN = 14;                  // pin that controls the magnet
int  LIGHT_THRESHOLD;                  // what threshold to hit before we trigger drop (default was 0)
int  LIGHT_THRESHOLD_ADDR = 0;         // where to store light threshold in eeprom
int  LIGHT_THRESHOLD_WAIT_MS;          // how long to wait at threshold before we trigger the drop, in milliseconds (default was 1500)
int  LIGHT_THRESHOLD_WAIT_MS_ADDR = 4; // where to store wait time in eeprom (need 4 because 4 bytes per int)
bool REALTIME_ENABLED = false;         // when true, prints measurements out to serial in realtime
bool PRINT_ENABLED = false;            // when true, prints measurement out to serial once
int  DROP_OVERRIDE = 0;                // controls dropping of the magnet, either manually or dark triggered
int  DROP_OVERRIDE_TIMEOUT = 2000;     // how long to be in a dropped state before turning magnet back on

unsigned long dark_detected_timestamp = 0; // when we started to detect dark
unsigned long drop_override_timestamp = 0; // when the drop was triggered 

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  Serial.println("Painting bottom dropper by kevinc");
  readStoredVars();
  printHelp();
  printVariables();
  SerialBT.begin("ExitPaint"); //Bluetooth device name

  pinMode(MAGNET_PIN, OUTPUT); 
}

void readStoredVars() {
  EEPROM.begin(64); // don't need a big size
  
  EEPROM.get(LIGHT_THRESHOLD_ADDR, LIGHT_THRESHOLD);
  EEPROM.get(LIGHT_THRESHOLD_WAIT_MS_ADDR, LIGHT_THRESHOLD_WAIT_MS);
}

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  enable      - turns light detection on");
  Serial.println("  drop        - triggers dropping the magnet manually");
  Serial.println("  threshold N - set threshold to be used to detect light");
  Serial.println("  wait N      - set time in milliseconds to wait while at or below threshold before triggering");
  Serial.println("  status      - prints the status of the device variables");
  Serial.println("  print       - print last detected light value");
  Serial.println("  realtime    - print out light values in realtime");
}

void printVariables() {
  Serial.println("");
  Serial.printf("Current Variables: \n");
  Serial.printf("  threshold:  %d\n", LIGHT_THRESHOLD);
  Serial.printf("  wait:       %d\n", LIGHT_THRESHOLD_WAIT_MS);
}

void handleMessage(String msg) {
  Serial.print("got '");
  Serial.print(msg);
  Serial.println("' command");

  String command = msg;
  int value = -1;

  // check if we need to split on space for advance commands
  for (int i = 0; i <= msg.length(); i++) {
      if (msg.charAt(i) == ' ') {         
          command = msg.substring(0, i);
          value = msg.substring(i+1, msg.length()).toInt();
      }
  }
 
  if (command == "enable") {
    Serial.println("enabling device to drop now...");
    ENABLED = true;
  }
  else if (command == "drop") {
    DROP_OVERRIDE = 2;
  }
  else if (command == "threshold") {
    Serial.printf("setting threshold to '%d'...\n", value);
    LIGHT_THRESHOLD = value;
    EEPROM.put(LIGHT_THRESHOLD_ADDR, value);
    EEPROM.commit();    
  }
  else if (command == "wait") {
    Serial.printf("setting wait time to '%d'...\n", value);
    LIGHT_THRESHOLD_WAIT_MS = value;
    EEPROM.put(LIGHT_THRESHOLD_WAIT_MS_ADDR, value);
    EEPROM.commit();    
  }
  else if (command == "status") {
    printVariables();
  }
  else if (command == "print") {
    PRINT_ENABLED = true;
  }  
  else if (command == "realtime") {
    REALTIME_ENABLED = !REALTIME_ENABLED; 
  } else {
    Serial.print("unknown command: ");
    Serial.println(command);
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

void resetState() {
  Serial.println("turning off drop override");
  DROP_OVERRIDE = 0;
  drop_override_timestamp = 0;
  ENABLED = false;
}

void loop() {
  // read bluetooth messages
  readAnyBluetoothMessage();

  // read serial messages
  readAnySerialMessage();

  // read value coming from light sensor
  int ls = analogRead(LIGHT_SENSOR_PIN);

  if (REALTIME_ENABLED || PRINT_ENABLED) {
    Serial.println(ls);
  }

  // print enabled is for a single recorded value
  if (PRINT_ENABLED) {
    PRINT_ENABLED = false;
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
      resetState();
    }
  } else {

    // only do light check logic if the device is enabled 
    if (ENABLED) {
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
  } 
 
  delay(50);
}
