#include "BluetoothSerial.h"
#include "EEPROM.h"

bool ENABLED = false;                  // default to disabled.  let bluetooth turn it on
int  LIGHT_SENSOR_PIN = 34;            // sensing LED connected to IO34
int  MAGNET_PIN = 14;                  // pin that controls the magnet
int  LED_PIN = 33;                     // pin that controls the status LED
int  TOGGLE_BUTTON = 0;                // points to the built in boot button on esp32
int  LIGHT_THRESHOLD;                  // what threshold to hit before we trigger drop (default was 0)
int  LIGHT_THRESHOLD_ADDR = 0;         // where to store light threshold in eeprom
int  LIGHT_THRESHOLD_WAIT_MS;          // how long to wait at threshold before we trigger the drop, in milliseconds (default was 1500)
int  LIGHT_THRESHOLD_WAIT_MS_ADDR = 4; // where to store wait time in eeprom (need 4 because 4 bytes per int)
bool REALTIME_ENABLED = false;         // when true, prints measurements out to serial in realtime
int  MAGNET_OVERRIDE_STATE = 2;        // state of magnet, either on or off, or disabled
int  MAGNET_OVERRIDE_ADDR = 8;         // addr to store in eeprom
bool PRINT_ENABLED = false;            // when true, prints measurement out to serial once
char CRLF[] = "\r\n";

// Poor mans state machine
enum state { ST_INIT, ST_LIGHT_DETECTED, ST_DARK_DETECTED, ST_MAGNET_ON, ST_MAGNET_OFF };
enum state current_state = ST_INIT;
enum state prev_state = ST_INIT;

// offline logic
int  OFFLINE = 0;             // should we fallback to simple offline only logic.  anything other than 0 will fallback
int  OFFLINE_ADDR = 12;       // where to store offline in eeprom
bool OFFLINE_LED_ON = false;
bool updated_offline_mem = false;
unsigned long offline_pressed_timestamp = 0;

unsigned long dark_detected_timestamp = 0; // when we started to detect dark

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  Serial.println("Painting bottom dropper by kevinc");
  readStoredVars();
  printHelp();
  printVariables();
  SerialBT.begin("ExitPaint2"); //Bluetooth device name

  pinMode(MAGNET_PIN, OUTPUT); 
  pinMode(LED_PIN, OUTPUT);
  pinMode(TOGGLE_BUTTON, INPUT_PULLUP);
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
  EEPROM.get(OFFLINE_ADDR, OFFLINE);
}

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  enable         - turns light detection on");
  Serial.println("  disable        - turns light detection off");
  Serial.println("  magnet <0|1|2> - 0 turns off magnet, 1 turns on magnet, 2 disables override mode");
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
  p("  offline:    %d%s", OFFLINE, CRLF);
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

void handleStatusPin() {
  int buttonState = digitalRead(TOGGLE_BUTTON);

  if (buttonState == 0) {
    if (offline_pressed_timestamp == 0) {
      offline_pressed_timestamp = millis();
    }
    else if (millis() - offline_pressed_timestamp > 3000) { // hold time for button to reset mode
      OFFLINE_LED_ON = true;
      if (!updated_offline_mem) {
        OFFLINE = OFFLINE == 0 ? 1 : 0;

        EEPROM.put(OFFLINE_ADDR, OFFLINE);
        EEPROM.commit();
        updated_offline_mem = true; 
      }
    }
  } else {
    offline_pressed_timestamp = 0;
    OFFLINE_LED_ON = false;
    updated_offline_mem = false;
  }

  digitalWrite(LED_PIN, OFFLINE_LED_ON ? HIGH : LOW);
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

int readAndPrintLightSensor() {
  int ls = analogRead(LIGHT_SENSOR_PIN);

  if (REALTIME_ENABLED || PRINT_ENABLED) {
    p("%d%s", ls, CRLF);
  }

  // print enabled is for a single recorded value
  if (PRINT_ENABLED) {
    PRINT_ENABLED = false;
  }

  return ls;
}

void loop() {
  // read bluetooth messages
  readAnyBluetoothMessage();

  // read serial messages
  readAnySerialMessage();

  // set status pin
  handleStatusPin();

  // read value coming from light sensor
  int ls = readAndPrintLightSensor();
  
  // detect if light is below threshold
  if (ls <= LIGHT_THRESHOLD) {
    if (dark_detected_timestamp == 0) {
      dark_detected_timestamp = millis();
    }
            
    if (millis() - dark_detected_timestamp > LIGHT_THRESHOLD_WAIT_MS) {
      prev_state = current_state;

      // only change the state if the device is enabled
      if (ENABLED || OFFLINE) {
        current_state = ST_DARK_DETECTED;
      }
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

  // apply one time transition logic here
  if (prev_state != current_state) {
    // Serial.printf("transition: %d => %d\n", prev_state, current_state);

    switch(current_state) {
      case ST_LIGHT_DETECTED:
        p("Light detected.%s", CRLF);
        break;
      case ST_DARK_DETECTED:
        p("Dark detected.  Dropping now!%s", CRLF);
        break;
    }
  }

  // finally do work based on current state
  switch(current_state)  {
    case ST_DARK_DETECTED:
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
