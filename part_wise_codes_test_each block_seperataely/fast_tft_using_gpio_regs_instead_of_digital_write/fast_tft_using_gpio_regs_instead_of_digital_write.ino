#define SERIAL_RX_BUFFER_SIZE 512

// ══════════════════════════════════════════════════════
// THEJAS32 TRUE GPIO — from VEGA SDK config.h
// GPIO_0: pins  0–15  base 0x10080000
// GPIO_1: pins 16–31  base 0x10180000
// Direction reg = base + 0x40000
// Pin address   = base | (bit << 2)
// ══════════════════════════════════════════════════════
#define GPIO_0_BASE  0x10080000UL
#define GPIO_1_BASE  0x10180000UL
#define GPIO_0_DIR   (*(volatile uint16_t*)(GPIO_0_BASE + 0x40000UL))
#define GPIO_1_DIR   (*(volatile uint16_t*)(GPIO_1_BASE + 0x40000UL))

// ── TFT pins ──────────────────────────────────────────
// DB0 –DB11 = GPIO4 –GPIO15 → GPIO_0 bank bits 4–15
// DB12–DB15 = GPIO16–GPIO19 → GPIO_1 bank bits 0–3
#define RS   27   // GPIO_1 bit 11
#define WR   28   // GPIO_1 bit 12
#define CS   29   // GPIO_1 bit 13
#define RST  30   // GPIO_1 bit 14

#define BAUD        500000
#define WIDTH       240
#define HEIGHT      320
#define FRAME_BYTES (WIDTH * HEIGHT * 2UL)   // 153600

// ── Precomputed pin addresses (set once, reuse forever) ──
static volatile uint16_t* DB[16];   // DB0..DB15 pin addresses
static volatile uint16_t* WR_ADDR;
static volatile uint16_t* RS_ADDR;
static volatile uint16_t* CS_ADDR;
static volatile uint16_t* RST_ADDR;
static uint16_t DB_BIT[16];         // bit value for each DB pin
static uint16_t WR_BIT, RS_BIT, CS_BIT, RST_BIT;

void initPins() {
  // ── Direction: GPIO_0 bits 4–15 as output ──
  GPIO_0_DIR |= 0xFFF0;
  __asm__ __volatile__ ("fence");

  // ── Direction: GPIO_1 bits 0–3 (DB12–15) + 11–14 (RS/WR/CS/RST) ──
  GPIO_1_DIR |= 0x7800 | 0x000F;   // bits 11,12,13,14 + bits 0,1,2,3
  __asm__ __volatile__ ("fence");

  // ── Precompute DB addresses for GPIO_0 (DB0–DB11 = GPIO4–15) ──
  for (int i = 0; i < 12; i++) {
    uint16_t bit = (1 << (i + 4));
    DB_BIT[i] = bit;
    DB[i] = (volatile uint16_t*)(GPIO_0_BASE | ((uint32_t)bit << 2));
  }
  // ── Precompute DB addresses for GPIO_1 (DB12–DB15 = GPIO16–19) ──
  for (int i = 0; i < 4; i++) {
    uint16_t bit = (1 << i);
    DB_BIT[12 + i] = bit;
    DB[12 + i] = (volatile uint16_t*)(GPIO_1_BASE | ((uint32_t)bit << 2));
  }

  // ── Control pin addresses ──
  WR_BIT  = (1 << 12); WR_ADDR  = (volatile uint16_t*)(GPIO_1_BASE | ((uint32_t)WR_BIT  << 2));
  RS_BIT  = (1 << 11); RS_ADDR  = (volatile uint16_t*)(GPIO_1_BASE | ((uint32_t)RS_BIT  << 2));
  CS_BIT  = (1 << 13); CS_ADDR  = (volatile uint16_t*)(GPIO_1_BASE | ((uint32_t)CS_BIT  << 2));
  RST_BIT = (1 << 14); RST_ADDR = (volatile uint16_t*)(GPIO_1_BASE | ((uint32_t)RST_BIT << 2));
}

// ── Write 16-bit value to data bus + pulse WR ─────────
inline void writeBus(uint16_t d) {
  // Write all 16 data bits
  for (int i = 0; i < 16; i++)
    *DB[i] = ((d >> i) & 1) ? DB_BIT[i] : 0;
  // Pulse WR
  *WR_ADDR = 0;
  __asm__ __volatile__ ("fence");
  *WR_ADDR = WR_BIT;
  __asm__ __volatile__ ("fence");
}

// ── Command / Data ────────────────────────────────────
inline void writeCommand(uint16_t cmd) {
  *RS_ADDR = 0;       // RS low
  *CS_ADDR = 0;       // CS low
  __asm__ __volatile__ ("fence");
  writeBus(cmd);
  *CS_ADDR = CS_BIT;  // CS high
  __asm__ __volatile__ ("fence");
}

inline void writeData(uint16_t data) {
  *RS_ADDR = RS_BIT;  // RS high
  *CS_ADDR = 0;       // CS low
  __asm__ __volatile__ ("fence");
  writeBus(data);
  *CS_ADDR = CS_BIT;  // CS high
  __asm__ __volatile__ ("fence");
}

// ── For bulk pixel writes: hold CS+RS steady ──────────
inline void startPixels() {
  *RS_ADDR = RS_BIT;
  *CS_ADDR = 0;
  __asm__ __volatile__ ("fence");
}
inline void endPixels() {
  *CS_ADDR = CS_BIT;
  __asm__ __volatile__ ("fence");
}
inline void writePixel(uint16_t d) {
  for (int i = 0; i < 16; i++)
    *DB[i] = ((d >> i) & 1) ? DB_BIT[i] : 0;
  *WR_ADDR = 0;
  __asm__ __volatile__ ("fence");
  *WR_ADDR = WR_BIT;
  __asm__ __volatile__ ("fence");
}

// ── Address window ────────────────────────────────────
void setAddrWindow() {
  writeCommand(0x2A);
  writeData(0x00); writeData(0x00);
  writeData(0x00); writeData(0xEF);
  writeCommand(0x2B);
  writeData(0x00); writeData(0x00);
  writeData(0x01); writeData(0x3F);
  writeCommand(0x2C);
}

// ── TFT init ──────────────────────────────────────────
void tftInit() {
  initPins();
  *CS_ADDR  = CS_BIT;
  *WR_ADDR  = WR_BIT;
  *RST_ADDR = RST_BIT;
  delay(10);
  *RST_ADDR = 0;       delay(100);
  *RST_ADDR = RST_BIT; delay(150);
  writeCommand(0x11);  delay(150);
  writeCommand(0x3A);  writeData(0x55);
  writeCommand(0x29);
}

// ── Receive one frame from serial ─────────────────────
void receiveFrame() {
  setAddrWindow();
  startPixels();
  uint32_t count = 0;

  while (count < FRAME_BYTES) {
    uint32_t t = millis();
    while (Serial.available() < 2) {
      if (millis() - t > 2000) { endPixels(); return; }
    }
    uint8_t hi = Serial.read();
    uint8_t lo = Serial.read();
    writePixel(((uint16_t)hi << 8) | lo);
    count += 2;
  }
  endPixels();
  Serial.write('K');   // ACK
}

// ── Setup ─────────────────────────────────────────────
void setup() {
  Serial.begin(BAUD);
  tftInit();

  // Blue screen = waiting
  setAddrWindow();
  startPixels();
  for (long i = 0; i < (long)WIDTH * HEIGHT; i++)
    writePixel(0x001F);
  endPixels();

  Serial.write('R');   // Ready signal to Python
}

// ── Loop ──────────────────────────────────────────────
void loop() {
  static uint8_t hbuf[4] = {0};
  if (Serial.available()) {
    hbuf[0] = hbuf[1]; hbuf[1] = hbuf[2];
    hbuf[2] = hbuf[3]; hbuf[3] = Serial.read();
    if (hbuf[0]==0xAB && hbuf[1]==0xCD &&
        hbuf[2]==0xAB && hbuf[3]==0xCD) {
      receiveFrame();
    }
  }
}