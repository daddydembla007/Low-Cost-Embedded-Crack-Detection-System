// ── Pin Definitions ──
#define LCD_RD     3
#define DATA_START 4   // D0=4, D1=5, D2=6, D3=7, D4=8, D5=9, D6=10, D7=11
#define LCD_RS     12  
#define LCD_WR     13
#define LCD_CS     14
#define LCD_RST    15

#define WIDTH       240
#define HEIGHT      320

// Basic Colors
#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// ── 8-bit Bus Write ──
void writeBus(uint8_t d) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(DATA_START + i, (d >> i) & 1);
  }
  // Micro-delays for the SN74HC245 chips to process
  delayMicroseconds(2); 
  digitalWrite(LCD_WR, LOW);
  delayMicroseconds(2); 
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

// ── Set Drawing Area ──
void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  writeReg(0x0050, x0); 
  writeReg(0x0051, x1); 
  writeReg(0x0052, y0); 
  writeReg(0x0053, y1); 
  writeReg(0x0020, x0); 
  writeReg(0x0021, y0); 
  writeCmd16(0x0022);   
}

// ── ILI9325 Initialization Sequence ──
void tftInit() {
  for (int i = 3; i <= 15; i++) pinMode(i, OUTPUT);

  digitalWrite(LCD_RD, HIGH);
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(LCD_WR, HIGH);
  
  digitalWrite(LCD_RST, HIGH); delay(50);
  digitalWrite(LCD_RST, LOW);  delay(100);
  digitalWrite(LCD_RST, HIGH); delay(150);
  
  // Turn on oscillator
  writeReg(0x00E3, 0x3008); 
  writeReg(0x00E7, 0x0012); 
  writeReg(0x00EF, 0x1231); 
  
  writeReg(0x0001, 0x0100); 
  writeReg(0x0002, 0x0700); 
  writeReg(0x0003, 0x1030); // Entry mode
  writeReg(0x0004, 0x0000); 
  writeReg(0x0008, 0x0207); 
  writeReg(0x0009, 0x0000); 
  writeReg(0x000A, 0x0000); 
  writeReg(0x000C, 0x0000); 
  writeReg(0x000D, 0x0000); 
  writeReg(0x000F, 0x0000); 
  
  // Power On Sequence
  writeReg(0x0010, 0x0000); 
  writeReg(0x0011, 0x0007); 
  writeReg(0x0012, 0x0000); 
  writeReg(0x0013, 0x0000); 
  delay(200); 
  writeReg(0x0010, 0x1690); 
  writeReg(0x0011, 0x0227); 
  delay(50);
  writeReg(0x0012, 0x000D); 
  delay(50);
  writeReg(0x0013, 0x1200); 
  writeReg(0x0029, 0x000A); 
  writeReg(0x002B, 0x000C); 
  delay(50);
  
  // Gate Scan Line (Crucial to fix the "strip" issue)
  writeReg(0x0060, 0x2700); 
  writeReg(0x0061, 0x0001); 
  writeReg(0x006A, 0x0000); 
  
  // Display ON
  writeReg(0x0090, 0x0010);
  writeReg(0x0092, 0x0000);
  writeReg(0x0007, 0x0133); 
}

void fillScreen(uint16_t color) {
  setAddrWindow(0, 0, WIDTH - 1, HEIGHT - 1);
  digitalWrite(LCD_RS, HIGH);
  digitalWrite(LCD_CS, LOW);
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;
  for (uint32_t i = 0; i < (uint32_t)WIDTH * HEIGHT; i++) {
    writeBus(hi);
    writeBus(lo);
  }
  digitalWrite(LCD_CS, HIGH);
}

void setup() {
  tftInit();
}

void loop() {
  fillScreen(RED);    delay(1000);
  fillScreen(GREEN);  delay(1000);
  fillScreen(BLUE);   delay(1000);
  fillScreen(WHITE);  delay(1000);
}