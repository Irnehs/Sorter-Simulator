/** Simulator.ino
  * Last modified 8/21/23
  * Idea is to simulate IR sensors and serial communications for testing
  * Outputs Serial information

**/

#include <SoftwareSerial.h>

// State variables
bool isDefaultState = false;
enum Antenna { NoAntenna,
               Antenna1,
               Antenna2,
               Antenna3,
               AntennaErr };

// IR Sensor Pins
#define IR1 11
#define IR2 12
#define IR3 13

#define Relay1 8
#define Relay2 9
#define Relay3 10

#define rxPython 3
#define txPython 4
#define rxRFID 6
#define txRFID 7

// Serial communications
#define Logger Serial
#define NULL_REPLY ""
#define ERR_REPLY "ERR"
SoftwareSerial Python = SoftwareSerial(rxPython, txPython);
SoftwareSerial RFID = SoftwareSerial(rxRFID, txRFID);

// Setup
void setup() {

  // Serial setups
  Logger.begin(9600);
  Logger.setTimeout(250);

  Python.begin(9600);
  Python.setTimeout(250);

  RFID.begin(9600);
  RFID.setTimeout(250);

  Python.read();
  RFID.read();


  // IR sensor setups
  pinMode(IR1, OUTPUT);
  pinMode(IR2, OUTPUT);
  pinMode(IR3, OUTPUT);

  pinMode(Relay1, INPUT);
  pinMode(Relay2, INPUT);
  pinMode(Relay3, INPUT);
 
  // Startup state
  defaultState();
}

bool state = false;
void loop() {
  if (!isDefaultState) {
    defaultState();
    isDefaultState = true;
  }

  if (Python.available() > 0) {
    Python.readString();
    state ^= true;
    if (state == true) {
      digitalWrite(13, HIGH);
      state == false;
    }  
    else
      digitalWrite(13, LOW);
  }

  delay(1000);
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
  Logger.println(message);
  return message;
}

void writePython(String message) {
  Python.print(message);
  Logger.print("PYTHON, OUT, ");
  Logger.println(message);
}

String readRFID() {
  int active = activeAntenna();
  if (active == Antenna1 || active == Antenna2 || active == Antenna3) {
    String message = RFID.readString();
    Logger.print("RFID");
    Logger.print(active);
    Logger.print(", IN, ");
    Logger.println(message);
    return message;
  } else if (active == NoAntenna) {
    Logger.print("RFID");
    Logger.print(active);
    //NSNR - None Selected Not Recieved
    Logger.print(", IN, NSNR:");
    Logger.println(NULL_REPLY);
    return (NULL_REPLY);
  } else if (active == AntennaErr) {
    Logger.print("RFID");
    Logger.print(active);
    //SENR - Selection Error Not Recieved
    Logger.print(", OUT, SENR:");
    Logger.println(ERR_REPLY);
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
    Logger.println(message);
  } else if (active == NoAntenna || antenna == NoAntenna) {
    RFID.print(message);
    Logger.print("RFID");
    Logger.print(antenna);
    //NSNR - None Selected Not Recieved
    Logger.print(", OUT, NSNR:");
    Logger.println(message);
  } else if (active == AntennaErr || antenna == AntennaErr) {
    RFID.print(message);
    Logger.print("RFID");
    Logger.print(antenna);
    //NSNR - Selection Error Not Recieved
    Logger.print(", OUT, SENR:");
    Logger.println(message);
  }
}

void waitForPython(String message) {
  while(1) {
    while(Python.available() == 0) {}
    String readstring = readPython();
    if(readstring == message) {
      return;
    }
  }
}

void waitForRFID(String message) {
  while(1) {
    while(RFID.available() == 0) {}
    String readstring = readRFID();
    if(readstring == message) {
      return;
    }
  }
}

  
