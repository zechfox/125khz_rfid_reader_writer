
#define LCD_DUMMY_COMMAND (0)
#define LCD_PIXEL_BUFFER_ROW (6)
#define LCD_PIXEL_BUFFER_COLUMN (84)
#define LCD_PIXEL_CHARACTER_WIDTH (6)

extern unsigned char display_buffer[LCD_DISPLAY_BUFFER_ROW][LCD_DISPLAY_BUFFER_COLUMN];
extern unsigned char * pixel_buffer_ptr;
extern volatile unsigned int pixel_horizon_addr;

void lcd_initial(void);
void lcd_clr_scr(void);
void lcd_reset(void);
void lcd_write_english_string(unsigned char X,unsigned char Y,char *s);
void lcd_draw_bmp_pixel(unsigned char X,unsigned char Y,unsigned char *map,
                        unsigned char Pix_x,unsigned char Pix_y);
void lcd_write_cmd_blk(unsigned char command);
void lcd_write_data(unsigned char data);
void lcd_set_cursor(unsigned char X, unsigned char Y);

void lcd_refresh_screen(void);
void lcd_update_display_buffer(unsigned char row, unsigned char column, char *new_char);

void lcd_spi_master_irq_handler(void);

void just_delay(void);

