// Initialize set up for servo motor
#include <Servo.h>
#include <string.h>

// Pin assignments
const int closeGateEmitPin = 9;
const int closeGateRecvPin = 12;
const int disableEmitPin = 10;
const int disableRecvPin = 13;
const int openGateEmitPin = 11;
const int openGateRecvPin = 14;
const int motorGate0Pin = 5;
const int motorGate1Pin = 6;
const int usEchoPin = 2;
const int usTrigPin = 3;
const int buzzerPin = 4;
const int LED0Pin = 22;
const int LED1Pin = 24;

// Constants needed for US sensor
const float speed = 0.0343;
const float normalDistance = 16; //MEASURE THE DISTANCE RETURNED WHEN THE GATES ARE OPENED AND CLOSED AND SET THE LOWEST VALUE HERE, ONLY WHEN THE DISTANCE GETS LOWER THAN THAT IS THERE AN OBSTRUCTION ON THE RAILROAD

// Initialize variables for logic
int oldOpenRecvState = 0;
int openRecvState = 0;
int oldCloseRecvState = 0;
int closeRecvState = 0;
int oldDisableRecvState = 0;
int disableRecvState = 0;
int closeCount = 0;
int openCount = 0;
Servo gate0, gate1;
bool gateUp = true;
float time, distance;
String sendCommand;


// Creates struct that contains all given commands from LabView
struct cmd{
    String fullCommand;
    int gateCommand;
    int LEDCommand;
    int soundCommand;
    int firstIndex;
    int secondIndex;
};

// Initial setup for program
void setup() {
  // Sets IR emitters and receiver pins
  pinMode(openGateEmitPin, OUTPUT);
  pinMode(closeGateEmitPin, OUTPUT);
  pinMode(disableEmitPin, OUTPUT);
  pinMode(openGateRecvPin, INPUT);
  pinMode(closeGateRecvPin, INPUT);
  pinMode(disableRecvPin, INPUT);

  // Sets up LED pins
  pinMode(LED0Pin, OUTPUT);
  pinMode(LED1Pin, OUTPUT);

  // Sets up US sensor pins
  pinMode(usEchoPin, INPUT);
  pinMode(usTrigPin, OUTPUT);

  // Sets up motor pins
  gate0.attach(motorGate0Pin);
  gate1.attach(motorGate1Pin);

  // Initializes motors to return to their starting angles
  gate0.write(0);
  gate1.write(1);

  // Required for reading and writing to LabView
  Serial.begin(9600);

}

// Generates a 38kHz pulse that allows IR receivers to detect light
void send38kHz(int durationMicros) {
  unsigned long startTime = micros();
  while (micros() - startTime < durationMicros) {
    digitalWrite(openGateEmitPin, HIGH);
    digitalWrite(closeGateEmitPin, HIGH);
    digitalWrite(disableEmitPin, HIGH);
    delayMicroseconds(13);  // 13 µs HIGH
    digitalWrite(openGateEmitPin, LOW);
    digitalWrite(closeGateEmitPin, LOW);
    digitalWrite(disableEmitPin, LOW);

    delayMicroseconds(13);  // 13 µs LOW
    // 26 µs total = ~38.4 kHz
  }
}

// Sets up cmd data type 
cmd getInputs(){
  cmd setCommand;
  // Receives a string of information from LabView
  if (Serial.available() > 0){
    // Should be in the format of: [sound on?],[lights on?],[gate up?] = 1,0,1 to turn the sound on, turn the lights off, and move the gate up for example
    setCommand.fullCommand = Serial.readStringUntil('/n');
    setCommand.firstIndex = setCommand.fullCommand.indexOf(',');
    setCommand.secondIndex = setCommand.fullCommand.indexOf(',',setCommand.firstIndex + 1);
    setCommand.soundCommand = setCommand.fullCommand.substring(0,setCommand.firstIndex).toInt();
    setCommand.LEDCommand = setCommand.fullCommand.substring(setCommand.firstIndex+1,setCommand.secondIndex).toInt();
    setCommand.gateCommand = setCommand.fullCommand.substring(setCommand.secondIndex+1).toInt();
  }

  return setCommand;
}

// Controls the LED lights in the system
void checkLights(cmd desCommand, bool gateUp){
  // A command from LabView will overwrite any existing logic
  if (desCommand.LEDCommand == 1 || !gateUp){ 
    // Flashing light sequence on LED0 and LED1
    digitalWrite(LED0Pin, HIGH);
    digitalWrite(LED1Pin, LOW);
    delay(100);
    digitalWrite(LED0Pin, LOW);
    digitalWrite(LED1Pin, HIGH);
    delay(100);
  } else {
    // LEDs are turned off
    digitalWrite(LED0Pin, LOW);
    digitalWrite(LED1Pin, LOW);
  }
}

// Controls the buzzer in the system
void checkAlarm(cmd desCommand, bool gateUp){
  // A command from LabView will overwrite any existing logic
  if (desCommand.soundCommand == 1 || !gateUp){ 
    tone(buzzerPin, 440);
    delay(100);
    tone(buzzerPin, 360);
    delay(100);
  } else {
    noTone(buzzerPin);
  }
}

// Sends out a pulse from the US sensor
float sendPulse(){
  digitalWrite(usTrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(usTrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(usTrigPin, LOW);

  time = pulseIn(usEchoPin, HIGH);
  return (time*speed)/2;
}

void loop() {
  cmd desCommand;

  sendCommand = "";

  // Gets inputs from LabView via Serial commands
  desCommand = getInputs();
  
  // Sends a short IR pulse for emitters to read
  send38kHz(1000); // 1 ms of 38kHz

  // Reads the receiver output for all IR receivers
  closeRecvState = digitalRead(closeGateRecvPin);
  openRecvState = digitalRead(openGateRecvPin);
  disableRecvState = digitalRead(disableRecvPin);

  // Gets distance of obstruction using US sensor
  distance = sendPulse();
  //Serial.println(distance);

  // Creates string to send to LabView
  if (disableRecvState == 1 || distance > normalDistance){
    sendCommand += "0,";
  } else {
    sendCommand = sendCommand + String(distance) + ",";
  } 


  // Gets the status of the close IR receiver and increments a counter for however long it has been blocked without changing (otherwise it resets back to 0)
  if (closeRecvState == 1){
    closeCount++;
    if (closeRecvState != oldCloseRecvState){
      closeCount = 0;
    }
    oldCloseRecvState = 1;
  } else {
    oldCloseRecvState = 0;
    closeCount = 0;
  }

  // Gets the status of the open IR receiver and increments a counter for however long it has been blocked without changing (otherwise it resets back to 0)
  if (openRecvState == 1){
    openCount++;
    if (openRecvState != oldOpenRecvState){
      openCount = 0;
    }
    oldOpenRecvState = 1;
  } else {
    oldOpenRecvState = 0;
    openCount = 0;
  }


  // When either counter has been blocked for over 5 cycles, it will toggle the gate variable
  if (openCount > 2){
    gateUp = true;
  } else if (closeCount > 4){
    gateUp = false;
  }

  // Moves the gate up or down based on the gate variable
  if (!gateUp || desCommand.gateCommand == 1){
    sendCommand += "1"; 
    gate0.write(90);
    gate1.write(90);
  } else if (gateUp){
    sendCommand += "0"; 
    gate0.write(0);
    gate1.write(0);
  }

  // Controls lights depending on receiver states
  checkLights(desCommand, gateUp);

  // Controls alarm depending on receiver states
  checkAlarm(desCommand, gateUp);
  
  Serial.println(sendCommand);
  

  delay(600); // Delays before next reading
}
