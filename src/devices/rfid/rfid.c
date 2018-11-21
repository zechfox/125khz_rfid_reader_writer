
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
unsigned char g_rfid_carrier_cycle_counter = 0;
unsigned char g_rfid_bit_period = 0;
#ifdef RFID_DBG 
unsigned int g_rfid_dbg_counter = 0;
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
    /* Enable RFID receiver interrupt */
    PORT_SetPinInterruptConfig(RFID_RECEIVER_DATA_PORT, RFID_RECEIVER_DATA_GPIO_INDEX, kPORT_InterruptRisingEdge);

    /* Enable at the NVIC */
    EnableIRQ(RFID_RECEIVE_DATA_INTERRUPT_NUMBER);
  }

  TPM_StartTimer(RFID_TRANSMIT_TPM, kTPM_SystemClock);
  g_is_transmitting = true;
  g_recv_data_state = DETERMINE_PERIOD;
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
	    PRINTF("INFO: Bit %d. raw %d parity is %d, column %d parity is %d. \r\n", bit_counts++, i, raw_parity & 1, h, column_parity[h] & 1);
#endif
	}
	// the last bit of each group indicate 1 numbers
#ifdef RFID_DBG_PARITiY
	PRINTF("INFO: Bit %d. Check Raw %d parity.\r\n", bit_counts++, i);
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
	PRINTF("INFO: Bit %d. Check Column %d parity.\r\n", bit_counts++, p);
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
    PRINTF("INFO: Bit %d. The last bit is stop bit, should be 0.\r\n", bit_counts++);
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

void RFID_RECEIVER_DATA_HANDLER(void)
{
    static unsigned char max_rise_edge_gap = 0;
    static unsigned char min_rise_edge_gap = 0xff;
    static unsigned char rise_edge_counter = 0;

    unsigned int pin_mask = PORT_GetPinsInterruptFlags(RFID_RECEIVER_DATA_PORT);
    // handle rise edge interrupt of rfid receiver pin 
    if(pin_mask & (1 << RFID_RECEIVER_DATA_GPIO_INDEX))
    {
	// start count carrier cycle
	if(0 == g_rfid_carrier_cycle_counter)
	{
	    g_rfid_count_carrier_cycle_flag = true; 
	}
	// just ignore the glitch
	else if(g_rfid_carrier_cycle_counter > RFID_GLITCH_CYCLE)
	{
	    switch(g_recv_data_state)
	    {
		case DETERMINE_PERIOD:
		    rise_edge_counter++;

		    if(min_rise_edge_gap > g_rfid_carrier_cycle_counter)
		    {
			min_rise_edge_gap = g_rfid_carrier_cycle_counter;
		    }
		    if(max_rise_edge_gap < g_rfid_carrier_cycle_counter)
		    {
			max_rise_edge_gap = g_rfid_carrier_cycle_counter;
		    }

		    if(rise_edge_counter > RFID_MAX_COUNTER)
		    {
			rise_edge_counter = 0;
			unsigned max_gap_upbound = max_rise_edge_gap + RFID_CYCLE_OFFSET;
			unsigned max_gap_downbound = max_rise_edge_gap - RFID_CYCLE_OFFSET;
			// max rise edge gap = 2 * min rise edge gap, biploar
			if(max_gap_upbound > 
				(max_rise_edge_gap << 1) >
				max_gap_downbound)
			{

			}
			// max rise edge gap = 1.5 min rise edge gap, menchester or psk
			else if(max_gap_upbound > 
				(3 * (min_rise_edge_gap >> 1)) >
				max_gap_downbound)
			{


			    // min rise edge gap = 16, psk
			    if(RFID_CYCLE_PSK_UPBOUND > min_rise_edge_gap > RFID_CYCLE_PSK_DOWNBOUND)
			    {

			    }
			}
		    }
		    break;
		case SYNC_HEAD:

		    break;

		case RECEIVE_DATA:

		    break;

		case DATA_READY:

		    break;
		    // something wrong
		default:
		    break;
	    }
	    // count from current rise edge to next one.
	    g_rfid_carrier_cycle_counter = 0;
	} //else if(g_rfid_carrier_cycle_counter > RFID_GLITCH_CYCLE)
    } //if(pin_mask & (1 << RFID_RECEIVER_DATA_GPIO_INDEX))

    // just clear what interrupts
    // we do not care other pin interrupt
    // and we also need clear RFID_RECEIVER_DATA_GPIO_INDEX
    return PORT_ClearPinsInterruptFlags(RFID_RECEIVER_DATA_PORT, pin_mask);
}
