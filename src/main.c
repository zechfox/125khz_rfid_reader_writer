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
  char tag_data_str[10];
  rfidTag rfid_tag;

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
  lcd_update_display_buffer(0, 0, "Tag:");
  lcd_update_display_buffer(1, 0, "FFFF");
  lcd_update_display_buffer(2, 0, "FFFFFFFF");
  lcd_update_display_buffer(3, 0, "Mod:UNKN");
  lcd_update_display_buffer(4, 0, "Cyc:UNKN");
  lcd_update_display_buffer(5, 0, "Mode:Read");

  PRINTF("refresh LCD. \r\n");
  lcd_refresh_screen();
#ifdef DBG_PWM_OUTPUT
  GPIO_WritePinOutput(TPM1_CH1_PIN_PORT, TPM1_CH1_PIN_IDX, 1);
  PRINTF("Force pin of TPM1_CH1 output high! \r\n");
#endif
  rfid_init(&rfid_tag);
  rfid_enable_carrier();

  while(1)
  {
    result = rfid_receive_data(&rfid_tag);

    if(kStatus_Success == result)
    {
      PRINTF(">>> RFID Received Successfully! >>> \r\n");
      unsigned int len = 0;
      lcd_clr_scr();
      lcd_update_display_buffer(0, 0, "Tag:");
      len =  out_hex_str((unsigned int)rfid_tag.customer_spec_id, tag_data_str);
      if(len > 0)
      {
        lcd_update_display_buffer(1, 0, tag_data_str);
      }
      else
      {
        lcd_update_display_buffer(1, 0, "UNKN");
      }

      // tag id
      len = out_hex_str(rfid_tag.tag_data, tag_data_str);
      if(len > 0)
      {
        lcd_update_display_buffer(2, 0, tag_data_str);
      }
      else
      {
        lcd_update_display_buffer(2, 0, "UNKN");
      }

      // code scheme
      if (MANCHESTER == rfid_tag.encode_scheme)
      {
        lcd_update_display_buffer(3, 0, "Mod:MAN");
      }
      else if (BIPHASE == rfid_tag.encode_scheme)
      {
        lcd_update_display_buffer(3, 0, "Mod:BI");
      }
      else if (PSK == rfid_tag.encode_scheme)
      {
        lcd_update_display_buffer(3, 0, "Mod:PSK");
      }

      // cycle
      if (RFID_CYCLE_16 == rfid_tag.bit_length)
      {
        lcd_update_display_buffer(4, 0, "Cyc:16");
      }
      else if (RFID_CYCLE_32 == rfid_tag.bit_length)

      {
        lcd_update_display_buffer(4, 0, "Cyc:32");
      }
      else
      {
        lcd_update_display_buffer(4, 0, "Cyc:64");
      }

      // work mode
      if(READ == g_work_mode)
      {
        lcd_update_display_buffer(5, 0, "Mode:Read");
      }
      else
      {
        lcd_update_display_buffer(5, 0, "Mode:Write");
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



