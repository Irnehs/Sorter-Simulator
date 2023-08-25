/** Simulator.ino
  * Last modified 8/24/23
  * Simulates IR sensors and Serial communication
  * When updating Sorter code, ensure PINS 0,1, and reset are unplugged
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
          default:
            antenna = AntennaErr;
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
      } else if (input.startsWith("WM:")) {
        messageToWait = input.substring(3, input.length());
        Logger.println("Will wait for:" + messageToWait);
      } else if (input.startsWith("WT:")) {
        waitTime = input.substring(3, input.length()).toInt();
        Logger.print("Will wait for: ");
        Logger.print(String(waitTime));
        Logger.println(" ms");
      } else if (input == "SETUP") {
        Logger.println("PREPPING SETUP:");
        Logger.print("Num mice: ");
        while(Logger.available() == 0) {}
        int numMice = Logger.readString().toInt();
        Logger.println(numMice);
        Logger.print("Entries: ");
        while(Logger.available() == 0) {}
        int entries = Logger.readString().toInt();
        Logger.println(entries);
        setupExperiment(numMice, entries);
      } else if (input == "ENTER") {
        Logger.println("PREPPING ENTRY");
        Logger.print("Num mice: ");
        while(Logger.available() == 0) {}
        int numMice = Logger.readString().toInt();
        Logger.println(String(numMice));
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
        Logger.println("End User Input");
        return;
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


// Successfully sets up experiment with 2 dummy mice
void setupExperiment(int numMice, int entries) {
  Python.readString();
  Logger.println("SETUP STARTING\n");
  reset();
  String mouseID[4] = { "000000000000", "111111111111", "222222222222", "333333333333" };
  String mouseRFID[4] = { "0000000000000000", "1111111111111111", "2222222222222222", "3333333333333333" };
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

void startSession(int numMice, int mouse) {
  Logger.println("STARTING ENTRY\n");
  String mouseID[4] = { "000000000000", "111111111111", "222222222222", "333333333333" };
  String mouseRFID[4] = { "0000000000000000", "1111111111111111", "2222222222222222", "3333333333333333" };
  writeRFID(mouseRFID[0], Antenna1);
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
