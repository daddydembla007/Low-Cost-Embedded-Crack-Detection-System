// ── Pin Definitions (Based on your working hardware) ──
#define LCD_RD      3
#define DATA_START  4   // D0=4...D7=11
#define LCD_RS      12  
#define LCD_WR      13
#define LCD_CS      14
#define LCD_RST     15

#define WIDTH       240
#define HEIGHT      320
#define BAUD        115200
#define FRAME_BYTES (WIDTH * HEIGHT * 2UL) 

// ── ILI9325 Working Driver Functions ──

void writeBus(uint8_t d) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(DATA_START + i, (d >> i) & 1);
  }
  delayMicroseconds(2); 
  digitalWrite(LCD_WR, LOW);
  delayMicroseconds(5); // The delay that fixed your "lines"
  digitalWrite(LCD_WR, HIGH);
  delayMicroseconds(2);
}

void writeCmd16(uint16_t cmd) {
  digitalWrite(LCD_RS, LOW);
  digitalWrite(LCD_CS, LOW);
  writeBus(cmd >> 8);   
  writeBus(cmd & 0xFF); 
  digitalWrite(LCD_CS, HIGH);
}

void writeData16(uint16_t data) {
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW);
  writeBus(data >> 8);
  writeBus(data & 0xFF);
  digitalWrite(LCD_CS, HIGH);
}

void writeReg(uint16_t reg, uint16_t val) {
  writeCmd16(reg);
  writeData16(val);
}

// ILI9325 specific window setting
void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  writeReg(0x0050, x0); 
  writeReg(0x0051, x1); 
  writeReg(0x0052, y0); 
  writeReg(0x0053, y1); 
  writeReg(0x0020, x0); 
  writeReg(0x0021, y0); 
  writeCmd16(0x0022); // Prepare RAM write
}

// ── Webcam Frame Reception ──

void receiveFrame() {
  // 1. Set the window to cover the whole screen
  setAddrWindow(0, 0, WIDTH - 1, HEIGHT - 1);
  
  uint32_t count = 0;
  digitalWrite(LCD_RS, HIGH); // Set to Data mode for the duration of the frame

  while (count < FRAME_BYTES) {
    // Wait for 2 bytes (1 pixel) to arrive
    if (Serial.available() >= 2) {
      uint8_t hi = Serial.read();
      uint8_t lo = Serial.read();

      // Write to LCD using your safe 8-bit method
      digitalWrite(LCD_CS, LOW);
      writeBus(hi);
      writeBus(lo);
      digitalWrite(LCD_CS, HIGH);
      
      count += 2;
    }
  }

  // Send ACK back to Python
  Serial.write('K');
}

// ── Initialization ──

void tftInit() {
  for (int i = 3; i <= 15; i++) pinMode(i, OUTPUT);
  digitalWrite(LCD_RD, HIGH);
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(LCD_WR, HIGH);
  
  digitalWrite(LCD_RST, HIGH); delay(100);
  digitalWrite(LCD_RST, LOW);  delay(100);
  digitalWrite(LCD_RST, HIGH); delay(150);
  
  // Power & Control Sequence (Same as your working concentric circle code)
  writeReg(0x00E3, 0x3008); writeReg(0x00E7, 0x0012); writeReg(0x00EF, 0x1231); 
  writeReg(0x0001, 0x0100); writeReg(0x0002, 0x0700); 
  writeReg(0x0003, 0x1030); // Entry mode
  writeReg(0x0010, 0x0000); writeReg(0x0011, 0x0007); delay(200);
  writeReg(0x0010, 0x1690); writeReg(0x0011, 0x0227); delay(100);
  writeReg(0x0012, 0x000D); delay(100);
  writeReg(0x0013, 0x1200); writeReg(0x0029, 0x000A); writeReg(0x002B, 0x000C); delay(100);
  writeReg(0x0060, 0xA700); writeReg(0x0061, 0x0001); 
  writeReg(0x0007, 0x0133); delay(50);
}

void setup() {
  Serial.begin(BAUD);
  tftInit();

  // Clear screen to blue while waiting
  setAddrWindow(0, 0, WIDTH-1, HEIGHT-1);
  for (long i = 0; i < (long)WIDTH * HEIGHT; i++) {
    writeData16(0x001F);
  }

  Serial.write('R'); // Signal Python we are ready
}

void loop() {
  // Sync header detection: 0xAB 0xCD 0xAB 0xCD
  static uint8_t hbuf[4] = {0};
  
  if (Serial.available()) {
    hbuf[0] = hbuf[1]; hbuf[1] = hbuf[2]; hbuf[2] = hbuf[3];
    hbuf[3] = Serial.read();
    
    if (hbuf[0] == 0xAB && hbuf[1] == 0xCD &&
        hbuf[2] == 0xAB && hbuf[3] == 0xCD) {
      receiveFrame();
    }
  }
}