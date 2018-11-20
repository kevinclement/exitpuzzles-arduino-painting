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
int  MAGNET_OVERRIDE_STATE = 2;        // state of magnet, either on or off, or disabled
int  MAGNET_OVERRIDE_ADDR = 8;         // addr to store in eeprom
bool PRINT_ENABLED = false;            // when true, prints measurement out to serial once
bool DROP_OVERRIDE = false;            // if drop override was requested
int  DROP_OVERRIDE_TIMEOUT = 2000;     // how long to be in a dropped state before turning magnet back on
char CRLF[] = "\r\n";

// Poor mans state machine
enum state { ST_INIT, ST_LIGHT_DETECTED, ST_DARK_DETECTED, ST_DROP_REQ, ST_MAGNET_ON, ST_MAGNET_OFF };
enum state current_state = ST_INIT;
enum state prev_state = ST_INIT;

unsigned long dark_detected_timestamp = 0; // when we started to detect dark
unsigned long drop_override_timestamp = 0; // when the drop was triggered 

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  Serial.println("Painting bottom dropper by kevinc");
  readStoredVars();
  printHelp();
  printVariables();
  SerialBT.begin("ExitPaint2"); //Bluetooth device name

  pinMode(MAGNET_PIN, OUTPUT); 
}

void p(char *fmt, ...){
    char buf[128];     // resulting string limited to 128 chars
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 128, fmt, args);
    va_end(args);

    // print to serial
    Serial.print(buf);

    // print to bluetooth if available
    SerialBT.print(buf);
}

void readStoredVars() {
  EEPROM.begin(64); // don't need a big size
  
  EEPROM.get(LIGHT_THRESHOLD_ADDR, LIGHT_THRESHOLD);
  EEPROM.get(LIGHT_THRESHOLD_WAIT_MS_ADDR, LIGHT_THRESHOLD_WAIT_MS);
  EEPROM.get(MAGNET_OVERRIDE_ADDR, MAGNET_OVERRIDE_STATE);
}

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  enable         - turns light detection on");
  Serial.println("  disable        - turns light detection off");
  Serial.println("  magnet <0|1|2> - 0 turns off magnet, 1 turns on magnet, 2 disables override mode");
  Serial.println("  drop           - triggers dropping the magnet manually");
  Serial.println("  threshold N    - set threshold to be used to detect light");
  Serial.println("  wait N         - set time in milliseconds to wait while at or below threshold before triggering");
  Serial.println("  status         - prints the status of the device variables");
  Serial.println("  print          - print last detected light value");
  Serial.println("  realtime       - print out light values in realtime");
}

void printVariables() { 
  p(CRLF);   
  p("Current Variables:%s", CRLF);
  p("  threshold:  %d%s", LIGHT_THRESHOLD, CRLF);
  p("  wait:       %d%s", LIGHT_THRESHOLD_WAIT_MS, CRLF);
  p("  magnet:     %d%s", MAGNET_OVERRIDE_STATE, CRLF);
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
    p("enabling device to drop now...%s", CRLF);
    ENABLED = true;
  }
  else if (command == "disable") {
    p("disabling device now...%s", CRLF);
    ENABLED = false;
  }
  else if (command == "magnet") {
    char * state = "disabled";
    if (value == 0) {
      state = "off";
    } else if (value == 1) {
      state = "on";
    }

    p("turning magnet override %s...%s", state, CRLF);
    MAGNET_OVERRIDE_STATE = value;
    EEPROM.put(MAGNET_OVERRIDE_ADDR, value);
    EEPROM.commit();
  }
  else if (command == "drop") {
    DROP_OVERRIDE = true;
  }
  else if (command == "threshold") {
    p("setting threshold to '%d'...%s", value, CRLF);
    LIGHT_THRESHOLD = value;
    EEPROM.put(LIGHT_THRESHOLD_ADDR, value);
    EEPROM.commit();    
  }
  else if (command == "wait") {
    p("setting wait time to '%d'...%s", value, CRLF);
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
    int str_len = command.length() + 1; 
    char char_array[str_len];
    command.toCharArray(char_array, str_len);
    p("unknown command: %s%s", char_array, CRLF);
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
}

void loop() {
  // read bluetooth messages
  readAnyBluetoothMessage();

  // read serial messages
  readAnySerialMessage();

  // read value coming from light sensor
  int ls = analogRead(LIGHT_SENSOR_PIN);

  if (REALTIME_ENABLED || PRINT_ENABLED) {
    p("%d%s", ls, CRLF);
  }

  // print enabled is for a single recorded value
  if (PRINT_ENABLED) {
    PRINT_ENABLED = false;
  }

  // detect if light is below threshold
  if (ls <= LIGHT_THRESHOLD) {
    if (dark_detected_timestamp == 0) {
      dark_detected_timestamp = millis();
    }
            
    if (millis() - dark_detected_timestamp > LIGHT_THRESHOLD_WAIT_MS) {
      prev_state = current_state;
      current_state = ST_DARK_DETECTED;
    }
  } else {
    prev_state = current_state;
    current_state = ST_LIGHT_DETECTED;
    dark_detected_timestamp = 0;
  }

  // apply any overrides to the state
  if (MAGNET_OVERRIDE_STATE == 0 || MAGNET_OVERRIDE_STATE == 1) {
    prev_state = current_state = MAGNET_OVERRIDE_STATE == 0 ? ST_MAGNET_OFF : ST_MAGNET_ON;
  }

  // drop override has highest precedent over others, so it goes last and overrides all the current states 
  if (DROP_OVERRIDE) {
    prev_state = current_state = ST_DROP_REQ;
  }

  // apply one time transition logic here
  if (prev_state != current_state) {
    // Serial.printf("transition: %d => %d\n", prev_state, current_state);

    if (current_state == ST_LIGHT_DETECTED) {
      p("Light detected.%s", CRLF);
    }

    if (current_state == ST_DARK_DETECTED) {
        p("Dark detected.  Dropping now!%s", CRLF);
    } else if (current_state == ST_DROP_REQ) {
        p("Dark override.  Dropping now!%s", CRLF);
    }

    if (prev_state == ST_DARK_DETECTED) {
      if (millis() - drop_override_timestamp > DROP_OVERRIDE_TIMEOUT) {
        p("turning off drop%s", CRLF);  
        drop_override_timestamp = 0;
        DROP_OVERRIDE = false;
       }
    }
  }

  // finally do work based on current state
  switch(current_state)  {
    case ST_DARK_DETECTED:
    case ST_DROP_REQ: 
      digitalWrite(MAGNET_PIN, LOW);

      if (drop_override_timestamp == 0) {
        drop_override_timestamp = millis();
      } 
      break;

    case ST_MAGNET_OFF:
      digitalWrite(MAGNET_PIN, LOW);
      break;

    case ST_MAGNET_ON:
    case ST_LIGHT_DETECTED:
      digitalWrite(MAGNET_PIN, HIGH);
      break;
  } 
    
  delay(100);
}
