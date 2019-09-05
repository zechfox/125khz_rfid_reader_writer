/*
 * general configuration file
 *
 *
 */

#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_gpio.h"

//============================
//
//MCU configuratoin
//
//============================

//clock
#define RUN_MODE_CORE_CLOCK                               47972352U  /*!< Core clock frequency: 47972352Hz */
#define OSC_CAP0P                                         0U  /*!< Oscillator 0pF capacitor load */
#define OSC_ER_CLK_DISABLE                                0U  /*!< Disable external reference clock */


//GPIO
//bit mask for GPIO A output pins
//bit31 bit30 bit29 .... bit2 bit1 bit0
//pin31 pin30 pin29 .... pin2 pin1 pin0
#define GPIO_A_OUTPUT_PINS 0b00000000000000000000001000000000

#ifdef DBG_PWM_OUTPUT
// PORTB5 as output, which used output PMW as TPM1_CH1
#define GPIO_B_OUTPUT_PINS 0b00000000000000000010110000000000
#else
#define GPIO_B_OUTPUT_PINS 0b00000000000000000000110000000000
#endif

#define GPIO_A_INPUT_PINS 0b00000000000000000001000000000000
// PORTB12 as input pin
#define GPIO_B_INPUT_PINS 0b00000000000000000000000000000000

//SPI
/* PORTA5 (pin 9) is configured as SPI0_SS_b */
/* PORTA6 (pin 10) is configured as SPI0_MISO */
/* PORTA7 (pin 15) is configured as SPI0_MOSI */
/* PORTB0 (pin 16) is configured as SPI0_SCK */
#define USE_SPI
#define DEFAULT_SPI_MASTER (SPI0)
#define DEFAULT_SPI_MASTER_SOURCE_CLOCK (kCLOCK_BusClk)
#define DEFAULT_SPI_MASTER_CLK_FREQ CLOCK_GetFreq((kCLOCK_BusClk))


//UART
#define USE_UART0

#define UART_BAUDRATE 115200

#ifdef USE_UART0
#define UART0_DEBUG
#endif


//ADC


//I2C
/* PORTB3 (pin 23) is configured as I2C0_SCL */
/* PORTB4 (pin 24) is configured as I2C0_SDA */
#ifndef USE_I2C
#define USE_I2C
#define I2C_BASE_ADDR (I2C0)
#define I2C_PORT (PORTB)
#define I2C_GPIO_PORT (GPIOB)
#define I2C_BAUDRATE 100000U

#define I2C_SDA_PORT I2C_PORT 
#define I2C_SCL_PORT I2C_PORT
#define I2C_SDA_GPIO_PORT I2C_GPIO_PORT
#define I2C_SDA_PIN 4U
#define I2C_SCL_GPIO_PORT I2C_GPIO_PORT
#define I2C_SCL_PIN 3U
#define I2C_RELEASE_BUS_COUNT 100U

#endif

// TPM
/* PORTB13 (pin 21) is configured as TPM1 CH1 */
#ifndef USE_TPM
#define USE_TPM
// TPM port info
#define TPM_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_McgFllClk)
#define SOPT4_TPM1CH0SRC_TPM1         0x00u   /*!< TPM1 channel 0 input capture source select: TPM1_CH0 signal */
#define TPM1_CH1_PORT (PORTB)
// TPM pin info
// use pin index, not pin number of chip
#define TPM1_CH1_PIN_PORT (GPIOB)
#define TPM1_CH1_PIN_IDX (13U)

#define TPM1_CH1_CHANNEL1_FLAG kTPM_Chnl1Flag
#define TPM_CHANNEL1_INTERRUPT_ENABLE kTPM_Chnl1InterruptEnable

// TPM function info
#define TPM1_CH0 PMW
#define TPM1_CH1 PMW

#endif

//============================
//
//Devices configuration
//
//============================

//lcd

#define LCD_SPI (SPI0)
#define LCD_SPI_IRQ (SPI0_IRQn)
//gpio used for lcd must be set in GPIO section

//data/command pin, output, 0 for data, 1 for command
#define LCD_DC_GPIO_PORT GPIOB
#define LCD_DC_GPIO_PIN  10U

//chip enable pin, output, 1 for enable
#define LCD_CE_GPIO_PORT GPIOB
#define LCD_CE_GPIO_PIN  11U

//reset pin, output, ---__---, low level
#define LCD_RST_GPIO_PORT GPIOA
#define LCD_RST_GPIO_PIN 9U

// each charactor take 6X8 pixel
// display buffer same as pixel buffer 14X6(columnXrow) character
#define LCD_DISPLAY_BUFFER_COLUMN (14)
#define LCD_DISPLAY_BUFFER_ROW (6)

// rfid
// 1 tpm for transmit 
// 1 tpm for modulation 

// TPM1 use 1 channel for transmit carrier
#define RFID_TRANSMIT_TPM TPM1
#define RFID_TRNSIMIT_INTERRUPT_FLAG TPM1_CH1_CHANNEL1_FLAG
// TPM0 used for modulation
#define RFID_MODULE_TPM TPM0
#define RFID_CARRIER_FEQ 125000U

/* Interrupt number and interrupt handler for the RFID used */
#define RFID_RECEIVER_DATA_INTERRUPT_NUMBER PORTA_IRQn
//#define RFID_RECEIVER_DATA_HANDLER PORTA_IRQHandler
#define RFID_TRANSMITTER_INTERRUPT_NUMBER TPM1_IRQn
#define RFID_TRANSMITTER_INTERRUPT_ENABLE TPM_CHANNEL1_INTERRUPT_ENABLE
//#define RFID_TRANSMITTER_HANDLER TPM1_IRQHandler

// RFID receive pin
#define RFID_RECEIVER_DATA_PORT PORTA
#define RFID_RECEIVER_DATA_GPIO_PORT GPIOA
#define RFID_RECEIVER_DATA_GPIO_INDEX 12

