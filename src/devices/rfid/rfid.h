/*
 * RFID head file
 *
 *
 */

#include "configuration.h"

typedef enum encode_scheme {
    UNKNOWN,
    MANCHESTER,
    BIPHASE,
    PSK
} encodeScheme;

typedef struct rfid_tag {
  unsigned char version_number;
  unsigned int tag_id;  
  encodeScheme encode_scheme; 
  unsigned char bit_length; // bit length can be 16, 32, 64
} rfidTag;

typedef enum reader_state {
  IDLE,
  WAIT_STABLE,
  DETECT_PERIOD,
  COLLECT_DATA,
  SYNC_HEAD,
  READ_DATA,
  PARSE_DATA,
  RESET
} readerState;

typedef enum Work_mode {
    READ,
    WRITE
} workMode;

extern workMode g_work_mode;
extern readerState g_reader_state;
extern unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
extern rfidTag g_rfid_tag;
#ifdef RFID_DBG
extern unsigned int g_rfid_dbg_counter;
#endif
void rfid_enable_carrier();
void rfid_disable_carrier();
status_t rfid_parity_check(unsigned char * check_data);
status_t rfid_parse_data(rfidTag *rfid_tag_ptr);
status_t rfid_parse_data_manchester();
status_t rfid_parse_data_biphase();
status_t rfid_demodulation(encodeScheme code_scheme, unsigned char * mod_data, bool shift_phase, unsigned char * demod_data);
unsigned char rfid_decode_manchester(unsigned char manchester_code);
unsigned char rfid_decode_biphase(unsigned char biphase_code);
void rfid_extract_bits(unsigned char * source, unsigned char * destination, unsigned char start_bit_index, unsigned char number);
void rfid_format_for_parity_check(unsigned char * unformat_data, unsigned char * formatted_data);
unsigned char rfid_get_bit_length(unsigned char cycles);
status_t rfid_receive_data();
void rfid_reset();
void rfid_init();
