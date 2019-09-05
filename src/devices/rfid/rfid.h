/*
 * RFID head file
 *
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <limits.h>


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

typedef struct rfid_support_functions {
  void (*enable_receiver_fun_ptr)(void);
  void (*enable_transmitter_fun_ptr)(void);
  void (*disable_receiver_fun_ptr)(void);
  void (*disable_transmitter_fun_ptr)(void);
  unsigned int (*read_receiver_level_fun_ptr)(void);
} rfidSupportFun;

extern workMode g_work_mode;
void rfid_enable_carrier();
void rfid_disable_carrier();
bool rfid_parity_check(unsigned char * check_data);
bool rfid_try_get_tag_payload(unsigned char * check_data, unsigned char * tag_payload_ptr);
bool rfid_parse_data(rfidTag *rfid_tag_ptr);
bool rfid_parse_data_manchester(unsigned char * modulated_data_ptr, unsigned char * tag_payload_ptr);
bool rfid_parse_data_biphase(unsigned char * modulated_data_ptr, unsigned char * tag_payload_ptr);
bool rfid_demodule_data(encodeScheme code_scheme, unsigned char * mod_data, bool shift_phase, unsigned char * demod_data);
unsigned char rfid_decode_manchester(unsigned char manchester_code);
unsigned char rfid_decode_biphase(unsigned char biphase_code);
void rfid_get_tag_payload(unsigned char * demod_data_ptr, unsigned char * tag_payload_ptr);
void rfid_extract_bits(unsigned char * source, unsigned char * destination, unsigned char start_bit_index, unsigned char number);
void rfid_format_for_parity_check(unsigned char * unformat_data, unsigned char * formatted_data);
bool rfid_get_bit_length(unsigned char cycles, unsigned char * bit_length_ptr);
bool rfid_receive_data(rfidTag * rfid_tag_ptr);
void rfid_reset(rfidTag * rfid_tag_ptr);
void rfid_init(rfidTag * rfid_tag_ptr);
void rfid_receiver_data_handler(void);
void rfid_transmitter_handler(void);
void rfid_configuration(rfidSupportFun functions);


// used for receive rfid data
#define RFID_HEADER_BITS 9U
#define RFID_EM4100_ROW_PARITY_BITS 10U
#define RFID_EM4100_COL_PARITY_BITS 4U
#define RFID_EM4100_PARITY_BITS (RFID_EM4100_ROW_PARITY_BITS + RFID_EM4100_COL_PARITY_BITS)
#define RFID_EM4100_STOP_BITS 1U
#define RFID_EM4100_DATA_BITS 64U
#define RFID_EM4100_DATA_BUFFER_SIZE (RFID_EM4100_DATA_BITS / 8)
#define RFID_EM4100_PAYLOAD_BITS (RFID_EM4100_DATA_BITS - RFID_HEADER_BITS - RFID_EM4100_PARITY_BITS - RFID_EM4100_STOP_BITS)
#define RFID_EM4100_PAYLOAD_BUFFER_SIZE (RFID_EM4100_PAYLOAD_BITS / 8)
#define RFID_RECORDS_PER_BIT_PERIOD 2U
// 2frames X 64bits/frame X 2records/bit = 256records
#define RFID_WAVEFORM_RECORDS_NUMBER (RFID_RECORDS_PER_BIT_PERIOD * RFID_EM4100_DATA_BITS * 2)
// 1 record take 1 bit, total size: records number / 8bits
#define RFID_WAVEFORM_ARRAY_SIZE (RFID_WAVEFORM_RECORDS_NUMBER / 8)
// 
#define RFID_DETECT_RISE_EDGE_NUMBER (RFID_EM4100_DATA_BITS * 2)
#define RFID_MANCHE_REVERSE_PULSE_WIDTH 333
#define RFID_MANCHE_PULSE_WIDTH  111
#define RFID_MANCHE_PULSE_BIAS  20

#define RFID_GLITCH_CYCLE  4
#define RFID_MAX_COUNTER 128
#define RFID_TAG_LEAVE_COUNTER 1000000 

#define RFID_CYCLE_OFFSET 5
#define RFID_CYCLE_16 16
#define RFID_CYCLE_32 32
#define RFID_CYCLE_64 64
#define RFID_CYCLE_PSK_UPBOUND (RFID_CYCLE_16 + RFID_CYCLE_OFFSET)
#define RFID_CYCLE_PSK_DOWNBOUND (RFID_CYCLE_16 - RFID_CYCLE_OFFSET)
// used for parse rfid data
// total 11 groups(raws) bit, first 10 group for data, 
// the last one for columns parity
#define RFID_BIT_GROUPS 11U
#define RFID_BITS_EACH_GROUP ((RFID_EM4100_DATA_BITS - RFID_HEADER_BITS) / RFID_BIT_GROUPS)

