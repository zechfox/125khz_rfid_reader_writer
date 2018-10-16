/*
 * mcu MKL02Z32VFM4
 *
 *
 */
#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "fsl_spi.h"
#include "fsl_smc.h"
#include "fsl_debug_console.h"
#include "fsl_lpsci.h"
#include "fsl_i2c.h"
#include "fsl_tpm.h"

#include "configuration.h"
#include "mcu_MKL02Z32VFM4.h"

i2c_master_handle_t g_i2c_master_handler;
volatile bool completionFlag = false;
volatile bool nakFlag = false;


static void CLOCK_CONFIG_FllStableDelay(void)
{
  uint32_t i = 30000U;
  while (i--)
  {
    __NOP();
  }
}
static void just_short_delay(void)
{
  uint32_t i = 0;
  for (i = 0; i < 100; i++)
  {
    __NOP();
  }
}

i2c_master_handle_t* get_i2c_handler()
{
  return &g_i2c_master_handler;
}

static void i2c_master_callback(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *userData)
{
  /* Signal transfer success when received success status. */
  if (status == kStatus_Success)
  {
    completionFlag = true;
  }
  /* Signal transfer success when received success status. */
  if ((status == kStatus_I2C_Nak) || (status == kStatus_I2C_Addr_Nak))
  {
    nakFlag = true;
  }

  PRINTF("i2c callback, status:%d \r\n", status);

}

bool initial_mcu(void)
{
  PRINTF("Start initial MCU. \r\n");
  if(!initial_run_clk())
  {
    return false;
  }

  if(!initial_gpio())
  {
    return false;
  }

  if(!initial_uart())
  {
    return false;
  }

  if(!initial_master_spi())
  {
    return false;
  }
  if(!initial_i2c())
  {
    return false;
  }

  if(!initial_adc())
  {
    return false;
  }

  if(!initial_tpm())
  {
      return false;
  }

  PRINTF("Initial MCU finished. \r\n");

  return true;
}

bool initial_run_clk(void)
{
  PRINTF("Start initial MCU Clock as RUN Mode. \r\n");

  const mcg_config_t mcgConfig_BOARD_BootClockRUN =
  {
    .mcgMode = kMCG_ModeFEE,                  /* FEE - FLL Engaged External */
    .irclkEnableMode = kMCG_IrclkEnable,      /* MCGIRCLK enabled, MCGIRCLK disabled in STOP mode */
    .ircs = kMCG_IrcSlow,                     /* Slow internal reference clock selected */
    .fcrdiv = 0x0U,                           /* Fast IRC divider: divided by 1 */
    .frdiv = 0x0U,                            /* FLL reference clock divider: divided by 1 */
    .drs = kMCG_DrsMid,                       /* Mid frequency range */
    .dmx32 = kMCG_Dmx32Fine,                  /* DCO is fine-tuned for maximum frequency with 32.768 kHz reference */
  };
  const sim_clock_config_t simConfig_BOARD_BootClockRUN =
  {
    .clkdiv1 = 0x10000U,                      /* SIM_CLKDIV1 - OUTDIV1: /1, OUTDIV4: /2 */
  };
  const osc_config_t oscConfig_BOARD_BootClockRUN =
  {
    .freq = 32768U,                           /* Oscillator frequency: 32768Hz */
    .capLoad = (OSC_CAP0P),                   /* Oscillator capacity load: 0pF */
    .workMode = kOSC_ModeOscLowPower,         /* Oscillator low power */
    .oscerConfig =
      {
        .enableMode = kOSC_ErClkEnable,   /* Enable external reference clock, disable external reference clock in STOP mode */
      }
  };

  /* Set the system clock dividers in SIM to safe value. */
  CLOCK_SetSimSafeDivs();
  /* Initializes OSC0 according to board configuration. */
  CLOCK_InitOsc0(&oscConfig_BOARD_BootClockRUN);
  CLOCK_SetXtal0Freq(oscConfig_BOARD_BootClockRUN.freq);
  /* Set MCG to FEE mode. */
  CLOCK_BootToFeeMode(kMCG_OscselOsc,
                      mcgConfig_BOARD_BootClockRUN.frdiv,
                      mcgConfig_BOARD_BootClockRUN.dmx32,
                      mcgConfig_BOARD_BootClockRUN.drs,
                      CLOCK_CONFIG_FllStableDelay);
  /* Configure the Internal Reference clock (MCGIRCLK). */
  CLOCK_SetInternalRefClkConfig(mcgConfig_BOARD_BootClockRUN.irclkEnableMode,
                                mcgConfig_BOARD_BootClockRUN.ircs,
                                mcgConfig_BOARD_BootClockRUN.fcrdiv);
  /* Set the clock configuration in SIM module. */
  CLOCK_SetSimConfig(&simConfig_BOARD_BootClockRUN);
  /* Set SystemCoreClock variable. */
  SystemCoreClock = RUN_MODE_CORE_CLOCK;

  PRINTF("Initial MCU clock finished. \r\n");

  return true;

}

bool initial_vlpr_clk(void)
{
  PRINTF("Start initial MCU clock as VLPR. \r\n");

  PRINTF("Initial MCU clock finished. \r\n");

  return true;
}


bool initial_gpio(void)
{

  PRINTF("Start initial MCU GPIO. \r\n");
  
  unsigned int pin_masks = 0;

  /* Define the init structure for the input pin*/
  gpio_pin_config_t input_pin_config = {
    kGPIO_DigitalInput, 0,
  };

  /* Define the init structure for the output pin*/
  gpio_pin_config_t output_pin_config = {
    kGPIO_DigitalOutput, 0,
  };

#if defined(GPIO_A_OUTPUT_PINS) || defined(GPIO_A_INPUT_PINS)
  CLOCK_EnableClock(kCLOCK_PortA); 

#ifdef GPIO_A_INPUT_PINS  
  pin_masks = GPIO_A_INPUT_PINS;
  for(int i = 0;i < 32;i++)
  {
    if(pin_masks & (1 << i))
    {
      PORT_SetPinMux(PORTA, i, kPORT_MuxAsGpio);
      GPIO_PinInit(GPIOA, i, &input_pin_config);
      PRINTF("Configure GPIO A Pin %d as Input.\n", i);
    }
  }
#endif

#ifdef GPIO_A_OUTPUT_PINS
  pin_masks = GPIO_A_OUTPUT_PINS;
  for(int i = 0;i < 32;i++)
  {
    if(pin_masks & (1 << i))
    {
      PORT_SetPinMux(PORTA, i, kPORT_MuxAsGpio);
      GPIO_PinInit(GPIOA, i, &output_pin_config);
      PRINTF("Configure GPIO A Pin %d as Output.\n", i);
    }
  }
#endif

#endif

#if defined(GPIO_B_OUTPUT_PINS) || defined(GPIO_B_INPUT_PINS) || defined(USE_I2C)
  CLOCK_EnableClock(kCLOCK_PortB); 

#ifdef GPIO_B_INPUT_PINS  
  pin_masks = GPIO_B_INPUT_PINS;
  for(int i = 0;i < 32;i++)
  {
    if(pin_masks & (1 << i))
    {
      PORT_SetPinMux(PORTB, i, kPORT_MuxAsGpio);
      GPIO_PinInit(GPIOB, i, &input_pin_config);
      PRINTF("Configure GPIO B Pin %d as Input.\n", i);

    }
  }
#endif

#ifdef GPIO_B_OUTPUT_PINS
  pin_masks = GPIO_B_OUTPUT_PINS;
  for(int i = 0;i < 32;i++)
  {
    if(pin_masks & (1 << i))
    {
      PORT_SetPinMux(PORTB, i, kPORT_MuxAsGpio);
      GPIO_PinInit(GPIOB, i, &output_pin_config);
      PRINTF("Configure GPIO B Pin %d as Output.\n", i);
    }
  }
#endif

#ifdef USE_I2C
#endif

#endif
  PRINTF("Initial MCU GPIO finished. \r\n");

  return true;
}

bool initial_uart(void)
{
  PRINTF("Start initial MCU UART. \r\n");
  
#ifdef USE_UART0

#if !defined(GPIO_A_OUTPUT_PINS) || !defined(GPIO_A_INPUT_PINS)
    CLOCK_EnableClock(kCLOCK_PortA);
#endif

#if !defined(GPIO_B_OUTPUT_PINS) || !defined(GPIO_B_INPUT_PINS)
    CLOCK_EnableClock(kCLOCK_PortB);
#endif

#define SOPT5_UART0RXSRC_UART_RX      0x00u   /*!< UART0 receive data source select: UART0_RX pin */
#define SOPT5_UART0TXSRC_UART_TX      0x00u   /*!< UART0 transmit data source select: UART0_TX pin */


  PORT_SetPinMux(PORTB, 1, kPORT_MuxAlt2);            /* PORTB1 (pin 17) is configured as UART0_TX */
  PORT_SetPinMux(PORTB, 2, kPORT_MuxAlt2);            /* PORTB2 (pin 18) is configured as UART0_RX */
  SIM->SOPT5 = ((SIM->SOPT5 &
    (~(SIM_SOPT5_UART0TXSRC_MASK | SIM_SOPT5_UART0RXSRC_MASK))) /* Mask bits to zero which are setting */
      | SIM_SOPT5_UART0TXSRC(SOPT5_UART0TXSRC_UART_TX)       /* UART0 transmit data source select: UART0_TX pin */
      | SIM_SOPT5_UART0RXSRC(SOPT5_UART0RXSRC_UART_RX)       /* UART0 receive data source select: UART0_RX pin */
  );

  CLOCK_SetLpsci0Clock(0x1); /* select FLL clock for LPSCI */
#ifdef UART0_DEBUG
  DbgConsole_Init((uint32_t)UART0, UART_BAUDRATE, DEBUG_CONSOLE_DEVICE_TYPE_LPSCI, CLOCK_GetFllFreq());
#else
  lpsci_config_t config;
  /*
   * config.parityMode = kLPSCI_ParityDisabled;
   * config.stopBitCount = kLPSCI_OneStopBit;
   * config.enableTx = false;
   * config.enableRx = false;
  */
  LPSCI_GetDefaultConfig(&config);
  config.baudRate_Bps = UART_BAUDRATE;
  config.enableTx = true;
  config.enableRx = true;
  LPSCI_Init(UART0, &config, CLOCK_GetFllFreq());
#endif

#endif
  PRINTF("Initial MCU UART finished. \r\n");

  return true;

}

bool initial_master_spi(void)
{
  PRINTF("Start initial MCU SPI as master. \r\n");
  
#ifdef USE_SPI

  spi_master_config_t masterConfig = {0};
  uint32_t sourceClock = 0U;
#if (GPIO_A_OUTPUT_PINS & 0xE0) || (GPIO_A_OUTPUT_PINS & 0xE0)
#error Inconsistance configuration: SPI pins were occupied by GPIO A.
#endif
#if (GPIO_B_OUTPUT_PINS & 0x01) || (GPIO_B_OUTPUT_PINS & 0x01)
#error Inconsistance configuration: SPI pins were occupied by GPIO B.
#endif

#if !defined(GPIO_A_OUTPUT_PINS) || !defined(GPIO_A_INPUT_PINS)
    CLOCK_EnableClock(kCLOCK_PortA);
#endif

#if !defined(GPIO_B_OUTPUT_PINS) || !defined(GPIO_B_INPUT_PINS)
    CLOCK_EnableClock(kCLOCK_PortB);
#endif

  PORT_SetPinMux(PORTA, 5, kPORT_MuxAlt3);            /* PORTA5 (pin 9) is configured as SPI0_SS_b */
  PORT_SetPinMux(PORTA, 6, kPORT_MuxAlt3);            /* PORTA6 (pin 10) is configured as SPI0_MISO */
  PORT_SetPinMux(PORTA, 7, kPORT_MuxAlt3);            /* PORTA7 (pin 15) is configured as SPI0_MOSI */
  PORT_SetPinMux(PORTB, 0, kPORT_MuxAlt3);            /* PORTB0 (pin 16) is configured as SPI0_SCK */

  /* Init SPI master */
  /*
   * masterConfig->enableStopInWaitMode = false;
   * masterConfig->polarity = kSPI_ClockPolarityActiveHigh;
   * masterConfig->phase = kSPI_ClockPhaseFirstEdge;
   * masterConfig->direction = kSPI_MsbFirst;
   * masterConfig->dataMode = kSPI_8BitMode;
   * masterConfig->txWatermark = kSPI_TxFifoOneHalfEmpty;
   * masterConfig->rxWatermark = kSPI_RxFifoOneHalfFull;
   * masterConfig->pinMode = kSPI_PinModeNormal;
   * masterConfig->outputMode = kSPI_SlaveSelectAutomaticOutput;
   * masterConfig->baudRate_Bps = 500000U;
   */
  SPI_MasterGetDefaultConfig(&masterConfig);
  sourceClock = DEFAULT_SPI_MASTER_CLK_FREQ;
  SPI_MasterInit(DEFAULT_SPI_MASTER, &masterConfig, sourceClock);


#endif
  PRINTF("Initial MCU SPI finished. \r\n");

  return true;
}

bool initial_i2c(void)
{
  PRINTF("Start initial MCU I2C. \r\n");
#ifdef USE_I2C
#if (GPIO_B_OUTPUT_PINS & I2C_SDA_PIN) || (GPIO_B_OUTPUT_PINS & I2C_SCL_PIN)
#error Inconsistance configuration: I2C pins were occupied by GPIO B.
#endif

  PRINTF("Release I2C Bus... ");
  i2c_release_bus();
  PRINTF("Done!\r\n");

  const port_pin_config_t i2c_scl_pin_config = {
    kPORT_PullUp,                                            /* Internal pull-up resistor is enabled */
    kPORT_PassiveFilterDisable,                              /* Passive filter is disabled */
    kPORT_HighDriveStrength,                                  /* Low drive strength is configured */
    kPORT_MuxAlt2,                                           /* Pin is configured as I2C0_SCL */
  };
  PORT_SetPinConfig(I2C_SCL_PORT, I2C_SCL_PIN, &i2c_scl_pin_config);  /* PORTB3 (pin 23) is configured as I2C0_SCL */

  const port_pin_config_t i2c_sda_pin_config = {
    kPORT_PullUp,                                            /* Internal pull-up resistor is enabled */
    kPORT_PassiveFilterDisable,                              /* Passive filter is disabled */
    kPORT_LowDriveStrength,                                  /* Low drive strength is configured */
    kPORT_MuxAlt2,                                           /* Pin is configured as I2C0_SDA */
  };
  PORT_SetPinConfig(I2C_SDA_PORT, I2C_SDA_PIN, &i2c_sda_pin_config);  /* PORTB4 (pin 24) is configured as I2C0_SDA */

  i2c_master_handle_t * i2c_handler_ptr = get_i2c_handler();
  I2C_MasterTransferCreateHandle(I2C_BASE_ADDR, i2c_handler_ptr, i2c_master_callback, NULL);

  i2c_master_config_t masterConfig;
  
  /*
   * masterConfig.baudRate_Bps = 100000U;
   * masterConfig.enableStopHold = false;
   * masterConfig.glitchFilterWidth = 0U;
   * masterConfig.enableMaster = true;
   */
  I2C_MasterGetDefaultConfig(&masterConfig);
 
  masterConfig.baudRate_Bps = I2C_BAUDRATE;
 
  I2C_MasterInit(I2C_BASE_ADDR, &masterConfig, CLOCK_GetFreq(I2C0_CLK_SRC));
#endif

  PRINTF("Initial MCU I2C finished. \r\n");

  return true;
}

bool initial_adc(void)
{
  PRINTF("Start initial MCU ADC. \r\n");

  PRINTF("Initial MCU ADC finished. \r\n");

  return true;
}

bool initial_tpm(void)
{
  PRINTF("Start initial MCU TPM. \r\n");
#ifdef USE_TPM
  CLOCK_EnableClock(kCLOCK_PortB);                           /* Port B Clock Gate Control: Clock enabled */

#ifndef DBG_PWM_OUTPUT
  // disable TPM1 CH1 as PMW
  PORT_SetPinMux(TPM1_CH1_PORT, TPM1_CH1_PIN_IDX, kPORT_MuxAlt2);    /* PORTB6 (pin 1) is configured as TPM1_CH1 */
#endif
  PORT_SetPinMux(TPM1_CH0_PORT, TPM1_CH0_PIN_IDX, kPORT_MuxAlt2);    /* PORTB7 (pin 2) is configured as TPM1_CH0 */

  /* Select the clock source for the TPM counter as MCGFLLCLK */
  CLOCK_SetTpmClock(1U);

  tpm_config_t tpmInfo;
#ifndef DBG_PWM_OUTPUT
  tpm_chnl_pwm_signal_param_t tpmParam[2];

  /* Configure tpm params with frequency 125kHZ */
  tpmParam[0].chnlNumber = (tpm_chnl_t)1;
  tpmParam[0].level = kTPM_LowTrue;
  tpmParam[0].dutyCyclePercent = 50;

  tpmParam[1].chnlNumber = (tpm_chnl_t)0;
  tpmParam[1].level = kTPM_LowTrue;
  tpmParam[1].dutyCyclePercent = 50;
#endif
  /*
   * tpmInfo.prescale = kTPM_Prescale_Divide_1;
   * tpmInfo.useGlobalTimeBase = false;
   * tpmInfo.enableDoze = false;
   * tpmInfo.enableDebugMode = false;
   * tpmInfo.enableReloadOnTrigger = false;
   * tpmInfo.enableStopOnOverflow = false;
   * tpmInfo.enableStartOnTrigger = false;
   * tpmInfo.enablePauseOnTrigger = false;
   * tpmInfo.triggerSelect = kTPM_Trigger_Select_0;
   * tpmInfo.triggerSource = kTPM_TriggerSource_External;
   */
  TPM_GetDefaultConfig(&tpmInfo);

  /* Initialize TPM module */
  TPM_Init(TPM1, &tpmInfo);
#ifndef DBG_PWM_OUTPUT
  TPM_SetupPwm(TPM1, tpmParam, 1U, kTPM_EdgeAlignedPwm, 125000U, TPM_SOURCE_CLOCK);
#endif
  /* Setup input capture on a TPM channel */
  TPM_SetupInputCapture(TPM1, kTPM_Chnl_0, kTPM_RiseAndFallEdge);

#endif
  PRINTF("Initial MCU TPM finished. \r\n");

  return true;

}

void i2c_release_bus(void)
{
  uint8_t i = 0;
  gpio_pin_config_t pin_config;
  port_pin_config_t i2c_pin_config = {0};

  /* Config pin mux as gpio */
  i2c_pin_config.pullSelect = kPORT_PullUp;
  i2c_pin_config.mux = kPORT_MuxAsGpio;

  pin_config.pinDirection = kGPIO_DigitalOutput;
  pin_config.outputLogic = 1U;
#if !defined(GPIO_B_OUTPUT_PINS) || !defined(GPIO_B_INPUT_PINS)
  CLOCK_EnableClock(kCLOCK_PortB);
#endif
  PORT_SetPinConfig(I2C_SCL_PORT, I2C_SCL_PIN, &i2c_pin_config);
  PORT_SetPinConfig(I2C_SDA_PORT, I2C_SDA_PIN, &i2c_pin_config);

  GPIO_PinInit(I2C_SCL_GPIO_PORT, I2C_SCL_PIN, &pin_config);
  GPIO_PinInit(I2C_SDA_GPIO_PORT, I2C_SDA_PIN, &pin_config);

  /* Drive SDA low first to simulate a start */
  GPIO_WritePinOutput(I2C_SDA_GPIO_PORT, I2C_SDA_PIN, 0U);
  just_short_delay();
  /* Send 9 pulses on SCL and keep SDA low */
  for (i = 0; i < 9; i++)
  {
    GPIO_WritePinOutput(I2C_SCL_GPIO_PORT, I2C_SCL_PIN, 0U);
    just_short_delay();

    GPIO_WritePinOutput(I2C_SDA_GPIO_PORT, I2C_SDA_PIN, 1U);
    just_short_delay();

    GPIO_WritePinOutput(I2C_SCL_GPIO_PORT, I2C_SCL_PIN, 1U);
    just_short_delay();
    just_short_delay();
  }

    /* Send stop */
  GPIO_WritePinOutput(I2C_SCL_GPIO_PORT, I2C_SCL_PIN, 0U);
  just_short_delay();

  GPIO_WritePinOutput(I2C_SDA_GPIO_PORT, I2C_SDA_PIN, 0U);
  just_short_delay();

  GPIO_WritePinOutput(I2C_SCL_GPIO_PORT, I2C_SCL_PIN, 1U);
  just_short_delay();

  GPIO_WritePinOutput(I2C_SDA_GPIO_PORT, I2C_SDA_PIN, 1U);
  just_short_delay();

}

