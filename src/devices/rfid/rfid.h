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
  DETECT_PERIOD,
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
#ifdef RFID_DBG_RECV
extern unsigned char g_rfid_rise_edge_width[RFID_DETECT_RISE_EDGE_NUMBER];
#endif
extern rfidTag g_rfid_tag;
#ifdef RFID_DBG
extern unsigned int g_rfid_dbg_counter;
#endif
void rfid_enable_carrier();
void rfid_disable_carrier();
status_t rfid_parity_check();
status_t rfid_parse_data(rfidTag *rfid_tag_ptr);
void rfid_reset();
void rfid_init();
