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

static char hex [] = { '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9' ,'A', 'B', 'C', 'D', 'E', 'F' };

unsigned int out_hex_str(unsigned int number, char* hex_str)
{
    int len = 0, k = 0;
    do//for every 4 bits
    {
        //get the equivalent hex digit
        hex_str[len] = hex[number & 0xF];
        len++;
        number >>= 4;
    }while(number != 0);
    //since we get the digits in the wrong order reverse the digits in the buffer
    for(;k < len / 2;k++)
    {//xor swapping
        hex_str[k] ^= hex_str[len - k - 1];
        hex_str[len - k - 1] ^= hex_str[k];
        hex_str[k] ^= hex_str[len - k - 1];
    }
    //null terminate the buffer and return the length in digits
    hex_str[len]='\0';
    return len;
}

int main(void)
{

  status_t result = kStatus_Fail;
  char tag_id_str[10];
  //initial MCU
  initial_mcu();

  //initial devices
  lcd_initial();

  PRINTF("Delay 2 secondes. \r\n");
  for(int i = 0;i < 40000000;i++)
  {
      __NOP();
  }
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
  lcd_update_display_buffer(0, 0, "TagId:");
  lcd_update_display_buffer(1, 0, "FFFFFFFF");
  lcd_update_display_buffer(2, 0, "TagType:UNKN");
  lcd_update_display_buffer(3, 0, "Mod:UNKN");
  lcd_update_display_buffer(4, 0, "Mode:Read");

  PRINTF("refresh LCD. \r\n");
  lcd_refresh_screen();
#ifdef DBG_PWM_OUTPUT
  GPIO_WritePinOutput(TPM1_CH1_PIN_PORT, TPM1_CH1_PIN_IDX, 1);
  PRINTF("Force PORTB6 output high! \r\n");
#endif

  while(1)
  {

    if(!g_is_transmitting)
    {
      PRINTF("Start TPM1. \r\n");
      rfid_enable_carrier();
    }
    
    if(DATA_READY == g_recv_data_state)
    {
      // got enough data, disable carrier
      // next loop will enable it again
      rfid_disable_carrier();
      result = rfid_parse_data(&g_rfid_tag);
#ifdef RFID_DBG_RECV
      for(unsigned char i = 0;i < RFID_EM4100_DATA_BITS;i++)
      {
        PRINTF("DBG: Bit %d bit width: %d \r\n", i, g_rfid_pulse_width[i]);
      }
#endif
    }
#ifdef RFID_DBG
    if(RECEIVE_DATA == g_recv_data_state)
    {
	g_recv_data_state = SEEK_HEADER;
	PRINTF("DBG: Received bit width: %d \r\n", g_rfid_pulse_width[0]);

    }
#endif

    if(kStatus_Success == result)
    {
      unsigned int len = out_hex_str(g_rfid_tag.tag_id, tag_id_str);
      lcd_update_display_buffer(1, 0, "TagId:");
      if(len > 0)
      {
        lcd_update_display_buffer(2, 0, tag_id_str);
      }
      else
      {
        lcd_update_display_buffer(2, 0, "UNKN");
      }
      lcd_update_display_buffer(3, 0, "TagType:EM4100");
      lcd_update_display_buffer(4, 0, "Modulation:MACHE");
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



