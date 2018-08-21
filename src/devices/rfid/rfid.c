
#include <stdio.h>

#include "fsl_debug_console.h"
#include <fsl_tpm.h>

#include "configuration.h"
#include "rfid.h"

bool g_is_transmitting = false;
workMode g_work_mode = READ;
recvDataState g_recv_data_state = SEEK_HEADER;
unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
rfidTag g_rfid_tag;
unsigned char g_rfid_pulse_width[RFID_EM4100_DATA_BITS];

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

  for(unsigned char i = 0;i < RFID_EM4100_DATA_BITS;i++)
  {
    g_rfid_pulse_width[i] = 0x0;
  }

  g_rfid_tag.version_number = 0xFF;
  g_rfid_tag.tag_id = 0xFFFFFFFF;


  return;
}
// enable transmit 125khz carrier
// set carrier used gpio pin
void rfid_enable_carrier()
{
  // enable receive data only in read mode
  if(READ == g_work_mode)
  {
    // set counter start at maxmium
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

status_t rfid_parity_check()
{
    status_t result = kStatus_Fail;
    unsigned char column_parity[4] = {0, 0, 0, 0};
    unsigned char *check_bits_ptr = &g_rfid_bits_buffer[RFID_HEADER_BITS];

#ifdef RFID_DBG_PARITiY
    unsigned char bit_counts = 0;
#endif

    for(unsigned char i = 0; i < RFID_BIT_GROUPS;i++)
    {
	unsigned char raw_parity = 0;
	// calculate how many 1 in the first 4bits of each group
	for(unsigned char h = 0; h < RFID_BITS_EACH_GROUP - 1; h++)
	{
	    unsigned char check_bit = *check_bits_ptr++;
	    raw_parity += check_bit; 
	    column_parity[h] += check_bit;
#ifdef RFID_DBG_PARITiY
	    PRINTF("INFO: Bit %d. raw %d parity is %d, column %d parity is %d", bit_counts++, i, raw_parity & 1, h, column_parity[h] & 1);
#endif
	}
	// the last bit of each group indicate 1 numbers
#ifdef RFID_DBG_PARITiY
	PRINTF("INFO: Bit %d. Check Raw %d parity", bit_counts++, i);
#endif
	if((raw_parity & 1) != *check_bits_ptr++)
	{
	    PRINTF("ERROR: Parity error in raw %d parity.\r\n", i);
            result = kStatus_Fail;
	    break;
	}
    }
    // last group contains column parity
    for(unsigned char p = 0;p < 4;p++)
    {
#ifdef RFID_DBG_PARITiY
	PRINTF("INFO: Bit %d. Check Column %d parity", bit_counts++, p);
#endif
	if((column_parity[p] & 1) != *check_bits_ptr++)
	{
#ifdef RFID_DBG_PARITiY
	    PRINTF("ERROR: Parity error in column %d parity. \r\n", p);
#endif
            result = kStatus_Fail;
	    break;
	}
    }

    // the last bit should always be 0
#ifdef RFID_DBG_PARITiY
    PRINTF("INFO: Bit %d. The last bit is stop bit, should be 0.", bit_counts++);
#endif
    if(*check_bits_ptr)
    {
#ifdef RFID_DBG_PARITY
	    PRINTF("ERROR: The last bit is not 0. \r\n", (p+1));
#endif
            result = kStatus_Fail;
    }

    return result;
}

// parse tag data
status_t rfid_parse_data(rfidTag *rfid_tag_ptr)
{
    status_t result = kStatus_Fail;
    unsigned char *tag_bits_ptr = &g_rfid_bits_buffer[RFID_HEADER_BITS];
    
    result = rfid_parity_check();    

    if(kStatus_Success == result)
    {
	// first 8bits is version number/customer ID
	rfid_tag_ptr->version_number = 0;
	for(unsigned char i = 0;i < 8;i++)
	{
	    rfid_tag_ptr->version_number |= (*tag_bits_ptr << i); 
	    tag_bits_ptr++;
	}
        // next 32bits is tag id
	rfid_tag_ptr->tag_id = 0;
	for(unsigned char i = 0;i < 32;i++)
	{
	    rfid_tag_ptr->tag_id |= (*tag_bits_ptr << i); 
	    tag_bits_ptr++;
	}
    }
    return result;
}

void RFID_RECEIVE_DATA_HANDLER(void)
{
    static unsigned char valid_bits = 0;
    bool is_reverse_bit = 0; // don't reverse bit by default

    uint32_t capture_value = RFID_TRANSMIT_TPM->CONTROLS[RFID_RECEIVE_DATA_CHANNEL].CnV;
#ifdef RFID_DBG_RECV
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
