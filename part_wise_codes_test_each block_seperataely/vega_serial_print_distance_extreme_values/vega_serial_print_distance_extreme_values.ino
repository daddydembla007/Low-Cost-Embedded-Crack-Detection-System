#include <HardwareSerial.h>

/* * Vega Aries V3 - UART Receiver for STM32 Boundary Data
 * Manually instantiating UART1 and using char-by-char reading
 */

// Manually create the Serial1 object (UART port 1)
HardwareSerial Serial1(1);

// Global variables to store the distances
float vega_left  = 0.0;
float vega_right = 0.0;
float vega_bot   = 0.0;
float vega_top   = 0.0;

// String to accumulate incoming characters
String incomingData = ""; 

void setup() {
  // Start the PC serial monitor
  Serial.begin(115200);
  
  // Start the hardware UART to listen to the STM32
  Serial1.begin(115200); 

  Serial.println("Vega Aries V3 Initialized.");
  Serial.println("Waiting for boundary data from STM32...");
}

void loop() {
  // Check if STM32 is sending data
  while (Serial1.available() > 0) {
    
    // Read the incoming characters one by one
    char c = (char)Serial1.read();

    // If we hit a newline, the STM32 has finished sending the line
    if (c == '\n') {
      
      incomingData.trim(); // Clean up any invisible \r characters

      // Parse only if it starts with our specific tag
      if (incomingData.startsWith("DATA:")) {
        
        String dataOnly = incomingData.substring(5);

        int comma1 = dataOnly.indexOf(',');
        int comma2 = dataOnly.indexOf(',', comma1 + 1);
        int comma3 = dataOnly.indexOf(',', comma2 + 1);

        if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
          vega_left  = dataOnly.substring(0, comma1).toFloat();
          vega_right = dataOnly.substring(comma1 + 1, comma2).toFloat();
          vega_bot   = dataOnly.substring(comma2 + 1, comma3).toFloat();
          vega_top   = dataOnly.substring(comma3 + 1).toFloat();

          Serial.println("\n--- NEW BOUNDARY DATA LODGED ---");
          Serial.print("Left Distance:   "); Serial.print(vega_left);  Serial.println(" cm");
          Serial.print("Right Distance:  "); Serial.print(vega_right); Serial.println(" cm");
          Serial.print("Bottom Distance: "); Serial.print(vega_bot);   Serial.println(" cm");
          Serial.print("Top Distance:    "); Serial.print(vega_top);   Serial.println(" cm");
        }
      }
      
      // CRITICAL: Clear the buffer so we are ready for the next round of data!
      incomingData = "";
      
    } else {
      // If it's a normal character, add it to our string
      incomingData += c;
    }
  }
}