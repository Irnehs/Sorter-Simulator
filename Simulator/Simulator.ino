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
int numMice = 0;

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
  userInput();
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
        while(Logger.available() == 0) {}
        int desiredNumMice = Logger.readString().toInt();
        Logger.println(desiredNumMice);
        Logger.print("Entries: ");
        while(Logger.available() == 0) {}
        int entries = Logger.readString().toInt();
        Logger.println(entries);
        setupExperiment(desiredNumMice, entries);
      } else if (input == "ENTER") {
        Logger.println("PREPPING ENTRY");
        Logger.print("Mouse to enter: ");
        while(Logger.available() == 0) {}
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
        while(Logger.available() == 0) {}
        int activeMouse = Logger.readString().toInt();
        Logger.println(activeMouse);
        mouseExit(activeMouse);
      } else if (input == "YOLO") {
        setupExperiment(3,10);
        startSession(3,0);
        MinTimeCheck(1000);
        MaxTimeCheck(1000);
        mouseExit(mouse);
        
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
 * 
 * Remaining challenges:
 * How do we make sure that Python responds in a timely manner? Polling could work, but might
 * be a bit slow. Maybe can use interrupt to detect when Python.available() flag is set?


*/
String readPython() {
  String message = Python.readString();
  Logger.print("PYTHON, IN, ");
  Logger.print(message);
  Logger.println("[END]\n");
  return message;
}

void writePython(String message) {
  Python.print(message);
  Logger.print("PYTHON, OUT, ");
  Logger.print(message);
  Logger.println("[END]\n");
}

String readRFID() {
  int active = activeAntenna();
  // int active = activeAntenna();
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
    if (timeoutTimer > timeout || Logger.available() > 0) {
      timedOut = true;
    }
  }
  if (timedOut) {
    Logger.print("TIMER, PYTHON, FAIL [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(buffer);
    Logger.println("[END]\n");
    userInput();
  } else {
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
    Logger.print("TIMER, RFID, FAIL [");
    Logger.print(timeoutTimer);
    Logger.print("/");
    Logger.print(timeout);
    Logger.print("], ");
    Logger.print(buffer);
    Logger.println("[END]\n");
    userInput();
  } else {
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
  waitForPython("Num mice: " + String(numMice), 2000);
  writePython(String(entries));
  waitForPython("Max entries: " + String(entries), 2000);
  for (int i = 0; i < numMice; i++) {
    writePython(mouseID[i]);
    waitForPython(mouseID[i], 2000);
    waitForPython(String(i), 2000);
  }
  waitForPython("Experiment Timer Started", 5000);

  Logger.println("SETUP DONE FOR " + String(numMice) + " MICE\n");
}

void reset() {
  Logger.println("RESET SORTER\n");
  digitalWrite(RESET, LOW);
  delay(100);
  digitalWrite(RESET, HIGH);
}

void startSession(int numMice, int selectedMouse) {
  mouse = selectedMouse;
  Logger.println("STARTING ENTRY\n");
  writeRFID(mouseRFID[mouse], Antenna1);
  waitForPython("Check Exp Time");
  writePython("1");
  waitForPython("Update Timeout");
  String timeoutString = "";
  for(int i = 0; i < numMice; i++) {
    timeoutString = timeoutString + "1";
  }
  writePython(timeoutString);
  waitForPython("1");
  waitForPython("1");
  waitForPython("G1 Open");
  writeRFID(mouseRFID[mouse], Antenna2);
  digitalWrite(IR3, LOW);
  waitForPython("G1 close", 5000);
  waitForPython("Mouse in tube");
  writeRFID(mouseRFID[mouse], Antenna2);
  waitForPython("Only 1 Mouse");
  waitForPython("G2 Open");
  writeRFID(mouseRFID[mouse], Antenna3);
  digitalWrite(IR3, HIGH);
  waitForPython("StartMED:");
  Logger.println("SESSION STARTED FOR MOUSE " + String(mouse) + "\n");
}

void MinTimeCheck(int time) {
  writePython("0");
  waitForPython("Mintimechck");
  delay(time);
  writePython("0");
  waitForPython("Min Time Reached");
}

void MaxTimeCheck(int time) {
  waitForPython("Maxtimechck",10000);
  writePython("0");
  waitForPython("Max Time Reached");
}

void mouseExit(int activeMouse) {
  mouse = activeMouse;
  String mouseID[4] = { "000000000000", "111111111111", "222222222222", "333333333333" };
  String mouseRFID[4] = { "0000000000000000", "1111111111111111", "2222222222222222", "3333333333333333" };
  digitalWrite(IR3, LOW);
  waitForRFID("SRA",15000);
  writeRFID(mouseRFID[mouse], Antenna2);
  waitForPython("Copy Data");
  digitalWrite(IR3, HIGH);
  waitForPython("G1 Open", 10000);
  writeRFID(mouseRFID[mouse], Antenna1);
  writePython("0");
  waitForPython("Restart Cycle");
}

/** 1 - continue
    else - expire
**/
void CheckExpTime(int expired) {
  if(expired == 1) {
    writePython("1");
    waitForPython("Update Timeout");
  }
  else {
    writePython("0");
    waitForPython("Experiment Terminated");
  }
}

//hello
void UpdateTimeout(String timeoutVal) {
  writePython(timeoutVal);
  if(timeoutVal.substring(mouse, mouse + 1) = "1") {
    waitForPython("G1 Open");    
  }
  else {
    waitForPython("attempted to enter");
  }
}

// 0 - correct RFID read
// 1 - alternate RFID read
// 2 - no RFID read
