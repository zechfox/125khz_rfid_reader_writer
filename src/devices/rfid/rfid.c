
#include <stdio.h>

#include "fsl_debug_console.h"
#include <fsl_tpm.h>

#include "configuration.h"
#include "rfid.h"

bool g_is_transmitting = false;
workMode g_work_mode = READ;
recvDataState g_recv_data_state = SEEK_HEADER;
unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
#ifdef RFID_DBG
unsigned char g_rfid_pulse_width[RFID_EM4100_DATA_BITS];
#endif

// initial rfid
void rfid_init()
{
  // first 9bits always 1
  for(unsigned char i = 0;i < RFID_HEADER_BITS;i++)
  {
      g_rfid_bits_buffer[i] = 0x1;
  }
  // clear 10-64bits
  for(unsigned char i = RFID_HEADER_BITS;i < RFID_EM4100_DATA_BITS;i++)
  {
    g_rfid_bits_buffer[i] = 0x0;
  }
#ifdef RFID_DBG
  for(unsigned char i = 0;i < RFID_EM4100_DATA_BITS;i++)
  {
    g_rfid_pulse_width[i] = 0x0;
  }
#endif

  return;
}
// enable transmit 125khz carrier
// set carrier used gpio pin
void rfid_enable_carrier()
{
  // enable receive data only in read mode
  if(READ == g_work_mode)
  {
    /* Set the timer to be in free-running mode */
    // TODO: need following line?
    TPM1->MOD = 0xFFFF;

    /* Enable channel interrupt when the second edge is detected */
    TPM_EnableInterrupts(RFID_TRANSMIT_TPM, RFID_RECEIVE_DATA_CHANNEL_INTERRUPT_ENABLE);

    /* Enable at the NVIC */
    EnableIRQ(RFID_RECEIVE_DATA_INTERRUPT_NUMBER);
  }

  TPM_StartTimer(RFID_TRANSMIT_TPM, kTPM_SystemClock);
  g_is_transmitting = true;
  return;
}

// disable transmit 125khz carrier
// clear carrier used gpio pin
void rfid_disable_carrier()
{
    TPM_StopTimer(RFID_TRANSMIT_TPM);
    g_is_transmitting = false;
    return;
}

// read tag id
rfidTag rfid_read_tag()
{
    rfidTag tag;
    tag.version_number = 0xFF;
    tag.rfid = 0xFFFFFFFF;

    return tag;
}

void RFID_RECEIVE_DATA_HANDLER(void)
{
    static unsigned char valid_bits = 0;
    bool is_reverse_bit = 0; // don't reverse bit by default

    uint32_t capture_value = RFID_TRANSMIT_TPM->CONTROLS[RFID_RECEIVE_DATA_CHANNEL].CnV;
#ifdef RFID_DBG
    if(DATA_READY != g_recv_data_state)
    {
	g_rfid_pulse_width[valid_bits++] = capture_value;
	if(valid_bits >= RFID_EM4100_DATA_BITS)
	{
	    valid_bits = 0;
	    g_recv_data_state = DATA_READY;
	}
    }
    return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_RECEIVE_DATA_CHANNEL_FLAG);
#endif
   
    // validate the pulse
    if(capture_value < (RFID_MANCHE_PULSE_WIDTH - RFID_MANCHE_PULSE_BIAS) 
       || capture_value > (RFID_MANCHE_REVERSE_PULSE_WIDTH + RFID_MANCHE_PULSE_BIAS))
    {
	// received invalid bit pulse
	// could be glitch or noise
	// ignore it
	return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_RECEIVE_DATA_CHANNEL_FLAG);
    }
    else if((capture_value > (RFID_MANCHE_REVERSE_PULSE_WIDTH - RFID_MANCHE_PULSE_BIAS))
	    && (capture_value < (RFID_MANCHE_REVERSE_PULSE_WIDTH + RFID_MANCHE_PULSE_BIAS)))
    {
	// received reverse pulse
	is_reverse_bit = true;
    }
    else if((capture_value > (RFID_MANCHE_PULSE_WIDTH + RFID_MANCHE_PULSE_BIAS))
	|| (capture_value < (RFID_MANCHE_REVERSE_PULSE_WIDTH - RFID_MANCHE_PULSE_BIAS)))
    {
	// received invalid bit pulse
	// maybe something wrong happend
	// reset valid_bits seek head again
	valid_bits = 0;
	return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_RECEIVE_DATA_CHANNEL_FLAG);
    }
    // else
    // receive non-reverse pulse

    // receive new bit
    valid_bits++;

    if(SEEK_HEADER == g_recv_data_state)
    {
	// received reverse pulse
	// that means not receive 9 bit1 sequently.
	// seek header again
	if(is_reverse_bit)
	{
            valid_bits = 0;
	    return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_RECEIVE_DATA_CHANNEL_FLAG);
	}

	// found header
	// receive data 
	if(RFID_HEADER_BITS == valid_bits)
	{
          g_recv_data_state = RECEIVE_DATA;
	}
    }
    else if(RECEIVE_DATA == g_recv_data_state)
    {
	// if reach here, 10bits(9 header bits + 1 new bit) had been received at least
	g_rfid_bits_buffer[valid_bits] = (g_rfid_bits_buffer[valid_bits - 1] ^ (unsigned char)is_reverse_bit) & 0x1;

	if(RFID_EM4100_DATA_BITS == valid_bits)
	{
            g_recv_data_state = DATA_READY;
	    // collect all expect bits, reset the bits counter
	    valid_bits = 0;
	}
    }
    else if(DATA_READY == g_recv_data_state)
    {
	// clear valid_bits in case enter interrupt service again
	// before main loop disable transmit
	valid_bits = 0;
    }
    else
    {
	// should not run here
	PRINTF("Seems something wrong happend. \r\n");
    }

    /* Clear interrupt flag.*/
    return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_RECEIVE_DATA_CHANNEL_FLAG);
}
