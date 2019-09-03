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
  unsigned char customer_spec_id;
  unsigned int tag_data;  
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
extern unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
#ifdef RFID_DBG
extern unsigned int g_rfid_dbg_counter;
#endif
void rfid_enable_carrier();
void rfid_disable_carrier();
bool rfid_parity_check(unsigned char * check_data);
bool rfid_try_get_tag_payload(unsigned char * check_data, unsigned char * tag_payload_ptr);
status_t rfid_parse_data(rfidTag *rfid_tag_ptr);
status_t rfid_parse_data_manchester(unsigned char * modulated_data_ptr, unsigned char * tag_payload_ptr);
status_t rfid_parse_data_biphase(unsigned char * modulated_data_ptr, unsigned char * tag_payload_ptr);
status_t rfid_demodule_data(encodeScheme code_scheme, unsigned char * mod_data, bool shift_phase, unsigned char * demod_data);
unsigned char rfid_decode_manchester(unsigned char manchester_code);
unsigned char rfid_decode_biphase(unsigned char biphase_code);
void rfid_get_tag_payload(unsigned char * demod_data_ptr, unsigned char * tag_payload_ptr);
void rfid_extract_bits(unsigned char * source, unsigned char * destination, unsigned char start_bit_index, unsigned char number);
void rfid_format_for_parity_check(unsigned char * unformat_data, unsigned char * formatted_data);
bool rfid_get_bit_length(unsigned char cycles, unsigned char * bit_length_ptr);
status_t rfid_receive_data(rfidTag * rfid_tag_ptr);
void rfid_reset(rfidTag * rfid_tag_ptr);
void rfid_init(rfidTag * rfid_tag_ptr);
