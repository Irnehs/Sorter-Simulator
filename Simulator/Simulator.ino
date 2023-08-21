/** Simulator.ino
  * Last modified 8/21/23
  * Idea is to simulate IR sensors and serial communications for testing
  * Outputs Serial information

**/

// State variables
bool isDefaultState = false;


// IR Sensor Pins
#define IR1 13
#define IR2 12
#define IR3 11

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

  // Startup state
  defaultState();
}


void loop() {
  if (!isDefaultState) {
    defaultState();
    isDefaultState = true;
  }
}



void defaultState() {
  digitalWrite(IR1, HIGH);
  digitalWrite(IR2, HIGH);
  digitalWrite(IR3, HIGH);
}

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

void writeRFID(String message) {
  RFID.print(message);
  Logger.print("RFID, OUT, ");
  Logger.println(message);
}
