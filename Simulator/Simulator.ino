/** Simulator.ino
  * Last modified 8/24/23
  * Simulates IR sensors and Serial communication
  * When updating Sorter code, ensure PINS 0,1, and reset are unplugged

  * USER INPUT KEY

  * Send RFID data - A[1/2/3]:[Data]
  * Check active antenna: QA
  * Send Python data - P:[Data]
  * Set IR sensor - IR[1/2/3][L(low)/H(high)]
  * Check active IR states: QIR
  * Setup new experiment: SETUP
  *     Follow prompts
  * Start session for a given mouse: ENTER
  *     Follow prompts and note that mouse to enter has range 0:(numMice-1)
  * Reset Sorter - RESET
  * End user mode - QUIT

**/
unsigned long timestamp;
#define MS_PER_HR 1000 * 60 * 60
#define MS_PER_MIN 10000 * 60
#define MS_PER_S 1000
unsigned long hour;
unsigned long min;
unsigned long sec;
unsigned long msec;

#include <SoftwareSerial.h>
#include <elapsedMillis.h>

// IR Sensor Pins
#define IR1 11
#define IR2 12
#define IR3 13

// Reset connection
#define RESET 7

// Relay inputs
#define Relay1 8
#define Relay2 9
#define Relay3 10

// Gate inputs and constants
enum Gate { Gate1 = 5,
            Gate2 = 6 };

// Sorter 2
#define GateOpen 1250
#define GateClosed 1550

// Serial communications
// Make sure both Arduinos share a common ground, and that the USB on Sorter is never
// plugged in at the same time as Pins 0/1
#define Logger Serial
#define Python Serial1  //RX 19 Tx 20
#define RFID Serial2    // RX 21 TX 22
#define NULL_REPLY ""
#define ERR_REPLY "ERR"


// State variables
enum Antenna { NoAntenna,
               Antenna1,
               Antenna2,
               Antenna3,
               AntennaErr = 5 };

String messageToWait = "";
int waitTime = 1000;

// Experimental variables
int numMice = 4;

// Mouse variables
int mouse = 0;
String mouseID[4] = { "000000000000", "111111111111", "222222222222", "333333333333" };
String mouseRFID[4] = { "0000000000000000", "1111111111111111", "2222222222222222", "3333333333333333" };

// Setup
void setup() {

  // Serial setups
  Logger.begin(9600);
  Logger.setTimeout(100);

  Python.begin(9600);
  Python.setTimeout(100);

  RFID.begin(9600);
  RFID.setTimeout(100);

  Logger.readString();
  Python.readString();
  RFID.readString();


  // IR sensor setups
  pinMode(IR1, OUTPUT);
  pinMode(IR2, OUTPUT);
  pinMode(IR3, OUTPUT);

  pinMode(Relay1, INPUT);
  pinMode(Relay2, INPUT);
  pinMode(Relay3, INPUT);

  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);

  // Startup state
  defaultState();
}

void loop() {
  int i = 0;
  setupExperiment(4, 10000);
  while (1) {
    Logger.print("i =");
    Logger.println(i);
    startSession(4, i);
    MinTimeCheck(30000);
    MaxTimeCheck(30000);
    mouseExit(mouse);
    i++;
    i %= 4;
  }
  Logger.println("All done");
}

void userInput() {
  Logger.println("\nSTART USER INPUT\n");

  while (1) {

    if (Python.available() > 0) {
      readPython();
    }

    if (RFID.available() > 0) {
      readRFID();
    }

    if (Logger.available() > 0) {
      String input = Logger.readString();
      if (input.startsWith("P:")) {
        writePython(input.substring(2, input.length()));
      } else if (input.startsWith("A")) {
        Antenna antenna;
        switch (input.substring(1, 2).toInt()) {
          case 1:
            antenna = Antenna1;
            break;
          case 2:
            antenna = Antenna2;
            break;
          case 3:
            antenna = Antenna3;
            break;
        }
        writeRFID(input.substring(3, input.length()), antenna);
      } else if (input.startsWith("IR")) {
        uint8_t state;
        if (input.substring(3, 4) == "L") {
          state = LOW;
        } else {
          state = HIGH;
        }


        int irSensor;
        switch (input.substring(2, 3).toInt()) {
          case 1:
            irSensor = IR1;
            break;
          case 2:
            irSensor = IR2;
            break;
          case 3:
            irSensor = IR3;
        }
        Logger.println("Pin " + String(irSensor) + ": " + String(state));
        digitalWrite(irSensor, state);
      } else if (input == "SETUP") {
        Logger.println("PREPPING SETUP:");
        Logger.print("Num mice: ");
        while (Logger.available() == 0) {}
        int desiredNumMice = Logger.readString().toInt();
        Logger.println(desiredNumMice);
        Logger.print("Entries: ");
        while (Logger.available() == 0) {}
        int entries = Logger.readString().toInt();
        Logger.println(entries);
        setupExperiment(desiredNumMice, entries);
      } else if (input == "ENTER") {
        Logger.println("PREPPING ENTRY");
        Logger.print("Mouse to enter: ");
        while (Logger.available() == 0) {}
        int entering = Logger.readString().toInt();
        Logger.println(entering);
        startSession(numMice, entering);
      } else if (input == "RESET") {
        reset();
      } else if (input == "QA") {
        Logger.println("Active Antenna: " + String(activeAntenna()));
      } else if (input == "QIR") {
        Logger.println("IR1: " + String(digitalRead(IR1)));
        Logger.println("IR2: " + String(digitalRead(IR2)));
        Logger.println("IR3: " + String(digitalRead(IR3)));
      } else if (input == "QUIT") {
        Logger.println("END USER INPUT");
        return;
      } else if (input == "MIN") {
        Logger.println("Min Time Check");
        MinTimeCheck(1000);
      } else if (input == "MAX") {
        Logger.println("Max Time Check");
        MaxTimeCheck(1000);
      } else if (input == "EXIT") {
        Logger.println("Starting Exit");
        Logger.println("Mouse to exit: ");
        while (Logger.available() == 0) {}
        int activeMouse = Logger.readString().toInt();
        Logger.println(activeMouse);
        mouseExit(activeMouse);
      } else if (input == "YOLO") {
        setupExperiment(3, 10);
        startSession(3, 0);
        delay(20000);
        MinTimeCheck(1000);
        delay(20000);
        MaxTimeCheck(1000);
        mouseExit(mouse);
      } else if (input == "LONGTIME") {
        int i = 0;
        setupExperiment(4, 10000);
        while (1) {
          startSession(4, i);
          MinTimeCheck(30000);
          MaxTimeCheck(30000);
          mouseExit(mouse);
          i++;
          i %= 4;
        }
      } else if (input == "QG") {
        Logger.print("G1: ");
        Logger.println(pulseIn(Gate1, HIGH, 100000));
        Logger.print("G2: ");
        Logger.println(pulseIn(Gate2, HIGH, 100000));
        Logger.println();
      }
    }
  }
}

// Default state: all IR sensors beams not broken and no antenna selected
void defaultState() {
  digitalWrite(IR1, HIGH);
  digitalWrite(IR2, HIGH);
  digitalWrite(IR3, HIGH);
}


// Returns which antenna is current selected
int activeAntenna() {
  switch (1 * digitalRead(Relay1) + 2 * digitalRead(Relay2) + 4 * digitalRead(Relay3)) {
    case 0:
      return NoAntenna;
    case 1:
      return Antenna1;
    case 2:
      return Antenna2;
    case 4:
      return Antenna3;
    default:
      return AntennaErr;
  }
}

/** Logger explanation
 *
 * By using read/write Python and read/write RFID functions, all information communicated
 * via serial is written to computer attached to simulator by USB (AKA Logger)
*/
String readPython() {
  TimeStamp();
  String message = Python.readString();
  Logger.print("PYTHON, IN, ");
  Logger.print(message);
  Logger.println("[END]\n");
  return message;
}

void writePython(String message) {
  TimeStamp();
  Python.print(message);
  Logger.print("PYTHON, OUT, ");
  Logger.print(message);
  Logger.println("[END]\n");
}

String readRFID() {
  int active = activeAntenna();
  // int active = activeAntenna();
  TimeStamp();
  if (active == Antenna1 || active == Antenna2 || active == Antenna3) {
    String message = RFID.readString();
    Logger.print("RFID");
    Logger.print(active);
    Logger.print(", IN, ");
    Logger.print(message);
    Logger.println("[END]\n");
    return message;
  } else if (active == NoAntenna) {
    Logger.print("RFID");
    Logger.print(active);
    //NSNR - None Selected Not Recieved
    Logger.print(", IN, NSNR:");
    Logger.println(NULL_REPLY + String("\n"));
    return (NULL_REPLY);
  } else if (active == AntennaErr) {
    Logger.print("RFID");
    Logger.print(active);
    //SENR - Selection Error Not Recieved
    Logger.print(", OUT, SENR:");
    Logger.println(ERR_REPLY + String("\n"));
    return ERR_REPLY;
  }
}

void writeRFID(String message, Antenna antenna) {
  TimeStamp();
  int active = activeAntenna();
  if (active == antenna) {
    RFID.print(message);
    Logger.print("RFID");
    Logger.print(antenna);
    Logger.print(", OUT, ");
    Logger.println(message + "\n");
  } else if (active == NoAntenna || antenna == NoAntenna) {
    RFID.print(message);
    Logger.print("RFID");
    Logger.print(antenna);
    //NSNR - None Selected Not Recieved
    Logger.print(", OUT, NSNR:");
    Logger.println(message + "\n");
  } else if (active == AntennaErr || antenna == AntennaErr) {
    RFID.print(message);
    Logger.print("RFID");
    Logger.print(antenna);
    //NSNR - Selection Error Not Recieved
    Logger.print(", OUT, SENR:");
    Logger.println(message + "\n");
  }
}

void waitForPython(String message, int timeout = 3000) {
  elapsedMillis timeoutTimer;
  String buffer = "";
  char byteBuffer[1];
  bool timedOut = false;
  TimeStamp();
  Logger.print("TIMER, PYTHON, ");
  Logger.print("[");
  Logger.print(timeout);
  Logger.print("], ");
  Logger.println(message);
  while (buffer.indexOf(message) == -1 && !timedOut) {
    if (Python.available() > 0) {
      Python.readBytes(byteBuffer, 1);
      buffer.concat(byteBuffer[0]);
    }
    if (timeoutTimer > timeout) {
      timedOut = true;
    }
    if (Logger.available() > 0) {
      if (Logger.readString().indexOf("EXIT") != -1) {
        timedOut = true;
      }
    }
  }
  if (timedOut) {
    TimeStamp();
    Logger.print("TIMER, PYTHON, FAIL [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(buffer);
    Logger.println("[END]\n");
    bool exit = false;
    while (!exit) {
      if (Logger.available() > 0) {
        if (Logger.readString() == "USER") {
          exit = true;
        }
      }
    }
    userInput();
  } else {
    TimeStamp();
    Logger.print("TIMER, PYTHON, PASS [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(buffer);
    Logger.println("[END]\n");
  }
}

void waitForRFID(String message, int timeout = 3000) {
  elapsedMillis timeoutTimer;
  String buffer = "";
  char byteBuffer[1];
  bool timedOut = false;
  Logger.print("TIMER, RFID, ");
  Logger.print("[");
  Logger.print(timeout);
  Logger.print("], ");
  Logger.println(message);
  while (buffer.indexOf(message) == -1 && !timedOut) {
    if (RFID.available() > 0) {
      RFID.readBytes(byteBuffer, 1);
      buffer.concat(byteBuffer[0]);
    }
    if (timeoutTimer > timeout || Logger.available() > 0) {
      timedOut = true;
    }
  }
  if (timedOut) {
    TimeStamp();
    Logger.print("TIMER, RFID, FAIL [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(buffer);
    Logger.println("[END]\n");
    bool exit = false;
    while (!exit) {
      if (Logger.available() > 0) {
        if (Logger.readString() == "USER") {
          exit = true;
        }
      }
    }
    userInput();
  } else {
    TimeStamp();
    Logger.print("TIMER, RFID, PASS [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(buffer);
    Logger.println("[END]\n");
  }
}


// Successfully sets up experiment with 2 dummy mice
void setupExperiment(int mice, int entries) {
  numMice = mice;
  Python.readString();
  Logger.println("SETUP STARTING\n");
  reset();
  waitForPython("Arduino Ready", 15000);
  writePython("0");
  waitForPython("Begin Experiment", 15000);
  writePython(String(numMice));
  delay(1000);
  writePython(String(entries));
  delay(5000);
  for (int i = 0; i < numMice; i++) {
    writePython(mouseID[i]);
    waitForPython(mouseID[i], 3000);
  }
  waitForPython("Experiment Timer Started", 5000);

  assertGate(Gate1, GateClosed, 5000);
  assertGate(Gate2, GateClosed, 5000);

  TimeStamp();
  Logger.println("SETUP DONE FOR " + String(numMice) + " MICE\n");
}

void reset() {
  TimeStamp();
  Logger.println("RESET SORTER\n");
  digitalWrite(RESET, LOW);
  delay(100);
  digitalWrite(RESET, HIGH);
}

void startSession(int numMice, int selectedMouse) {
  mouse = selectedMouse;
  TimeStamp();
  Logger.println("STARTING ENTRY\n");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  writeRFID(mouseRFID[mouse], Antenna1);
  waitForPython("Check Exp Time");
  writePython("1");
  waitForPython("Update Timeout");
  String timeoutString = "";
  for (int i = 0; i < numMice; i++) {
    timeoutString = timeoutString + "1";
  }
  writePython(timeoutString);
  waitForPython("G1 Open");
  writeRFID(mouseRFID[mouse], Antenna2);
  assertGate(Gate1, GateOpen, 500);
  assertGate(Gate2, GateClosed, 500);
  digitalWrite(IR3, LOW);
  waitForPython("G1 close", 5000);
  waitForPython("Mouse in tube");
  writeRFID(mouseRFID[mouse], Antenna2);
  waitForPython("Only 1 Mouse");
  waitForPython("G2 Open");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateOpen, 2000);
  writeRFID(mouseRFID[mouse], Antenna3);
  digitalWrite(IR3, HIGH);
  waitForPython("StartMED:");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  TimeStamp();
  Logger.println("SESSION STARTED FOR MOUSE " + String(mouse) + "\n");
}

void MinTimeCheck(int time) {
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  writePython("0");
  waitForPython("Mintimechck");
  delay(time);
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  writePython("0");
  waitForPython("Min Time Reached");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  TimeStamp();
  Logger.println("MIN TIME CHECK DONE");
}

void MaxTimeCheck(int time) {
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  delay(time);
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  waitForPython("Maxtimechck", 10000);
  writePython("0");
  waitForPython("Max Time Reached");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateOpen, 500);
  TimeStamp();
  Logger.println("MAX TIME CHECK DONE");
}

void mouseExit(int activeMouse) {
  mouse = activeMouse;
  String mouseID[4] = { "000000000000", "111111111111", "222222222222", "333333333333" };
  String mouseRFID[4] = { "0000000000000000", "1111111111111111", "2222222222222222", "3333333333333333" };
  digitalWrite(IR3, LOW);
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateOpen, 500);
  writeRFID(mouseRFID[mouse], Antenna2);
  waitForPython("G2 close");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
  waitForPython("Copy Data");
  digitalWrite(IR3, HIGH);
  writeRFID(mouseRFID[mouse], Antenna2);
  waitForPython("G1 Open", 10000);
  assertGate(Gate1, GateOpen, 500);
  assertGate(Gate2, GateClosed, 500);
  writeRFID(mouseRFID[mouse], Antenna1);
  waitForPython("G1 close", 10000);
  waitForPython("Session Finished");
  writePython("1111");
  waitForPython("Restart Cycle");
  assertGate(Gate1, GateClosed, 500);
  assertGate(Gate2, GateClosed, 500);
}

/** 1 - continue
    else - expire
**/
void CheckExpTime(int expired) {
  if (expired == 1) {
    writePython("1");
    waitForPython("Update Timeout");
  } else {
    writePython("0");
    waitForPython("Experiment Terminated");
  }
}


void UpdateTimeout(String timeoutVal) {
  writePython(timeoutVal);
  if (timeoutVal.substring(mouse, mouse + 1) = "1") {
    waitForPython("G1 Open");
  } else {
    waitForPython("attempted to enter");
  }
}

void assertGate(Gate gate, unsigned long position, int timeout) {
  bool gateAtPosition = false;
  bool timedOut = false;
  unsigned long input;
  elapsedMillis timeoutTimer;

  TimeStamp();
  Logger.print("TIMER, SERVO");
  Logger.print(gate - 4);
  Logger.print(", [");
  Logger.print(timeout);
  Logger.print("], ");
  Logger.println(position);

  while (!gateAtPosition && !timedOut) {
    input = pulseIn(gate, HIGH, 50000);
    if (abs(long(position - input)) < 20) {
      gateAtPosition = true;
    } else if (timeoutTimer > timeout) {
      timedOut = true;
    }
  }

  if (timedOut) {
    TimeStamp();
    Logger.print("TIMER, SERVO");
    Logger.print(gate - 4);
    Logger.print(", FAIL [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(input);
    Logger.println();
    bool exit = false;
    while (!exit) {
      if (Logger.available() > 0) {
        if (Logger.readString() == "USER") {
          exit = true;
        }
      }
    }
    userInput();
  } else {
    TimeStamp();
    Logger.print("TIMER, SERVO");
    Logger.print(gate - 4);
    Logger.print(", PASS [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(pulseIn(gate, HIGH, 50000));
    Logger.println();
  }
}

void TimeStamp() {
  unsigned long runMillis = millis();
  unsigned long allSeconds = millis() / 1000;
  int runHours = allSeconds / 3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;

  char buf[20];
  sprintf(buf, "[%02d:%02d:%02d]", runHours, runMinutes, runSeconds);
  Serial.print(buf);
}
