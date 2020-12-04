// Biblotek //
#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <Servo.h>

// Pins //
SoftwareSerial ESPserial(3, 1);
Servo SteeringServo;
#define D1 0
#define Pw 5
#define He 4


// utifrån vinkeln på väglinejn reglera requested rpm
// utifrån requested rpm 


// Variabler //

// tid
unsigned long previousMillis = 0, currentMillis = 0;

// pwm termer
const int maxPwm = 750, minPwm = 0;
const float Ki = 2, Kp = 5, Kd = 2, SpeedC = 0.1; // vad gör SpeedC? kehttahn?

// framräknade pwm termer
float e = 0, KiArea = 0;
int SerialDataBufferRPMStart = 10, SerialDataBufferRPM = 0;

// hastigheter
const int RequestedRPMBase = 70; // vill du ha en hastighet på 750 pwm, ger det dig en RequestedRPM på 750 / speedToRpm
float RequestedRPM = 0;
float RPM = 0;
int Speed = 0;
int Rev = 0;
float pt, it, dt;

// väglinjer
int V = 0, X = 0;
float rads = 0;

// distanser
const int roadDist = 220 * 3; //123 *3
int RevRoadDist = 0;

// stränger till json
String payload = "", readString = "";
char c;
int objSize = 0;

// json
StaticJsonBuffer<256> jsonBuffer;
JsonArray& obj = jsonBuffer.parseArray("[0, [0,0,0,0], 0, 0]");

// variabler till json
const String OWNER = "S";
int matrix[] = {0, 0, 0, 0};
int obstacle[] = {0, 0};

// lampa
int LedState = LOW;

// States //
typedef enum States {
  Stopped, FollowLine, Observe
};
States State;

// Mqtt //
void onConnectionEstablished();

EspMQTTClient client(
  "ABB_Indgym",
  "7Laddaremygglustbil", //Welcome2abb
  "maqiatto.com",
  1883,
  "simon.ogaardjozic@abbindustrigymnasium.se",
  "scavenger",
  "scavengerS",
  onConnectionEstablished,
  true,
  true
);

void onConnectionEstablished() {
  sendJSON("[\"" + OWNER + "\", \"Connected\"]");
  client.subscribe("simon.ogaardjozic@abbindustrigymnasium.se/Scavenger", [] (const String & payload) {
    StaticJsonBuffer<500> JsonBuffer;
    JsonArray& root = JsonBuffer.parseArray(payload);
    if (payload == "u good bro?"){
      sendJSON("[\"" + OWNER + "\", \"Connected\"]"); //println im good bro
    }
    if (root.success() && root[0] == OWNER) { // här är det endast 1 case (observe) i v2
      if (root[1] == "0") {
        resetValues();
//        Serial.println("driving forward");
        Serial.println("RevRoadDist | msDif | Rev || sin(rads) | RPM | RequestedRPM | Speed");
        State = FollowLine;
      } else if (root[1] == "3") {
        resetValues();
//        Serial.println("observing");
        State = Observe;
      }
    }
  });
}

// Setup //
void setup() {
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(He), HtoL, FALLING);
  pinMode(Pw, OUTPUT), pinMode(D1, OUTPUT), pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(D1, HIGH);
  SteeringServo.attach(13);
  State = Stopped;
}

// Loop //
void loop() {
  client.loop();
  if (!client.isConnected()) {
    Blink(100);
  }
  switch (State) {
    case Stopped:
      Blink(500);
      State = Stopped;
      break;

    case Observe:
      objSize = 3; // här skickar v2 ["s","observing"]
//      sendJSON("[\"" + OWNER + "\", \"Observing\"]");
      sendJSON("fuck you server");
      while (objSize == 3) { // hoppar ut när maixpy ändrar case
        SendSerialData();
        SerialData();
      }

      delay(250);
      resetValues();
      objSize = 2;
      while (objSize == 2) {
        SerialData(); // kollar vägtyp
      }
      sendJSON(String("[\"" + OWNER + "\", [" + String(matrix[0]) + ", " + String(matrix[1]) + ", " + String(matrix[2]) + ", " + String(matrix[3]) + "]]"));
//      delay(10);
//      Serial.println("shouldve made it");
      resetValues();
      State = Stopped; // här går v2 till folow line
      break;

    case FollowLine:
      if (getRPM()) {
//        Serial.println("Speed " + String(Speed) + " RequestedRPM " + String(RequestedRPM) + " DistanceDriven " + String(DistanceDriven));
        analogWrite(Pw, Speed);
        SteeringServo.write(V);
      }
      //      Serial.println(DistanceDriven);
      if (RevRoadDist >= roadDist) {
        sendJSON(String("[\"" + OWNER + "\",\"HasDriven\"]")); // denna är bortkommenterad i v2
        State = Stopped;
        break;
      }
      State = FollowLine;
      break;
  }
}

// Funktioner som kontrolerar hastigheten //
float proportionellTerm() {
//  Serial.println(String(Kp)+" "+String(e));
  return (float) Kp * e;
}

float integrationTerm() {
//  Serial.println(String(Ki)+" "+String(KiArea));
  return (float) Ki * KiArea;
}

float deriveringTerm() {
  return 0;
}

void calculateTerms() {
  pt = proportionellTerm();
  it = integrationTerm();
  dt = deriveringTerm();
  Speed = speedControll(pt + it + dt);
//  Serial.println(Speed);

//  Serial.println(String(pt) + " " + String(it) + " " + String(dt) + " " + String(currentMillis) + " " + String(previousMillis) + " " + String(RPM));
}

int speedControll(float Speed) {
  if (Speed > maxPwm) {
    return maxPwm;
  } else if (Speed < minPwm) {
    return minPwm;
  } else {
    return int(Speed + 0.5);
  }
}

boolean getRPM() {
  currentMillis = millis();
  if (SerialData()) {
    rads = V * PI / 180;
    RequestedRPM = (float) (RequestedRPMBase * sin(rads) * (1 - SpeedC) + SpeedC);

    RPM = (float) Rev / 96 / (currentMillis - previousMillis + 1) * 60000;

    e = (float) RequestedRPM - RPM;
    KiArea += (float) float(currentMillis - previousMillis) / 1000 * e;
//    Serial.print(String(RequestedRPM)+" "+String(RPM)+" "+String(Speed)+" || ");
    Serial.println(String(RevRoadDist)+" | "+String(currentMillis - previousMillis)+" | "+String(Rev)+" || "+String(sin(rads))+" | "+String(RPM)+" | "+String(RequestedRPM)+" | "+String(Speed));


/*
    Serial.println("speedToRpm "+String(speedToRpm));
    Serial.println("RequestedRPM "+String(RequestedRPM));

    
    Serial.println("V "+String(V));
    Serial.println("rads "+String(rads));
    Serial.println("maxPwm "+String(maxPwm));
    Serial.println("sin(rads) "+String(sin(rads)));
    Serial.println("1-SpeedC "+String(1-SpeedC));
    Serial.println("SpeedC "+String(SpeedC));
    Serial.println("speedToRpm "+String(speedToRpm));
    Serial.println("RequestedRPM "+String(RequestedRPM));
    Serial.println("Rev "+String(Rev));
    Serial.println("currentMillis - previousMillis "+String(currentMillis - previousMillis));
    Serial.println("RPM "+String(RPM));
    Serial.println("e "+String(e));
    Serial.println("KiArea "+String(KiArea));
*/
    if (SerialDataBufferRPM <= 0) {
      SerialDataBufferRPM = SerialDataBufferRPMStart;
      previousMillis = currentMillis;
      Rev = 0;
      Serial.println(" ");
    } else {
      SerialDataBufferRPM--;
    }
//    previousMillis = currentMillis;
    calculateTerms();
    
/*    Serial.println("pt "+String(pt));
    Serial.println("it "+String(it));
    Serial.println("dt"+String(dt));
    Serial.println("Speed "+String(Speed));*/

    return true;
  }
  return false;
}

// Funktion "Skicka data" //
void sendJSON(String JSON) {
  client.publish("simon.ogaardjozic@abbindustrigymnasium.se/Scavenger", JSON);
}

// Funktion "resetValues" //
void resetValues() {
  for (int i = 0; i < 4; i++) {
    matrix[i] = 0;
  }

  KiArea = 0;  
  SerialDataBufferRPM = SerialDataBufferRPMStart;
  Rev = 0;
  RevRoadDist = 0;
  previousMillis = millis();
  currentMillis = millis();
}

boolean SendSerialData() {
  Serial.write("[Observe]");
  delay(125);
  return true;
}

// Funktion "SerialData" //
boolean SerialData() {
  while (Serial.available()) {
    delay(1);
    c = Serial.read();
    readString += c;
  }
  if (readString.length()) {
//    Serial.print(readString);
//    Serial.print(readString.startsWith("[["));
    if (readString.startsWith("[[")){
//    if (readString.startsWith("[[", 0)){
      StaticJsonBuffer<256> jsonBuffer;
      JsonArray& obj = jsonBuffer.parseArray(readString);
      objSize = obj.size();
//      Serial.print(objSize); // skrev ut 03???? why
      for (int i = 0; i < 4; i++) {
        matrix[i] += obj[0][i].as<int>();
      }
      if (objSize == 3) {
        V = abs(obj[1].as<int>()-180);
        X = obj[2].as<int>();
      } else {
        obstacle[0] = obj[1][0].as<int>();
        obstacle[1] = obj[1][1].as<int>();
      }
//      Serial.println(readString);
      readString = "";
      return true;
    }
  }
  readString = "";
  return false;
}

// Funktion "Blink" //
void Blink(int BlinkTime) {
  analogWrite(Pw, 0);
  currentMillis = millis();
  if (currentMillis > previousMillis + BlinkTime) {
    previousMillis = currentMillis;
    LedState = !LedState;
    digitalWrite(LED_BUILTIN, LedState);
  }
}

// Funktion "Interupt" //
ICACHE_RAM_ATTR void HtoL() {
  Rev++;
  RevRoadDist++;
}
