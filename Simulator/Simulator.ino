/** Simulator.ino
  * Last modified 8/21/23
  * 
  *

**/

#define IR1 13
#define IR2 12
#define IR3 11

#define Computer Serial
#define RFID Serial1



void setup() {
  Computer.begin(9600);
  RFID.begin(9600);

  pinMode(IR1, OUTPUT);
  pinMode(IR2, OUTPUT);
  pinMode(IR3, OUTPUT);

}


void loop() {

    
}