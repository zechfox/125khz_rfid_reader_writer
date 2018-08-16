/*
 * entry of program
 *
 *
 */
#include <stdio.h>
#include <string.h>

#include "fsl_debug_console.h"
#include "configuration.h"
#include "lcd_PCD8544.h"
#include "rfid.h"
#include "mcu_MKL02Z32VFM4.h"

#include "fsl_gpio.h"


int main(void)
{

  //initial MCU
  initial_mcu();

  //initial devices
  lcd_initial();

  /*
   * LCD layout
   * ==============
   * TagId: 
   * xxxxxxxxxxxxxx
   * TagType:EM4100/T5557
   * Modulation:AM/FSK/PSK
   * Mode:Read/Write
   * ==============
   */

  while(1)
  {
    PRINTF("Delay 2 secondes. \r\n");
    for(int i = 0;i < 40000000;i++)
    {
      __NOP();
    }
    if(!g_is_transmitting)
    {
      PRINTF("Start TPM1. \r\n");
      rfid_enable_carrier();
    }
    status_t result = kStatus_Success;
    if(kStatus_Success == result)
    {
      lcd_update_display_buffer(1, 0, "TagId:");
      lcd_update_display_buffer(2, 0, "DummyId");
      lcd_update_display_buffer(3, 0, "TagType:");
      lcd_update_display_buffer(4, 0, "Modulation");
      if(READ == g_work_mode)
      {
        lcd_update_display_buffer(5, 3, "Mode:Read");
      }
      else
      {
        lcd_update_display_buffer(5, 3, "Mode:Write");
      } 

      PRINTF("refresh LCD. \r\n");
      lcd_refresh_screen();
    }

  }
  return 0;
}


// gpio interrrupt handler
// handle modulation signal


// timer handler
// toggle carrier transmit gpio pin



