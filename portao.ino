#include <AccelStepper.h>
#include <RCSwitch.h>


#define pinR 12
#define pinG 11
#define pinB 10

#define pinS1 7
#define pinS2 8

#define enable 6
#define stepper 5
#define dir 4
#define pulse 3

int on = 0;//pode ser necessario inverter o valor para 1
int off = 1;//pode ser necessario inverter o valor para 0

int front = 0; //pode ser necessario inverter o valor para 1
int back = 1;//pode ser necessario inverter o valor para 0

#define stepsPerRevolution 200

#include <EEPROM.h>

RCSwitch mySwitch = RCSwitch();

int codes[] = { 000, 111, 222};

boolean inProcess = false;
int operation = -1;


void setup() {
  // put your setup code here, to run once:

  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);

  pinMode(pinS1, INPUT);
  pinMode(pinS2, INPUT);

  pinMode(enable, OUTPUT);
  pinMode(stepper, OUTPUT);
  pinMode(dir, OUTPUT);

  digitalWrite(pinR, LOW);
  digitalWrite(pinG, LOW);
  digitalWrite(pinB, LOW);

  Serial.begin(9600);

  mySwitch.enableReceive(0);


  digitalWrite(enable, off);
  digitalWrite(stepper, HIGH);
  digitalWrite(dir, HIGH);

}

void loop() {
  readSignal();

  if (inProcess) {
    int statusGate = verifyStatusGate();
    if (operation == 0) {
      //gate oppening
      if (statusGate == -1) {
        //gate undefined
        moveGate();
      } else if (statusGate == 0) {
        //gate open
        moveGate();
      } else if (statusGate == 1) {
        //gate close
        writeLastOperation(1);
        stopProcess();
      }
    } else if (operation == 1) {
      //gate closeding
      if (statusGate == 1) {
        moveGate();
      } else if (statusGate == 0) {
        //gate close
        writeLastOperation(0);
        stopProcess();
      } else if (statusGate == 1) {
        //gate open
        moveGate();
      }
    }
  }

}

int verifyStatusGate() {

  int s1 = digitalRead(pinS1);
  int s2 = digitalRead(pinS2);

  if (s1 == 0 && s2 == 1) {
    //gate closed
    return 1;
  } else if (s1 == 1 && s2 == 0) {
    //gate open
    return 0;
  } else {
    //gate status undefined
    return -1;
  }


}

void readSignal() {
  if (mySwitch.available()) {

    int value = mySwitch.getReceivedValue();

    if (value != 0) {
      Serial.println(value);
      if (authenticate(value)) {

        if (inProcess) {
          stopProcess();
        } else {
          process();
        }
      }
    }

    mySwitch.resetAvailable();

  }
}

void process() {
  int lastOperation = readlastOperation();
  if (lastOperation == 0) {
    //gate open, closed
    closeGate();
  } else if (lastOperation == 1) {
    //gate close, open
    openGate();
  }
}

void openGate() {
  inProcess = true;
  writeLastOperation(0);
  operation = 0;
  digitalWrite(enable, on);
  digitalWrite(dir, back);
  moveGate();

}

void moveGate() {
  int ramps = 100;

  for (int i = 0; i < stepsPerRevolution * 5; i++) {
    digitalWrite(stepper, HIGH);
    delayMicroseconds(ramps);
    digitalWrite(stepper, LOW);
    delayMicroseconds(ramps);
    ramps += 100;
  }
}

void stopProcess() {
  digitalWrite(enable, off);
}

void closeGate() {
  inProcess = true;
  writeLastOperation(1);
  operation = 1;
  digitalWrite(enable, on);
  digitalWrite(dir, front);
  moveGate();

}

boolean authenticate(int code) {
  boolean code_valid = false;
  for (int i = 0; i < sizeof(codes) / sizeof(int); i++) {
    if (codes[i] == code) {
      code_valid = true;
      break;
    }
  }

  return code_valid;


}

void writeLastOperation(int operation) {
  EEPROM.write(0, operation);
}

int readlastOperation() {
  return EEPROM.read(0);
}


void infoLed(int r, int g, int b) {
  digitalWrite(pinR, r);
  digitalWrite(pinG, g);
  digitalWrite(pinB, b);
}
