#include <AccelStepper.h>
#include <RCSwitch.h>
#include <EEPROM.h>

#define pinR 12
#define pinG 11
#define pinB 10
#define rele 9
#define pinS1 7
#define pinS2 8

#define enable 6
#define stepper 5
#define dir 4
#define stepsPerRevolution 200 //numero de passos por volta



const byte pulse = 3;
int on = 0;//pode ser necessario inverter o valor para 1
int off = 1;//pode ser necessario inverter o valor para 0
int front = 0; //pode ser necessario inverter o valor para 1
int back = 1;//pode ser necessario inverter o valor para 0
boolean atrazo = false;
int codes[] = { 168018085, 168018101, 222};

boolean inProcess = false;
int operation = -1;

unsigned long ultimoPulso = millis();
unsigned long ultimaLeitura = millis();


float vel_min = 100;
float vel_max = 30;
float ramps = vel_min;
float vel_rampa = 0.01; //velocidade da rampa
/*
   30 macio -> velocidade boa
   100 -> devagar e macio
*/
boolean first_move = true;
int tempo_atraso_acionamento_motor = 3000;




RCSwitch mySwitch = RCSwitch();

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

  pinMode(rele, OUTPUT);

  digitalWrite(pinR, LOW);
  digitalWrite(pinG, LOW);
  digitalWrite(pinB, LOW);

  Serial.begin(115200);

  mySwitch.enableReceive(0); //configura a conexão do modulo de RF no pino 2

  attachInterrupt(digitalPinToInterrupt(pulse), registerPulse, RISING);//configura interrupcao no pino 3 para pulso externo

  digitalWrite(enable, off);
  digitalWrite(stepper, HIGH);
  digitalWrite(dir, HIGH);

  String ultima_operacao = readlastOperation() == 1 ? "Fechando Portão" : "Abrindo Portão";
  Serial.print("Ultima Operação: ");
  Serial.println(ultima_operacao);

  infoLed(1, 0, 0); //indica que o sistema esta parado

  digitalWrite(rele, HIGH);
}

void loop() {


  if (mySwitch.available()) {

    int value = mySwitch.getReceivedValue();

    if (value != 0) {
      //  Serial.println(value);
      if (authenticate(value)) {

        //Serial.println("Codigo cadastrado!");
        if ((millis() - ultimaLeitura) > 3000   ) {
          if (inProcess) {
            Serial.println("Sinal do controle recebido, Processo em execucao, parando...");
            stopProcess();
          } else {
            Serial.println("Sinal do controle recebido, Processo parado, iniciado...");
            process();
          }
          ultimaLeitura = millis();
        }
      }

      mySwitch.resetAvailable();

    }
  }



  if (inProcess) {

    int statusGate = verifyStatusGate();

    //  String status_portao = statusGate == 0 ? "Portão Aberto" : statusGate == 1 ? "Portão fechado" : "Estado Indefinido";

    //  Serial.print("Status atual: ");
    //Serial.println(status_portao);

    if (operation == 0 ) {
      //Serial.print("Operação atual: ");
      // Serial.println("Abrindo o portao");
      // atrazar();

      //gate oppening
      if (statusGate == -1) {
        //gate undefined

        //Serial.println("Estado indefinido, continuando movimento");
        moveGate();
      } else if (statusGate == 1) {
        //gate open
        // Serial.println("Portão esta totalmente fechado, continuando movimento para abrir");
        moveGate();
      } else if (statusGate == 0) {
        //gate open
        // Serial.println("Portão abrir, parar o motor");
        writeLastOperation(1);
        stopProcess();
      }
    } else if (operation == 1) {
      // atrazar();
      // Serial.print("Operação atual: ");
      // Serial.println("Fechando o portao");

      //gate closeding
      if (statusGate == -1) {
        //Serial.println("Estado indefinido, continuando movimento");
        digitalWrite(rele, LOW);
        moveGate();
      } else if (statusGate == 1) {
        //gate close
        // Serial.println("Portão fechou, parar o motor");
        writeLastOperation(0);
        digitalWrite(rele, HIGH); //desligar a lampada, portao fechou
        stopProcess();
      } else if (statusGate == 0) {
        //gate open
        //Serial.println("Portão esta totalmente aberto, continuando movimento para fechar");
        digitalWrite(rele, LOW);
        moveGate();
      }
    }


  }

}


void registerPulse() {
  if ((millis() - ultimoPulso) > 2000) {

    if (inProcess) {
      Serial.println("Pulso externo recebido, Processo em execucao, parando...");
      stopProcess();
    } else {
      Serial.println("Pulso externo recebido, Processo parado, iniciado...");
      process();
    }

    ultimoPulso = millis();
  }
}

int verifyStatusGate() {

  int s1 = digitalRead(pinS1);
  int s2 = digitalRead(pinS2);



  if (s1 == 0 && s2 == 1) {
    //gate closed
    //o portao esta fechando quando s2 esta ativo e s1 desativado
    return 1;
  } else if (s1 == 1 && s2 == 0) {
    //gate open
    //o portao esta aberto quando s1 esta ativo e s1 desativado
    return 0;
  } else {
    //gate status undefined
    return -1;
  }


}



void process() {
  int lastOperation = readlastOperation();
  int status_portao  = verifyStatusGate();
  if (status_portao ==  -1) {
    if (lastOperation == 0) {
      //gate open, closed
      Serial.println("Portao estava abrindo, agora fechando");
      closeGate();
    } else if (lastOperation == 1) {
      //gate close, open
      Serial.println("Portao estava fechado, agora abrindo");
      openGate();
    }
  } else if (status_portao == 0) {
    Serial.println("Sensor detectou portao aberto, fechando...");
    closeGate();
  } else {
    Serial.println("Sensor detectou portao fechado, abrindo...");
    openGate();
  }


}

void openGate() {
  inProcess = true; //processo em execucao
  infoLed(0, 1, 0); //liga o led verde
  writeLastOperation(0);//salva a operacao na memoria -> "Abrindo"
  operation = 0; //define localmente a operação para "Abrindo"
  digitalWrite(dir, back);//define a diracao do motor para tras
  digitalWrite(enable, off); //desliga o motor
  moveGate(); // move o motor
}

void closeGate() {

  inProcess = true; //processo rodando
  infoLed(0, 0, 1); // liga o led em azul
  writeLastOperation(1); //salvar o estado do processo -> "Fechando"
  operation = 1; //operacao local -> "Fechando"
  digitalWrite(dir, front); //define o sentido do motor para frente
  digitalWrite(enable, off); //desliga o motor
  moveGate(); //move o motor

}
void moveGate() {
  int status_portao  = verifyStatusGate();

  if (status_portao == 1) {
    if (first_move) {
      digitalWrite(rele, LOW);
      delay(3000);
      first_move = false;
    }
  }


  digitalWrite(enable, on);
  for (int i = 0; i < stepsPerRevolution * 2; i++) {
    digitalWrite(stepper, HIGH);
    delayMicroseconds(ramps);//velocidade
    digitalWrite(stepper, LOW);
    delayMicroseconds(ramps);

    if (ramps > 30) {
      ramps = ramps - vel_rampa;
    } else if (ramps < 30) {
      ramps = vel_max;
    }
  }
}

void stopProcess() {
  infoLed(1, 0, 0); //liga o led vermelho
  inProcess = false; // para o processo
  digitalWrite(enable, off); //desliga o motor
  first_move = true;
  ramps  = vel_min;
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

void atrazar() {
  if (atrazo) {
    delay(2000);
  }
}
