/** Simulator.ino
  * Last modified 8/21/23
  * Idea is to simulate IR sensors and serial communications for testing
  * Outputs Serial information

**/

// State variables
bool isDefaultState = false;
enum Antenna {Antenna1 = 1, Antenna2 = 2, Antenna3 = 4};
#define Antenna1 1
#define Antenna2 2
#define Anntena3 4



// IR Sensor Pins
#define IR1 11
#define IR2 12
#define IR3 13

#define Relay1 8
#define Relay2 9
#define Relay3 10

// Serial communications
#define Logger Serial
#define Python Serial1
#define RFID Serial2


// Setup
void setup() {
  // Serial setups
  Logger.begin(9600);
  Logger.setTimeout(300);

  Python.begin(9600);

  RFID.begin(9600);


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


void loop() {
  if (!isDefaultState) {
    defaultState();
    isDefaultState = true;
  }
}


// Default state: all IR sensors beams not broken and no antenna selected
void defaultState() {
  digitalWrite(IR1, HIGH);
  digitalWrite(IR2, HIGH);
  digitalWrite(IR3, HIGH);
}


int activeAntenna() {
  return digitalRead(Relay1) + 2 * digitalRead(Relay2) + 4 * digitalRead(Relay3);
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
  String message = RFID.readString();
  Logger.print("RFID, IN, ");
  Logger.println(message);
  return message;
}

void writeRFID(String message, Antenna antenna) {
  if(antenna == activeAntenna()) {
    RFID.print(message);
    Logger.print("RFID");
    Logger.print(antenna);
    Logger.print(", OUT, ");
    Logger.println(message);
  }
  else {
    Logger.print("RFID");
    Logger.print(antenna);
    Logger.print(", OUT, ");
    Logger.print("NR: ");
    Logger.println(message);
  }
  
  
}
