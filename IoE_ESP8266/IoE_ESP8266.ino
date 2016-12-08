// ESP8266 code for IoE project

#include <SoftwareSerial.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <Hash.h>

// Pins
const int LED_PIN = 5;
const int BUTTON_PIN = 14;
const int RELAY_PIN = 15;
const int SOFT_RX_PIN = 13;
const int SOFT_TX_PIN = 12;
const int BUFFER_OE_PIN = 4;

// Debounce duration in milliseconds
const int DB_COUNT = 50;

// States for state machine
enum STATE {POWER_ON, POWER_OFF};
enum STATE state;
enum BUTTON {DOWN, UP};
enum BUTTON button;

// String to receive over serial and send over websocket
String serialMessage = "";
bool endMessageFlag = false;

SoftwareSerial softSerial(SOFT_RX_PIN, SOFT_TX_PIN, false, 64); // Rx, Tx, inverted logic, buffer size
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

// Websocket event handler
void webSocketEvent(WStype_t type, uint8_t * payload, size_t lenght) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS]: Disconnected");
            break;
        case WStype_CONNECTED:
            Serial.println("[WS]: Connected");
            webSocket.sendTXT("Connected");
            break;
        case WStype_TEXT:
            if (((char)*payload) == '0'){
              Serial.println("[WS]: Received POWER OFF command");
              state = POWER_OFF;
            }
            else if (((char)*payload) == '1'){
              Serial.println("[WS]: Received POWER ON command");
              state = POWER_ON;
            }
            else {
              Serial.println("[WS]: Received unknown command");
            }
            break;
    }
}    

void setup() {
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUFFER_OE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  state = POWER_OFF;
  button = UP;
  
  digitalWrite(LED_PIN, HIGH);      // LED off
  digitalWrite(RELAY_PIN, LOW);     // Relay off
  digitalWrite(BUFFER_OE_PIN, LOW); // Buffer disabled
  
  // Initialize serial ports  
  Serial.begin(115200);
  softSerial.begin(9600);

  // Output boot message
  Serial.print("Booting.");
  for (short i=0; i<10; i++){
    Serial.print(".");
    delay(500);
  }
  Serial.println("");

  // Connect to Wi-Fi
  WiFiMulti.addAP("Cherlisnet", "Wireless1337");
  while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
    }

  // Connect to the websocket (need to check this address and port)
  webSocket.begin("192.168.0.41", 81);
  
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  // Check if we need to change state based on a button press
  if (digitalRead(BUTTON_PIN) == LOW){  // Check if button is pressed (down)
    if (button == UP){            // Check if it was not pressed on the last pass (UP), if so then debounce
      int dbStart = millis();
      bool dbComplete = false;
      while (digitalRead(BUTTON_PIN) == LOW){  // Check if the button stays low for DB_COUNT milliseconds
        if ((millis() - dbStart) > DB_COUNT){
          dbComplete = true;
          break;
        }
      }
      if (dbComplete == true){           // If the switch stayed low for DB_COUNT milliseconds then change state
        if (state == POWER_OFF) state = POWER_ON;
        else state = POWER_OFF;
        button = DOWN;
      }
    }
  }
  else{
    button = UP;   // Set button to UP if it was not pressed
  }

  // Check if we need to change state based on a websocket command
  webSocket.loop();  // TODO: need to set event handling to change power state
  
  // Add code to receive from soft serial and send out over websocket

  // State machine
  switch (state){
    case POWER_OFF:
      // Set the outputs
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(BUFFER_OE_PIN, LOW);
      break;
    case POWER_ON:
      // Set the outputs
      digitalWrite(LED_PIN, LOW);
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUFFER_OE_PIN, HIGH);
      // Check the soft serial port for new messages
      if (softSerial.available() > 0){
        //Serial.println("[SER]: Got a byte!");
        int incomingByte = softSerial.read();
        serialMessage += String((char)incomingByte);
        // If it is the last character in a message ("#")
        if (incomingByte == 35){
          //Serial.println("[SER]: Got a #!");
          endMessageFlag = true;
        }
      }
      // If the serial message is now complete
      if (endMessageFlag == true){
        webSocket.sendTXT(serialMessage);
        Serial.println(serialMessage);
        serialMessage = "";
        endMessageFlag = false;
      }
      break;
    default:
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(RELAY_PIN, LOW);
  }
  
  yield();  
}
