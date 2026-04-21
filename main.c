/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Dual HC-SR04 Boundary Measurement + I2C LCD Display + Vega Comms + UNO Q Interrupt
  *
  * SYSCLK = 168 MHz | APB1 timer clock = 84 MHz
  *
  * SENSOR 1 (Horizontal) TRIG=PC6  ECHO=PC7
  * SENSOR 2 (Vertical)   TRIG=PB12 ECHO=PB5
  *
  * BUTTON: PA3 (Falling-edge EXTI, internal Pull-Up)
  *
  * LEDs: LD1 Green PB0 | LD2 Blue PB7 | LD3 Red PB14
  *
  * LCD 16x2 I2C: SCL=PB8  SDA=PB9
  *
  * COMM: USART3 (PC) = Default ST-Link VCP
  * USART6 (Vega) = PG14 (TX) -> Connect this to Vega RX
  *
  * INTERRUPT: D2 (PF15) -> Connect this to UNO Q D2
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <string.h>
#include <stdio.h>

/* ── Handles ─────────────────────────────────────────────────────────────── */
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6; /* UART6 for Vega Aries */
PCD_HandleTypeDef  hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */
TIM_HandleTypeDef htim3;

/* Boundary distances in cm. -1 = not yet captured */
static float dist_left  = -1.0f;
static float dist_right = -1.0f;
static float dist_bot   = -1.0f;
static float dist_top   = -1.0f;

static volatile uint8_t systemMode     = 0;
static volatile uint8_t flag_doRead    = 0;
static volatile uint8_t flag_lcdRefresh= 0;
static volatile uint8_t flag_crack_detected = 0; /* NEW: Flag for UNO Q Interrupt */
/* USER CODE END PV */

/* ── Defines ─────────────────────────────────────────────────────────────── */
#define DEBOUNCE_MS       50
#define BTN_EXT_Pin       GPIO_PIN_3
#define BTN_EXT_GPIO_Port GPIOA

/* ── Prototypes ──────────────────────────────────────────────────────────── */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */
static void  MX_TIM3_Init(void);
static float HC_SR04_Read(GPIO_TypeDef *tPort, uint16_t tPin, GPIO_TypeDef *ePort, uint16_t ePin);
static void  LED_Flash(uint16_t pin);
static void  EnterMode(uint8_t mode);
static void  DoSensorReadAndLodge(void);

#define LCD_ADDR      (0x27 << 1)
#define LCD_BACKLIGHT  0x08
static void lcd_send_cmd(char cmd);
static void lcd_send_data(char data);
static void lcd_send_string(char *str);
static void lcd_put_cur(int row, int col);
static void lcd_clear(void);
static void lcd_init(void);
static void lcd_update(void);
/* USER CODE END PFP */

/* ═══════════════════════════════════════════════════════════════════════════
   USER CODE BEGIN 0
   ═══════════════════════════════════════════════════════════════════════════ */

int _write(int file, char *ptr, int len)
{
    /* Keep printf mapped to PC (USART3) */
    HAL_UART_Transmit(&huart3, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* Button on PA3 */
    if (GPIO_Pin == BTN_EXT_Pin)
    {
        static uint32_t lastPressTime = 0;
        uint32_t now = HAL_GetTick();

        if ((now - lastPressTime) > DEBOUNCE_MS)
        {
            lastPressTime = now;
            flag_doRead = 1;
        }
    }

    /* NEW: Listen for the Arduino UNO Q on D2 (PF15) */
    if (GPIO_Pin == GPIO_PIN_15)
    {
        flag_crack_detected = 1; /* Tell the main loop to flash the LEDs */
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    static uint8_t ticks = 0;
    if (htim->Instance == TIM3)
    {
        if (++ticks >= 10) {
            ticks = 0;
            flag_lcdRefresh = 1;
        }
    }
}

static float HC_SR04_Read(GPIO_TypeDef *tPort, uint16_t tPin, GPIO_TypeDef *ePort, uint16_t ePin)
{
    uint32_t t;

    HAL_GPIO_WritePin(tPort, tPin, GPIO_PIN_RESET);
    t = __HAL_TIM_GET_COUNTER(&htim2);
    while ((__HAL_TIM_GET_COUNTER(&htim2) - t) < 5U);

    HAL_GPIO_WritePin(tPort, tPin, GPIO_PIN_SET);
    t = __HAL_TIM_GET_COUNTER(&htim2);
    while ((__HAL_TIM_GET_COUNTER(&htim2) - t) < 10U);
    HAL_GPIO_WritePin(tPort, tPin, GPIO_PIN_RESET);

    uint32_t deadline = HAL_GetTick() + 30UL;
    while (HAL_GPIO_ReadPin(ePort, ePin) == GPIO_PIN_RESET)
        if (HAL_GetTick() > deadline) return -1.0f;

    uint32_t startUs = __HAL_TIM_GET_COUNTER(&htim2);

    deadline = HAL_GetTick() + 30UL;
    while (HAL_GPIO_ReadPin(ePort, ePin) == GPIO_PIN_SET)
        if (HAL_GetTick() > deadline) return -1.0f;

    return (float)(__HAL_TIM_GET_COUNTER(&htim2) - startUs) / 58.0f;
}

static void LED_Flash(uint16_t pin)
{
    for (int i = 0; i < 2; i++) {
        HAL_GPIO_WritePin(GPIOB, pin, GPIO_PIN_SET);
        HAL_Delay(150);
        HAL_GPIO_WritePin(GPIOB, pin, GPIO_PIN_RESET);
        HAL_Delay(100);
    }
}

static void lcd_send_cmd(char cmd)
{
    uint8_t buf[4];
    buf[0] = (cmd  & 0xF0)        | 0x0C | LCD_BACKLIGHT;
    buf[1] = (cmd  & 0xF0)        | 0x08 | LCD_BACKLIGHT;
    buf[2] = ((cmd << 4) & 0xF0)  | 0x0C | LCD_BACKLIGHT;
    buf[3] = ((cmd << 4) & 0xF0)  | 0x08 | LCD_BACKLIGHT;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, buf, 4, 100);
}

static void lcd_send_data(char data)
{
    uint8_t buf[4];
    buf[0] = (data  & 0xF0)        | 0x0D | LCD_BACKLIGHT;
    buf[1] = (data  & 0xF0)        | 0x09 | LCD_BACKLIGHT;
    buf[2] = ((data << 4) & 0xF0)  | 0x0D | LCD_BACKLIGHT;
    buf[3] = ((data << 4) & 0xF0)  | 0x09 | LCD_BACKLIGHT;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, buf, 4, 100);
}

static void lcd_put_cur(int row, int col)
{
    lcd_send_cmd((row == 0 ? 0x80 : 0xC0) | col);
}

static void lcd_send_string(char *str)
{
    while (*str) lcd_send_data(*str++);
}

static void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

static void lcd_init(void)
{
    HAL_Delay(50);
    lcd_send_cmd(0x30); HAL_Delay(5);
    lcd_send_cmd(0x30); HAL_Delay(1);
    lcd_send_cmd(0x30); HAL_Delay(10);
    lcd_send_cmd(0x20);
    lcd_send_cmd(0x28);
    lcd_send_cmd(0x0C);
    lcd_send_cmd(0x06);
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

static void lcd_update(void)
{
    char row0[17], row1[17];
    char lS[7], rS[7], bS[7], tS[7];

    #define FMT(buf, val) \
        if ((val) < 0) snprintf(buf, 7, "--.-"); \
        else snprintf(buf, 7, "%d.%d", (int)(val), \
                      (int)(((val) - (int)(val)) * 10.0f))

    FMT(lS, dist_left);
    FMT(rS, dist_right);
    FMT(bS, dist_bot);
    FMT(tS, dist_top);

    snprintf(row0, 17, "L:%-4s  R:%-4s", lS, rS);
    snprintf(row1, 17, "B:%-4s  T:%-4s", bS, tS);

    for (int i = strlen(row0); i < 16; i++) row0[i] = ' '; row0[16] = '\0';
    for (int i = strlen(row1); i < 16; i++) row1[i] = ' '; row1[16] = '\0';

    lcd_put_cur(0, 0); lcd_send_string(row0);
    lcd_put_cur(1, 0); lcd_send_string(row1);
}

static void EnterMode(uint8_t mode)
{
    systemMode = mode;
    HAL_GPIO_WritePin(GPIOB, LD1_Pin, (mode == 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);

    switch (mode) {
        case 0:
            printf("\r\n[MODE 0] IDLE — LD1 OFF\r\nPress button (PA3) to begin session.\r\n\r\n");
            break;
        case 1:
            printf("\r\n[MODE 1] Sensor 1 → LEFT.    Press button to lodge.\r\n");
            break;
        case 2:
            printf("\r\n[MODE 2] Sensor 1 → RIGHT.   Press button to lodge.\r\n");
            break;
        case 3:
            printf("\r\n[MODE 3] Sensor 2 → BOTTOM.  Press button to lodge.\r\n");
            break;
        case 4:
            printf("\r\n[MODE 4] Sensor 2 → TOP.     Press button to lodge.\r\n");
            break;
    }
    lcd_update();
}

static void DoSensorReadAndLodge(void)
{
    float reading = -1.0f;
    char vegaTxBuf[64]; /* Buffer for transmitting to Vega */

    switch (systemMode)
    {
        case 0:
            dist_left = dist_right = dist_bot = dist_top = -1.0f;
            EnterMode(1);
            break;

        case 1:
            reading = HC_SR04_Read(GPIOC, GPIO_PIN_6, GPIOC, GPIO_PIN_7);
            if (reading > 0.0f) {
                dist_left = reading;
                printf("[MODE 1] LEFT lodged: %d.%d cm\r\n", (int)dist_left, (int)((dist_left - (int)dist_left) * 10));
                LED_Flash(LD2_Pin);
                EnterMode(2);
            } else { printf("Timeout\r\n"); }
            break;

        case 2:
            reading = HC_SR04_Read(GPIOC, GPIO_PIN_6, GPIOC, GPIO_PIN_7);
            if (reading > 0.0f) {
                dist_right = reading;
                printf("[MODE 2] RIGHT lodged: %d.%d cm\r\n", (int)dist_right, (int)((dist_right - (int)dist_right) * 10));
                LED_Flash(LD2_Pin);
                EnterMode(3);
            } else { printf("Timeout\r\n"); }
            break;

        case 3:
            reading = HC_SR04_Read(GPIOB, GPIO_PIN_12, GPIOB, GPIO_PIN_5);
            if (reading > 0.0f) {
                dist_bot = reading;
                printf("[MODE 3] BOTTOM lodged: %d.%d cm\r\n", (int)dist_bot, (int)((dist_bot - (int)dist_bot) * 10));
                LED_Flash(LD2_Pin);
                EnterMode(4);
            } else { printf("Timeout\r\n"); }
            break;

        case 4:
            reading = HC_SR04_Read(GPIOB, GPIO_PIN_12, GPIOB, GPIO_PIN_5);
            if (reading > 0.0f) {
                dist_top = reading;
                printf("[MODE 4] TOP lodged: %d.%d cm\r\n", (int)dist_top, (int)((dist_top - (int)dist_top) * 10));
                LED_Flash(LD2_Pin);

                float width  = dist_right - dist_left;
                float height = dist_top   - dist_bot;

                /* Print to PC */
                printf("\r\n==========================================\r\n");
                printf("  CAPTURE COMPLETE\r\n");
                printf("  Width  = %d.%d cm\r\n", (int)width,  (int)((width  - (int)width)  * 10));
                printf("  Height = %d.%d cm\r\n", (int)height, (int)((height - (int)height) * 10));
                printf("==========================================\r\n");

                /* Transmit structured data to Vega via USART6 (PG14) */
                int len = snprintf(vegaTxBuf, sizeof(vegaTxBuf), "DATA:%d.%d,%d.%d,%d.%d,%d.%d\n",
                    (int)dist_left,  (int)((dist_left  - (int)dist_left)  * 10),
                    (int)dist_right, (int)((dist_right - (int)dist_right) * 10),
                    (int)dist_bot,   (int)((dist_bot   - (int)dist_bot)   * 10),
                    (int)dist_top,   (int)((dist_top   - (int)dist_top)   * 10));

                HAL_UART_Transmit(&huart6, (uint8_t*)vegaTxBuf, len, 100);

                EnterMode(0);
            } else { printf("Timeout\r\n"); }
            break;
    }
}
/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART3_UART_Init();
    MX_USART6_UART_Init();
    MX_USB_OTG_FS_PCD_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();

    /* Set up Interrupt priorities and enable them */
    HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    /* NEW: Enable Interrupts for Pins 10 through 15 (Catches PF15 from UNO Q) */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    HAL_TIM_Base_Start(&htim2);
    HAL_TIM_Base_Start_IT(&htim3);

    HAL_GPIO_WritePin(GPIOB, LD1_Pin | LD2_Pin | LD3_Pin, GPIO_PIN_RESET);

    lcd_init();
    lcd_clear();

    printf("==========================================\r\n");
    printf("  Dual HC-SR04 Boundary System Ready      \r\n");
    printf("  USART6 (PG14) -> Vega Aries TX Active   \r\n");
    printf("  Listening for UNO Q Interrupt on D2     \r\n");
    printf("==========================================\r\n");

    EnterMode(0);

    while (1)
    {
        if (flag_lcdRefresh) {
            flag_lcdRefresh = 0;
            lcd_update();
        }

        if (flag_doRead) {
            flag_doRead = 0;
            DoSensorReadAndLodge();
        }

        /* NEW: Handle the UNO Q Interrupt */
        if (flag_crack_detected) {
            flag_crack_detected = 0; /* Reset flag */
            printf("\r\n>>> INTERRUPT RECEIVED FROM UNO Q! Crack Detected! <<<\r\n");

            /* Flash all 3 onboard LEDs (Green, Blue, Red) for ~3 seconds */
            /* 15 loops * 200ms (100 ON + 100 OFF) = 3000ms */
            for (int i = 0; i < 15; i++) {
                HAL_GPIO_TogglePin(GPIOB, LD1_Pin | LD2_Pin | LD3_Pin);
                HAL_Delay(100);
                HAL_GPIO_TogglePin(GPIOB, LD1_Pin | LD2_Pin | LD3_Pin);
                HAL_Delay(100);
            }

            /* Ensure they turn off safely when finished */
            HAL_GPIO_WritePin(GPIOB, LD1_Pin | LD2_Pin | LD3_Pin, GPIO_PIN_RESET);

            /* Re-turn on LD1 if the system mode is currently active (Mode 1-4) */
            if(systemMode != 0) {
                 HAL_GPIO_WritePin(GPIOB, LD1_Pin, GPIO_PIN_SET);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Peripheral Init Functions
   ═══════════════════════════════════════════════════════════════════════════ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 4;
    RCC_OscInitStruct.PLL.PLLN       = 168;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

static void MX_USART6_UART_Init(void)
{
    __HAL_RCC_USART6_CLK_ENABLE();

    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = 115200;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void) {
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 83;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 4294967295UL;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
}

static void MX_TIM3_Init(void) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 8400 - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 1000 - 1;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(TIM3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

static void MX_USART3_UART_Init(void) {
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
}

static void MX_USB_OTG_FS_PCD_Init(void) {}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE(); /* NEW: Enable Clock for Port F */

    /* Original Pins Configs */
    HAL_GPIO_WritePin(GPIOB, LD1_Pin | LD3_Pin | GPIO_PIN_12 | LD2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin  = BTN_EXT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BTN_EXT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_4 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = LD1_Pin | LD3_Pin | GPIO_PIN_12 | LD2_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_5;
    GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Setup PG14 as the Hardware TX pin for USART6 (To Vega Aries) */
    GPIO_InitStruct.Pin       = GPIO_PIN_14;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* NEW: Setup D2 (PF15) as an Interrupt Input from UNO Q */
    GPIO_InitStruct.Pin       = GPIO_PIN_15;
    GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING; /* Triggers on HIGH pulse */
    GPIO_InitStruct.Pull      = GPIO_PULLDOWN;       /* Keeps it stable when unplugged */
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

void EXTI3_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(BTN_EXT_Pin); }

/* NEW: Handler to catch the PF15 (D2) interrupt */
void EXTI15_10_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}

void TIM3_IRQHandler(void)  { HAL_TIM_IRQHandler(&htim3);            }

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
