/*
 * RFID head file
 *
 *
 */

#include "configuration.h"

typedef struct rfid_tag {
  unsigned char version_number;
  unsigned int tag_id;  
} rfidTag;

typedef enum reader_state {
  IDLE,
  WAIT_HEAD,
  READ_DATA,
  DONE
} readerState;

typedef enum Work_mode {
    READ,
    WRITE
} workMode;

typedef enum Recv_data_state {
    SEEK_HEADER,
    RECEIVE_DATA,
    DATA_READY
} recvDataState;

extern bool g_is_transmitting;
extern workMode g_work_mode;
extern recvDataState g_recv_data_state;
extern unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
extern unsigned char g_rfid_pulse_width[RFID_EM4100_DATA_BITS];
extern rfidTag g_rfid_tag;

void rfid_enable_carrier();
void rfid_disable_carrier();
status_t rfid_parity_check();
status_t rfid_parse_data(rfidTag *rfid_tag_ptr);
