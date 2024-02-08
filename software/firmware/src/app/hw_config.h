typedef enum {
    PIN_NUM_CHG_IOX_RST  = 0,
    PIN_NUM_CHG_IOX_INT  = 1,
    PIN_NUM_MTR_RL_SPEED = 2,
    PIN_NUM_MTR_RR_SPEED = 3,
    PIN_NUM_MTR_FR_SPEED = 4,
    PIN_NUM_MISC_IOX_RST = 5,
    PIN_NUM_HP_SDA       = 6,
    PIN_NUM_HP_SCL       = 7,
    PIN_NUM_LP_SDA       = 8,
    PIN_NUM_LP_SCL       = 9,
    PIN_NUM_MTR_FL_SPEED = 10,
    PIN_NUM_MISC_IOX_INT = 11,
    PIN_NUM_SX_MISO      = 12,
    PIN_NUM_SX_CS        = 13,
    PIN_NUM_SX_SCK       = 14,
    PIN_NUM_SX_MOSI      = 15,
    PIN_NUM_SX_BUSY      = 16,
    PIN_NUM_BUZZ_CTRL    = 17,
    PIN_NUM_IMU_MISO     = 20,
    PIN_NUM_IMU_CS       = 21,
    PIN_NUM_IMU_SCK      = 22,
    PIN_NUM_IMU_MOSI     = 23,
    PIN_NUM_GPS_RX       = 24,
    PIN_NUM_GPS_TX       = 25,
    PIN_NUM_MOTOR_CS     = 26,
    PIN_NUM_LV_CS        = 27,
    PIN_NUM_VIDEO_RX     = 28,
    PIN_NUM_VIDEO_TX     = 29,
} pin_num_t;

typedef enum {
    // LP
    I2C_ADDRESS_QI_TEMP    = 0x48,
    I2C_ADDRESS_LED_DRIVER = 0x50,
    I2C_ADDRESS_MISC_IOX   = 0x74,
    I2C_ADDRESS_CHG_IOX    = 0x75,

    // HP
    I2C_ADDRESS_HP_TEMP = 0x48,
    I2C_ADDRESS_MTR_IOX = 0x74,
    I2C_ADDRESS_PWR_IOX = 0x75,
    I2C_ADDRESS_BMS_MODELGAUGE = 0x36,
    I2C_ADDRESS_BMS_NONVOLATILE = 0x0B,
    I2C_ADDRESS_MTR_FL = 0x60,
    I2C_ADDRESS_MTR_RR = 0x61,
    I2C_ADDRESS_MTR_RL = 0x62,
    I2C_ADDRESS_MTR_FR = 0x63,
    I2C_ADDRESS_CHG_CONT = 0x6B,
} i2c_addr_t;

#define IMU_SPI      spi0
#define SX_SPI       spi1
#define IMU_SPI_FREQ 1000000U
#define SX_SPI_FREQ  1000000U

#define VIDEO_UART           uart0
#define GPS_UART             uart1
#define VIDEO_UART_BAUD_RATE 9600U
#define GPS_UART_BAUD_RATE   9600U

#define LP_I2C i2c0
#define HP_I2C i2c1

void hw_init(void);
