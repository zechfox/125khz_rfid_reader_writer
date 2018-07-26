#include "fsl_spi.h"
#include "fsl_gpio.h"

#include "fsl_debug_console.h"

#include "configuration.h"
#include "english_6x8_pixel.h"
#include "lcd_PCD8544.h"

#define lcd_spi_master_irq_handler SPI0_IRQHandler

unsigned char display_buffer[LCD_DISPLAY_BUFFER_ROW][LCD_DISPLAY_BUFFER_COLUMN];
unsigned char * pixel_buffer_ptr = 0;
volatile unsigned int pixel_horizon_addr = 0;

void lcd_initial(void)
{
  pixel_buffer_ptr = &display_buffer[0][0];

  lcd_reset();

  lcd_write_cmd_blk(0x21);  //normal mode, Horizontal addressing, use extended instruction set
  lcd_write_cmd_blk(0x06);  //VLCD temperature coefficient 2
  lcd_write_cmd_blk(0x13);  //bias system (BSx) 1:48
  lcd_write_cmd_blk(0xc8);  //VLCD = 3.06 + 0.06*Vop, constrast

  lcd_write_cmd_blk(0x20);  //normal mode, Horizontal addressing, use basic instruction set
  lcd_write_cmd_blk(0x0c);  //normal mode
  lcd_write_cmd_blk(0x40);  //start row 0
  lcd_write_cmd_blk(0x80);  //start column 0

  EnableIRQ(LCD_SPI_IRQ);
  lcd_clr_scr();            //clear screen
  PRINTF("lcd initial...Done.\r\n");

}

void lcd_clr_scr(void)
{

  // fill pixel buffer by white space
  memset(pixel_buffer_ptr, ' ', LCD_PIXEL_BUFFER_ROW * LCD_PIXEL_BUFFER_COLUMN / LCD_PIXEL_CHARACTER_WIDTH);

  // flush pixel buffer to lcd 
  lcd_refresh_screen();

  while(pixel_horizon_addr != 0) {}

}


void lcd_write_english_string(unsigned char X,unsigned char Y,char *s)
{
}

void lcd_write_cmd_blk(unsigned char command)
{
  //enable lcd CE
  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 0);
  //DC 0 for command
  GPIO_WritePinOutput(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, 0);

  //polling SPI tx status
  while (0 == (SPI_GetStatusFlags(LCD_SPI) & kSPI_TxBufferEmptyFlag))
  {
  }

  //SPI ready tx
  SPI_WriteData(LCD_SPI, command);
  //polling SPI tx status
  while (0 == (SPI_GetStatusFlags(LCD_SPI) & kSPI_TxBufferEmptyFlag))
  {
  }

  SPI_WriteData(LCD_SPI, LCD_DUMMY_COMMAND);
  //polling SPI tx status
  while (0 == (SPI_GetStatusFlags(LCD_SPI) & kSPI_TxBufferEmptyFlag))
  {
  }

  //disable lcd
  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 1);

}

void lcd_draw_bmp_pixel(unsigned char X,unsigned char Y,unsigned char *map,
                        unsigned char Pix_x,unsigned char Pix_y)
{
}

void lcd_write_data(unsigned char data)
{
  //enable lcd CE
  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 0);

  //DC 1 for data
  GPIO_WritePinOutput(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, 1);

  //polling SPI tx status
  while (0 == (SPI_GetStatusFlags(LCD_SPI) & kSPI_TxBufferEmptyFlag))
  {
  }

  //SPI ready tx
  SPI_WriteData(LCD_SPI, data);

  //polling SPI tx status
  while (0 == (SPI_GetStatusFlags(LCD_SPI) & kSPI_TxBufferEmptyFlag))
  {
  }

  //disable lcd CE
  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 1);

}


void LCD_set_XY(unsigned char X, unsigned char Y)
{
}

void lcd_update_display_buffer(unsigned char row, unsigned char column, char *new_char)
{
  unsigned char x = column % LCD_DISPLAY_BUFFER_COLUMN;
  unsigned char y = row % LCD_DISPLAY_BUFFER_ROW;

  unsigned char * update_start = &display_buffer[y][x];
  int new_char_length = strlen(new_char);
  int display_buffer_capable = LCD_DISPLAY_BUFFER_COLUMN * LCD_DISPLAY_BUFFER_ROW - x * y;

  if((new_char_length - display_buffer_capable) > 0)
  {
    PRINTF("Do not have enough space for new character.\r\n");
    memcpy(update_start, new_char, display_buffer_capable);
  }
  else
  {
    PRINTF("Copy \"%s\" to display buffer, length is %d.\r\n", new_char, new_char_length);
    memcpy(update_start, new_char, new_char_length);
  }

}


void lcd_refresh_screen(void)
{
  //enable lcd CE, will be disabled in interrupt handler
  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 0);

  // always flush lcd by pixel_buffer_ptr
  // enable spi interrupt
  SPI_EnableInterrupts(LCD_SPI, kSPI_TxEmptyInterruptEnable | kSPI_RxFullAndModfInterruptEnable);

}

void just_delay(void)
{
  unsigned int i;
  for (i=0;i<5140;i++);
}

void lcd_reset(void)
{
  // output low level 
  GPIO_WritePinOutput(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, 0);
  just_delay();
  // output high level
  GPIO_WritePinOutput(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, 1);
  //enable lcd CE
  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 0);
  just_delay();

  GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 1);
  just_delay();


}

void lcd_spi_master_irq_handler(void)
{

  if ((SPI_GetStatusFlags(LCD_SPI) & kSPI_TxBufferEmptyFlag))
  {
    GPIO_WritePinOutput(LCD_DC_GPIO_PORT, LCD_DC_GPIO_PIN, 1);

    unsigned char font_index = *(pixel_buffer_ptr + (pixel_horizon_addr / LCD_PIXEL_CHARACTER_WIDTH)) - 32;
    unsigned char font_slot = pixel_horizon_addr % LCD_PIXEL_CHARACTER_WIDTH;
    unsigned char send_data = font6x8[font_index][font_slot];

    SPI_WriteData(LCD_SPI, (uint16_t)send_data);
    pixel_horizon_addr++;
  }

  if (pixel_horizon_addr > (LCD_PIXEL_BUFFER_COLUMN * LCD_PIXEL_BUFFER_ROW + 1))
  {
    pixel_horizon_addr = 0;
    SPI_DisableInterrupts(LCD_SPI, kSPI_TxEmptyInterruptEnable | kSPI_RxFullAndModfInterruptEnable);
    //disable lcd CE
    GPIO_WritePinOutput(LCD_CE_GPIO_PORT, LCD_CE_GPIO_PIN, 1);

  }

}

