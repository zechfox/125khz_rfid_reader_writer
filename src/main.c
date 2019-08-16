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
   * Mod: Man/Bi/PSK
   * Cyc:64/32/16
   * Mode:Read/Write
   * ==============
   */
  lcd_update_display_buffer(0, 0, "TagId:");
  lcd_update_display_buffer(1, 0, "FFFFFFFF");
  lcd_update_display_buffer(2, 0, "Mod:UNKN");
  lcd_update_display_buffer(3, 0, "Cyc:UNKN");
  lcd_update_display_buffer(4, 0, "Mode:Read");

  PRINTF("refresh LCD. \r\n");
  lcd_refresh_screen();
#ifdef DBG_PWM_OUTPUT
  GPIO_WritePinOutput(TPM1_CH1_PIN_PORT, TPM1_CH1_PIN_IDX, 1);
  PRINTF("Force pin of TPM1_CH1 output high! \r\n");
#endif
  rfid_init();

  while(1)
  {

    if(IDLE == g_reader_state)
    {
      rfid_enable_carrier();
    }
    else if(RESET == g_reader_state)
    {
      rfid_reset();
    }
    else if(PARSE_DATA == g_reader_state)
    {
      // got enough data, disable carrier
      // next loop will enable it again
      rfid_disable_carrier();
#ifdef RFID_DBG_RECV

      for(unsigned char i = 0;i < RFID_DETECT_RISE_EDGE_NUMBER;i++)
      {
        PRINTF("DBG: Rise Edge No.%d width: %d \r\n", i, g_rfid_rise_edge_width[i]);
      }

      PRINTF("DBG: Detected Tag bit length: %d cycle \r\n", g_rfid_tag.bit_length);
      PRINTF("DBG: Detected Tag encode scheme %d \r\n", g_rfid_tag.encode_scheme);
      g_reader_state = RESET;
#endif
      result = rfid_parse_data(&g_rfid_tag);
    }


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



