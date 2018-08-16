/*
 * RFID head file
 *
 *
 */

#include "configuration.h"

typedef struct rfid_tag {
  char version_number;
  unsigned int rfid;  
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

void rfid_enable_carrier();
void rfid_disable_carrier();
