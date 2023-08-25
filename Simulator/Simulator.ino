/** Simulator.ino
  * Last modified 8/24/23
  * Simulates IR sensors and Serial communication
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
               AntennaErr };

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

  Python.read();
  RFID.read();


  // IR sensor setups
  pinMode(IR1, OUTPUT);
  pinMode(IR2, OUTPUT);
  pinMode(IR3, OUTPUT);

  pinMode(Relay1, INPUT);
  pinMode(Relay2, INPUT);
  pinMode(Relay3, INPUT);

  pinMode(RESET,OUTPUT);
  digitalWrite(RESET, HIGH);

  // Startup state
  defaultState();
}

void loop() {
  userInput();
}

void userInput() {
  Logger.println("\nStart User Input");
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
      }  else if (input == "SETUP") {
        setupExperiment();
      }
      else if(input == "RESET") {
        reset();
      }
      else if (input == "END") {
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
  int active = Antenna1;
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

void waitForPython(String message, int timeout) {
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
  }
  else {
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
void setupExperiment() {
  Logger.println("SETUP STARTING\n");
  reset();
  String mouseID[2] = {"000000000000", "111111111111"};
  String mouseRFID[2] = {"0000000000000000", "1111111111111111"};
  waitForPython("Arduino Ready", 15000);
  writePython("0");
  waitForPython("Begin Experiment", 10000);
  writePython("2");
  waitForPython("Num mice: 2", 2000);
  writePython("10");
  waitForPython("Max entries: 10", 2000);
  for(int i = 0; i < 2; i++) {
    writePython(mouseID[i]);
    waitForPython(mouseID[i], 2000);
    waitForPython(String(i), 2000);
  }
  waitForPython("Experiment Timer Started", 5000);

  Logger.println("SETUP DONE\n");  
}

void reset() {
  Logger.println("RESET SORTER\n");
  digitalWrite(RESET, LOW);
  delay(100);
  digitalWrite(RESET, HIGH);
}
