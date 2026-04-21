#include "Arduino_RouterBridge.h"

const int interruptPinOut = 2; // Signal to STM32
const int buttonPin = 4;       // Physical Push Button

bool buttonPressedFlag = false;
bool lastButtonState = HIGH;   
bool confirmedState = HIGH;    
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Store the latest data from STM32
String boundaryData = "0,0,0,0"; 
String currentData = "0,0";

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); 

    pinMode(interruptPinOut, OUTPUT);
    digitalWrite(interruptPinOut, LOW); 

    pinMode(buttonPin, INPUT_PULLUP);

    // Initialize hardware serial to listen to STM32 (match STM32's 115200 baud rate)
    Serial.begin(115200); 

    Bridge.begin(); 
    Bridge.provide("set_led_state", set_led_state);
    Bridge.provide("trigger_stm32", trigger_stm32); 
    Bridge.provide("check_button", check_button);      
    Bridge.provide("get_boundaries", get_boundaries);
    Bridge.provide("get_current", get_current);
}

void loop() {
    // 1. Button Debounce Logic
    bool reading = digitalRead(buttonPin);
    if (reading != lastButtonState) { 
        lastDebounceTime = millis(); 
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != confirmedState) {
            confirmedState = reading; 
            if (confirmedState == LOW) { 
                buttonPressedFlag = true; 
            }
        }
    }
    lastButtonState = reading;

    // 2. Read UART Data from STM32
    if (Serial.available()) {
        String incoming = Serial.readStringUntil('\n');
        incoming.trim();
        if (incoming.startsWith("DATA:")) {
            boundaryData = incoming.substring(5); // Save L,R,B,T
        } else if (incoming.startsWith("CUR:")) {
            currentData = incoming.substring(4);  // Save X,Y
        }
    }
}

// --- Bridge Functions ---
void set_led_state(bool state) { digitalWrite(LED_BUILTIN, state ? LOW : HIGH); }

void trigger_stm32() {
    digitalWrite(interruptPinOut, HIGH);
    delay(10); 
    digitalWrite(interruptPinOut, LOW);
}

bool check_button() {
    if (buttonPressedFlag) { 
        buttonPressedFlag = false; 
        return true; 
    }
    return false;
}

String get_boundaries() { return boundaryData; }
String get_current() { return currentData; }