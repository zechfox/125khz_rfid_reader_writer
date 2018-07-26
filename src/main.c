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
#include "dht12.h"
#include "mcu_MKL02Z32VFM4.h"

#include "fsl_gpio.h"


int main(void)
{

  char temp_str[6];
  char humi_str[6];

  //initial MCU
  initial_mcu();
    

  //initial devices
  lcd_initial();


  while(1)
  {
    PRINTF("Delay 2 secondes. \r\n");
    for(int i = 0;i < 40000000;i++)
    {
      __NOP();
    }
    PRINTF("Start read DHT12. \r\n");
    status_t result = dht12_get_temp_humi(temp_str, humi_str);
    if(kStatus_Success == result)
    {
      lcd_update_display_buffer(1, 0, "temperature:");
      lcd_update_display_buffer(2, 3, temp_str);
      lcd_update_display_buffer(3, 0, "humidity");
      lcd_update_display_buffer(4, 3, humi_str);

      PRINTF("refresh LCD. \r\n");
      lcd_refresh_screen();
    }
    else
      PRINTF("read DHT12 failed %d \r\n", result);

  }
  return 0;
}
