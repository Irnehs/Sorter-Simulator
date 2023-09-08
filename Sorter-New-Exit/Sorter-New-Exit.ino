/// Libraries to include
#include <elapsedMillis.h>
#include <Servo.h>

///Declaring Variables/Values
Servo gate1;
Servo gate2;

#define IRSENSOR1pin 22  //IR detector gate 1 for sensor 1 (S1)
#define IRSENSOR2pin 23  //IR detector gate 2 for sensor 2 (S2)
#define IRSENSOR3pin 24  //IR detector center tube for sensor 3 (S3)

#define Relay1pin 6  //Relay 1 signal for Antenna 1 (A1)
#define Relay2pin 7  //Relay 2 signal for Antenna 2 (A2)
#define Relay3pin 8  //Relay 3 signal for Antenna 3 (A3)

//Sorter 1 gate settings
//const int gateOpen = 1450;
//const int gateClosed = 1150;

//////Sorter 2 gate settings
// const int gateOpen = 1550;
// const int gateClosed = 1250;

//////Sorter 3 gate settings
//const int gateOpen = 1650;
//const int gateClosed = 1250;

//////Sorter 4 gate settings
//const int gateOpen = 1350;
//const int gateClosed = 1625;

//////Sorter 2 gate settings
const int gateOpen = 1250;
const int gateClosed = 1550;


//Various Timer values
const int checkTime1 = 5000;  // Time to wait for mouse to trigger Antenna 2 on its way to MedPC chamber
const int checkTime2 = 3000;  // Time to check only one mouse in center tube on its way to MedPC chamber
const int checkTime3 = 4000;  // Time to hold mouse in center tube before allowing it back to home cage on its way back
const int gateTime = 5000;    // How long the first gate stays open when a mouse is in the center tube and returning to the cage.
//When multiple mice were at the first gate we experienced that Antenna 1 could sometimes not read the mouse leaving and thus not resetting the main loop.
//This makes sure the door closes, resets the main loop, and a new mouse is able to enter.
const int rfidTime = 4000;  // How long Antenna 2 is turned on for when checkTime1 runs out G1 closes and Sensor 3 detects a mouse inside.
//This was added because we saw that mice can be hesistant and stay right underneath the gate blocking the IR sensor and not triggering Antenna 2. After the mice are comfortable they then
//enter the center tube and this way we give them a 2nd chance to be read by Antenna 2.


//Gate variables
const int steps = 12;
const int degreePerStep = (gateOpen - gateClosed) / steps;

//IR variables
int sensorState1 = 0;
int sensorState2 = 0;
int sensorState3 = 0;


//Values obtained from Python
int maxEntries;
int numMice;
String IDarray[] = { "", "", "", "", "", "", "", "", "", "" };
int TimeoutBool[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };


String ID = "";  //Used for scanning in ID

//Keeping track of entries of each mouse for arduino so it knows if it should allow a mouse to enter based on the max entries constraint
int entries[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int attemptedEntries[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
String invalidArray[] = { "", "", "", "", "", "", "", "", "", "" };

//TRUE OR FALSE that get changed by Python
bool ExperimentTimeReached = false;
bool BeginExp = false;
bool ContExp = true;
bool ExpTerm = false;

//Changed by IR sensors
bool MouseInCenter = true;

//The variable that receives python command to continue executing
String readstring = "";  //RENAME

// Variables for situations with multiple mice in chamber
int numMiceToExit = 0;
String MiceToExitArray[] = { "", "", "", "", "", "", "", "", "", "" };
bool medClosed = false;  // Used to determine if "CloseMed" and "Copy Data" need to be sent


void setup() {

  //Setup serial ports, servo pin locations, IR sensor pin type, initiate pull-up resistor on IR pins
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial1.setTimeout(100);

  gate1.attach(9);
  gate2.attach(10);

  pinMode(IRSENSOR1pin, INPUT);
  pinMode(IRSENSOR2pin, INPUT);
  pinMode(IRSENSOR3pin, INPUT);

  digitalWrite(IRSENSOR1pin, HIGH);
  digitalWrite(IRSENSOR2pin, HIGH);
  digitalWrite(IRSENSOR3pin, HIGH);


  pinMode(Relay1pin, OUTPUT);
  pinMode(Relay2pin, OUTPUT);
  pinMode(Relay3pin, OUTPUT);

  digitalWrite(Relay1pin, LOW);
  digitalWrite(Relay2pin, LOW);
  digitalWrite(Relay3pin, LOW);


  // Reset gates to be closed,allows you to see if gates become blocked
  //Keep gate open so you dont have to turn on/off
  gate1.writeMicroseconds(gateOpen);
  delay(2000);
  gate1.writeMicroseconds(gateClosed);
  delay(2000);
  gate1.writeMicroseconds(gateOpen);
  delay(1000);
  gate2.writeMicroseconds(gateOpen);
  delay(2000);
  gate2.writeMicroseconds(gateClosed);
  delay(2000);
  gate2.writeMicroseconds(gateOpen);



  //For Event based med closure
  //pinMode(26,INPUT);

  //Set RFID module active (when RFID module is powered on it is by default set active. This is incnase it was set deactive during testing) and turn on Antenna 1
  Serial1.write("SRA\r");
  Serial1.readString();

  digitalWrite(Relay1pin, HIGH);


  //Send Arduino Ready message, Scan ID's if neccessary, and begin experiment function
  Serial.println("Arduino Ready");
  while (!BeginExp) {
    while (Serial.available() == 0) {}
    readstring = Serial.readString();
    if (readstring == "1") {
      Serial.println("Scan Mouse ID");
      while (Serial1.available() == 0) {}
      ID = "ScanID:" + Serial1.readString().substring(4, 16);
      Serial.println(ID);
    }

    else if (readstring == "0") {
      Serial.println("Begin Experiment");
      BeginExp = true;
    }
  }

  //Python transfers relevant variables/arrays to Arduino
  while (Serial.available() == 0) {}
  numMice = Serial.readString().toInt();
  while (Serial.available() == 0) {}
  maxEntries = Serial.readString().toInt();

  int i = 0;
  while (i < numMice) {
    while (Serial.available() == 0) {}
    IDarray[i] = Serial.readString();
    Serial.println(IDarray[i]);
    Serial.println(i);
    i++;
  }

  // Close gate before experiment
  gate1.writeMicroseconds(gateClosed);
  delay(500);
  gate2.writeMicroseconds(gateClosed);

  //Send message to Python to start Experiment Timer
  Serial.println("Experiment Timer Started");
}

void loop() {


  //Main loop for regulating mouse entrance into the sorter, is described by the logic flowchart
  while (ContExp) {

    // Variables that clear upon every mouse entrance
    for (int i = 0; i < 10; i++) {
      MiceToExitArray[i] = "";
    }
    numMiceToExit = 0;
    //Variables that reinitialize upon every mouse entrance
    int check = 0;          //index that is used when finding which mouse enters. We search thru the ID array and check will be set to the slot number in the array which has the entering mouse's ID.
    bool M2eM1 = false;     // M @ Antenna 2 equals M @ Antenna 1
    bool Mscanned = false;  //M scanned at Antenna 2
    bool Zone2Cont = true;  // Correct mouse in chamber, only 1
    bool tORf;              //Flexible boolean variable

    //Variables changed by Python
    bool MinTestTimeReached = false;
    bool MaxTestTimeReached = false;

    //Variables that store the ID read at each antenna
    String mouseZone1 = "";  //ID read @Antenna 1
    String mouseZone2 = "";  //ID read @Antenna 2
    String mouseZone3 = "";  //ID read @Antenna 3

    //Extra variables that store ID values for special cases when a mouse is stuck or multiple mice enter
    String mouseStuckID = "";  //mouse in the center tube when it shouldn't be
    String mouseCheck = "";    //Checking for multiple mice
    String ExitID = "";        //Store ID when mouse is coming back into cage


    // Wait for a mouse to trigger Antenna 1
    while (Serial1.available() == 0) {}

    //Read ID from Antenna 1, only stores the string values from position 4 to position 16 of the string(with our current tags it removes -->  999_ or 900_ )
    mouseZone1 = Serial1.readString().substring(4, 16);

    //Prints out the ID to Python serial monitor
    Serial.println(mouseZone1);
    delay(1000);

    //Check if time is under total Exp Time
    Serial.println("Check Exp Time");
    while (Serial.available() == 0) {}
    readstring = Serial.readString();
    if (readstring == "1") {
      ContExp = true;
    } else {
      ContExp = false;
      ExpTerm = true;
    }

    //If current experiment time is less than the total allowable experiment time
    if (ContExp) {

      //Find which mouse is trying to enter by searching thru ID array
      check = 0;
      while (mouseZone1 != IDarray[check]) {
        check++;
      }

      //Send signal to Python to get the updated Timeout status of each mouse
      Serial.println("Update Timeout");

      //Read the timeout array values from Python
      String TimeoutString = "";
      while (Serial.available() == 0) {}
      TimeoutString = Serial.readString();

      int k = 0;
      while (k < numMice) {
        TimeoutBool[k] = TimeoutString.substring(k, k + 1).toInt();
        Serial.println(TimeoutBool[k]);
        k++;
      }


      //Check if mouse can enter based entries  and timeout criteria, turn off Antenna 1, turn on Antenna 2, Open Gate 1
      if (entries[check] < maxEntries && TimeoutBool[check] == 1) {

        digitalWrite(Relay1pin, LOW);   //turn off Antenna 1
        digitalWrite(Relay2pin, HIGH);  //turn on Antenna 2

        gate1.writeMicroseconds(gateOpen);  //open gate 1
        Serial.println("G1 Open");

        //Start checktimer,
        tORf = true;
        elapsedMillis checkTimer;

        //Wait for Antenna 2 to be triggered , close once it is triggered or checkTime 1 is reached
        while (Mscanned == false && checkTimer < checkTime1) {
          mouseZone2 = Serial1.readString().substring(4, 16);
          Serial.println(mouseZone2);

          //the case where the mouse that triggered Antenna 1 triggered Antenna 2
          if (mouseZone2 == mouseZone1) {
            Mscanned = true;
            M2eM1 = true;
          }

          if (mouseZone2 != mouseZone1 && MouseIsInExperiment(mouseZone2) != -1) {  // the case where mouse that triggersgate to open turns around and another mouse enters the tube these conditions are set
            Mscanned = true;
            M2eM1 = false;
          }

          //if no mouse read at Antenna 2
          if (mouseZone2 == "") {
            Mscanned = false;
          }
        }

        CloseGate1();

        // If M2 = M1 , ID read at Antenna 2 equals ID read at Antenna 1
        if (Mscanned == true && M2eM1 == true) {

          //Check to see if there is a mouse inside center tube
          sensorState3 = digitalRead(IRSENSOR3pin);
          if (sensorState3 == LOW) {
            MouseInCenter = true;
            Serial.println("Mouse in tube");
          } else if (sensorState3 == HIGH) {
            MouseInCenter = false;
            Serial.println("Mouse not in tube");
          }

          //Mouse in center tube
          if (MouseInCenter) {

            //Start timer
            tORf = true;
            elapsedMillis checkTimer;

            int i = 0;  /////added this for invalidArray to store the invalid
            while (Zone2Cont && checkTimer < checkTime2) {
              if (Serial1.available() != 0) {  /////////ADDEDEDED NEW TO CHECK
                mouseCheck = Serial1.readString().substring(4, 16);
                Serial.println(mouseCheck);
                if (mouseCheck == mouseZone2 || mouseCheck == "") {
                  Zone2Cont = true;
                  Serial.println("Only 1 Mouse");
                }

                else {
                  int inval = MouseIsInExperiment(mouseCheck);
                  if (inval != -1) {
                    invalidArray[inval] = invalidArray[inval] + 1;
                  }
                  Zone2Cont = false;
                  Serial.println("More than 1 mouse");
                }
              }
            }

            ///Turn off Antenna 2
            digitalWrite(Relay2pin, LOW);


            //If only 1 mouse is in center tube
            if (Zone2Cont) {
AllowEntry:  //This is a goto point when M @ A2 is not read, but there is a mouse in the center tube


              //Turn on Antenna 3
              digitalWrite(Relay3pin, LOW);
              digitalWrite(Relay2pin, LOW);
              digitalWrite(Relay1pin, LOW);

              digitalWrite(Relay3pin, HIGH);

              // Fixed error: Gate would automatically trigger at line 466 since Serial1 was never cleared
              Serial1.readString();

              //Open gate 2
              Serial.println("G2 Open");
              gate2.writeMicroseconds(gateOpen);

              ///Wait for trigger @ Antenna 3, close Gate 2
              while (Serial1.available() == 0) {}
              /////////////ADD some code here to store the ID value and check zone 3 = zone 2 to final check

              mouseZone3 = Serial1.readString().substring(4, 16);

              //Close gate 2 slowly
              CloseGate2();

              //Check to see if mouse in center tube
              MouseInCenter = true;
              while (MouseInCenter) {

                //Read IR tube sensor
                sensorState3 = digitalRead(IRSENSOR3pin);
                if (sensorState3 == LOW) {
                  MouseInCenter = true;
                  Serial.println("Mouse in tube");

                } else if (sensorState3 == HIGH) {
                  MouseInCenter = false;
                  Serial.println("Mouse not in tube");
                }

                //Read IR sensor

                //Mouse in center tube
                if (MouseInCenter) {

                  //Open Gate 2
                  delay(1500);  //To have a little wait time
                  gate2.writeMicroseconds(gateOpen);
                  Serial.println("G2 Open");

                  ///Read from Antenna3 3 and close gate 2
                  while (Serial1.available() == 0) {}
                  mouseZone3 = Serial1.readString().substring(4, 16);

                  ///Close gate 2 slowly
                  CloseGate2();
                }
              }

              // Mark that one mouse has entered
              AddToExitArray(IDarray[check]);
              entries[check] = entries[check] + 1;

              //Send signal to Python to Start Med PC session
              //readstring = "StartMED:";
              Serial.print("M:");
              Serial.println(check);
              Serial.println("StartMED:");
              medClosed = false;

              // Start polling for Antenna 3

              //Waiting for Python to send signal to continue executing code below
              while (Serial.available() == 0) {}
              readstring = Serial.readString();

              //Minimum Time check
              Serial.println("Mintimechck");

              // Turn on Antenna 3 and clear any startup feedback
              Serial1.readString();

              while (!MinTestTimeReached) {
                // read IR3 and allow it to exit if mouse is in center
                if (digitalRead(IRSENSOR3pin) == LOW) {
                  // Detect mouse in center
                  Serial.println("Mouse detected in center");
                  ExitFromCenter();
                  digitalWrite(Relay3pin, HIGH);
                }
                //read RFID 3 and see if another mouse is present, could be a nice to have
                if (Serial1.available() != 0) {
                  readstring = Serial1.readString().substring(4, 16);
                  switch (CheckRFID(readstring, mouseZone3)) {

                    // No valid mouse detected
                    case 0:
                      break;

                    // Mouse that we know entered is detected
                    case 1:
                      break;

                    // Mouse that should not be inside is detected
                    case 2:
                      CloseMedIfNeeded();
                      AddToExitArray(readstring);
                      ExitFromOpChamber(readstring);
                      break;
                  }
                }

                //Ask Python if Min time reached
                if (Serial.available() != 0) {
                  readstring = Serial.readString();
                  if (readstring == "0") {
                    MinTestTimeReached = true;
                  } else {
                    Serial.println("Mintimechck");
                  }
                }
              }

              Serial.println("Min Time Reached");

              //Turn on Antenna 3
              digitalWrite(Relay3pin, HIGH);
              Serial1.readString();  ///CLEAR ANY INFO ,BANDAID FIX

              //Ask Python if max test time reached
              bool askPython = true;
              Serial.println("Maxtimechck");

              while (!MaxTestTimeReached && numMiceToExit != 0) {

                // read IR3 and allow it to exit if mouse is in center
                if (digitalRead(IRSENSOR3pin) == LOW) {
                  // Detect mouse in center
                  Serial.println("Mouse detected in center");
                  ExitFromCenter();
                  digitalWrite(Relay3pin, HIGH);
                }

                if (Serial1.available() != 0) {
                  readstring = Serial1.readString().substring(4, 16);
                  Serial.println(readstring);
                  switch (CheckRFID(readstring, mouseZone3)) {

                    // No valid mouse detected
                    case 0:
                      break;

                    // Mouse that we know entered is detected
                    case 1:
                      CloseMedIfNeeded();
                      AddToExitArray(readstring);
                      ExitFromOpChamber(readstring);
                      break;

                    // Mouse that should not be inside is detected
                    case 2:
                      CloseMedIfNeeded();
                      AddToExitArray(readstring);
                      ExitFromOpChamber(readstring);
                      break;
                  }
                }

                if (Serial.available() != 0) {
                  readstring = Serial.readString();
                  if (readstring == "0") {
                    CloseMedIfNeeded();
                    Serial.println("Max Time Reached");
                    MaxTestTimeReached = true;
                  } else {
                    Serial.println("Maxtimechck");
                  }
                }
              }

              while (numMiceToExit != 0) {
                ExitFromOpChamber(MiceToExitArray[0]);
              }

              //Send commant to Python to execute closing events
              Serial.println("Session Finished");

              //Read the timeout array values from Python
              TimeoutString = "";
              while (Serial.available() == 0) {}
              TimeoutString = Serial.readString();

              k = 0;
              while (k < numMice) {
                TimeoutBool[k] = TimeoutString.substring(k, k + 1).toInt();
                Serial.println(TimeoutBool[k]);
                k++;
              }

            }  ////IF not only one mouse

          }  //If mouse not detected in center tube after Antenna 2 triggered

        }  ///If no mouse scanned at Antenna 2 after checktime1
        else if (Mscanned == false) {
          // Empty to show possibility, but no code needed since it automatically jumps to check center at end
        }  //If mouse read at Antenna 2 is not the same as entry mouse
        else if (Mscanned == true && M2eM1 == false) {

          //Read IR tube sensor
          sensorState3 = digitalRead(IRSENSOR3pin);
          if (sensorState3 == LOW) {
            Serial.println("Mouse in tube");
            digitalWrite(Relay2pin, LOW);
            digitalWrite(Relay2pin, HIGH);
            elapsedMillis rfidTimer;
            while (rfidTimer < 5000) {
              if (Serial1.available() != 0) {
                readstring = Serial1.readString().substring(4, 16);
                if (MouseIsInExperiment(readstring) != -1) {
                  for (int i = 0; i < numMice; i++) {
                    if (IDarray[i] == readstring) {
                      check = i;
                      break;
                    }
                  }
                  if (TimeoutBool[check] == 1) {
                    goto AllowEntry;
                  }
                }
              }
            }

          } else if (sensorState3 == HIGH) {
            Serial.println("Mouse not in tube");
          }
        }

      }  //IF entries > maxEntries or time < timeout for mouse trying to enter

      //Attempted entry message and check that mouse has left zone 1
      else {
        attemptedEntries[check] = attemptedEntries[check] + 1;
        Serial.print("Mouse ");
        Serial.print(mouseZone1);
        Serial.println(" attempted to enter");
      }


      //Check to see if there is a mouse inside center tube before restarting cycle,

      //Read IR tube sensor
      sensorState3 = digitalRead(IRSENSOR3pin);
      if (sensorState3 == LOW) {
        Serial.println("Mouse in tube");
        ExitFromCenter();

      } else if (sensorState3 == HIGH) {
        Serial.println("Mouse not in tube");
      }

      //Resetting everything before starting a new cycle as a system redundancy
      //Turn on Antenna 1, off Antenna 2/3
      digitalWrite(Relay1pin, HIGH);
      digitalWrite(Relay2pin, LOW);
      digitalWrite(Relay3pin, LOW);

      //Clear RFID board of any tags read at antenna 1
      while (Serial1.available() != 0) {
        Serial1.readString();
      }

      //Print restart cycle
      Serial.println("Restart Cycle");

    }  //If experiment time is less than total allowable experiment time
  }    //While loop for overall experiment time


  if (ExpTerm == true) {
    Serial.println("Experiment Terminated");
    ExpTerm = false;
  }
}

// Returns 0 if no RFID match, 1 if RFID matches target, 2 if RFID matches another mouse
int CheckRFID(String toCheck, String target) {
  return (toCheck == target) ? 1 : MouseIsInExperiment(toCheck) != -1 ? 2
                                                                      : 0;
}

// Close gate 1 with safety
void CloseGate1() {
  ///Close gate 1 slowly
  int gateCounter = 0;
  while (gateCounter < steps) {
    sensorState1 = digitalRead(IRSENSOR1pin);
    if (sensorState1 == HIGH) {
      gate1.writeMicroseconds(gateOpen - (gateCounter + 1) * degreePerStep);
      gateCounter++;
      delay(150);
    }
  }
  Serial.println("G1 close");
}
// Close gate 2 with safety
void CloseGate2() {
  ///Close gate 2 slowly
  int gateCounter = 0;
  while (gateCounter < steps) {
    sensorState2 = digitalRead(IRSENSOR2pin);
    if (sensorState2 == HIGH) {
      gate2.writeMicroseconds(gateOpen - (gateCounter + 1) * degreePerStep);
      gateCounter++;
      delay(150);
    }
  }
  Serial.println("G2 close");
}

// Adds mouse to exit array if it is not already in it
void AddToExitArray(String ID) {
  String updated[] = { "", "", "", "", "", "", "", "", "", "" };
  int k = 0;
  for (int i = 0; i < numMiceToExit; i++) {
    if (MiceToExitArray[i] != "" && MiceToExitArray[i] != ID) {
      updated[k++] = MiceToExitArray[i];
    }
  }
  updated[k] = ID;
  numMiceToExit = k + 1;
  for (int i = 0; i < sizeof(MiceToExitArray) / sizeof(MiceToExitArray[0]); i++) {
    MiceToExitArray[i] = updated[i];
  }
}

// Removes mouse from exit array if it is in it
void RemoveFromExitArray(String ID) {
  String updated[] = { "", "", "", "", "", "", "", "", "", "" };
  int k = 0;
  for (int i = 0; i < numMiceToExit; i++) {
    if (MiceToExitArray[i] != "" && MiceToExitArray[i] != ID) {
      updated[k++] = MiceToExitArray[i];
    }
  }
  numMiceToExit = k;
  for (int i = 0; i < sizeof(MiceToExitArray) / sizeof(MiceToExitArray[0]); i++) {
    MiceToExitArray[i] = updated[i];
  }
}

// Returns whether mouse is in ExitArray
bool WaitingToExit(String ID) {
  for (int i = 0; i < numMiceToExit; i++) {
    if (MiceToExitArray[i] == ID) {
      return true;
    }
  }
  return false;
}

// Returns index of mouse in IDarray. -1 if not found
int MouseIsInExperiment(String ID) {
  for (int i = 0; i < numMice; i++) {
    if (IDarray[i] == ID) {
      return i;
    }
  }
  return -1;
}

// Lists all mice in ExitArray
void ShowMiceToExit() {
  bool cont = true;
  Serial.println("Mice to Exit:");
  for (int i = 0; i < numMiceToExit; i++) {
    Serial.println(MiceToExitArray[i]);
  }
  Serial.println();
}

// Returns numMiceToExit
int ExitFromCenter() {

  while (1) {
    Serial.println("Exiting from center");
    digitalWrite(Relay1pin, LOW);
    digitalWrite(Relay2pin, LOW);
    digitalWrite(Relay3pin, LOW);
    digitalWrite(Relay2pin, HIGH);
    String target;
    bool targetFound = false;
    elapsedMillis rfidTimer;
    while (!targetFound && rfidTimer < 10000) {
      if (Serial1.available() != 0) {
        target = Serial1.readString().substring(4, 16);
        if (target != "" && MouseIsInExperiment(target) != -1) {
          AddToExitArray(target);
          targetFound = true;
        }
      }
    }

    if (!targetFound) {
      Serial.println("No RFID scanned in middle");
      return -1;
    } else {
      Serial.println("Mouse detected: " + target);
      digitalWrite(Relay2pin, LOW);
      digitalWrite(Relay1pin, HIGH);

      bool mouseAtA1 = false;
      String read;

      Serial.println("G1 Open");
      gate1.writeMicroseconds(gateOpen);

      elapsedMillis gateTimer;
      while (!mouseAtA1 && gateTimer < 5000) {
        if (Serial1.available() != 0) {
          read = Serial1.readString().substring(4, 16);
          Serial.println(read);
          if (read == target) {
            Serial.println("Target moved to A1");
            mouseAtA1 = true;
          }
        }
      }
      if (!mouseAtA1) {
        Serial.println("Target not read at A1");
      }
      CloseGate1();
      digitalWrite(Relay1pin, LOW);
      RemoveFromExitArray(target);
      if (digitalRead(IRSENSOR3pin) == HIGH) {
        Serial.println("Mouse is not in center");
        ShowMiceToExit();
        return numMiceToExit;
      } else {
        Serial.println("Mouse is in center");
        ShowMiceToExit();
        Serial.println("Starting exit of unidentified mouse");
      }
    }
  }
}

// Returns ExitFromCenter. Loops until mouse is in center and it can call ExitFromCenter
int ExitFromOpChamber(String id) {
  String readstring;
  while (1) {
    Serial.println("Exiting from opchamber");
    digitalWrite(Relay1pin, LOW);
    digitalWrite(Relay2pin, LOW);
    digitalWrite(Relay3pin, LOW);
    digitalWrite(Relay2pin, HIGH);

    Serial.println("G2 Open");
    gate2.writeMicroseconds(gateOpen);


    bool mouseAtA2 = false;
    while (!mouseAtA2) {
      if (Serial1.available() != 0) {
        String read = Serial1.readString().substring(4, 16);
        Serial.println(read);
        switch (CheckRFID(read, id)) {
          // No match
          case 0:
            Serial.println("Invalid RFID");
            break;

          // Matches target
          case 1:
            Serial.println("Matches target");
            CloseMedIfNeeded();
            AddToExitArray(read);
            CloseGate2();
            if (digitalRead(IRSENSOR3pin) == LOW) {
              Serial.println("Mouse in center");
              ShowMiceToExit();
              return ExitFromCenter();
            } else {
              Serial.println("Mouse not in center");
              ShowMiceToExit();
              gate2.writeMicroseconds(gateOpen);
              Serial.println("G2 Open");
              mouseAtA2 = true;
            }
            break;

            // Matches another mouse
          case 2:
            Serial.println("Mouse other than target detected");
            CloseMedIfNeeded();
            AddToExitArray(read);
            CloseGate2();
            if (digitalRead(IRSENSOR3pin) == LOW) {
              Serial.println("Mouse in center");
              return ExitFromCenter();
            } else {
              Serial.println("Mouse not in center");
              gate2.writeMicroseconds(gateOpen);
              Serial.println("G2 Open");
              mouseAtA2 = true;
            }
            break;
        }
      }
    }
  }
}

void CloseMedIfNeeded() {
  if (!medClosed) {
    Serial.println("CloseMED");
    Serial.println("Copy Data");
    medClosed = true;
  }
}
