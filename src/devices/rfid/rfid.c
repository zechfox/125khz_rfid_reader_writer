
#include <stdio.h>
#include <limits.h>

#include "fsl_debug_console.h"
#include <fsl_tpm.h>

#include "configuration.h"
#include "rfid.h"

workMode g_work_mode = READ;
readerState g_reader_state = IDLE;
unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
unsigned int g_rfid_bits[RFID_EM4100_DATA_BITS/ CHAR_BIT * sizeof (int)];
rfidTag g_rfid_tag;
#ifdef RFID_DBG_RECV
unsigned char g_rfid_rise_edge_width[RFID_DETECT_RISE_EDGE_NUMBER];
#endif
unsigned char g_rfid_carrier_cycle_counter = 0;
bool g_rfid_count_carrier_cycle_flag = false;
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
  g_rfid_bits[0] = 0x1FF;
  g_rfid_bits[1] = 0;
#ifdef RFID_DBG_RECV
  for(unsigned char i = 0;i < RFID_DETECT_RISE_EDGE_NUMBER;i++)
  {
    g_rfid_rise_edge_width[i] = 0x0;
  }
#endif

  g_rfid_tag.version_number = 0xFF;
  g_rfid_tag.tag_id = 0xFFFFFFFF;
  g_rfid_tag.encode_scheme = UNKNOWN;
  g_rfid_tag.bit_length = 0xFF;

  PRINTF("RFID Initialized. \r\n");

  return;
}

// reset rfid state same as startup
void rfid_reset()
{
    PRINTF("RFID Resetting. \r\n");

    rfid_disable_carrier();
    g_work_mode = READ;
    g_reader_state = IDLE;
    g_rfid_carrier_cycle_counter = 0;
    g_rfid_bit_period = 0;
#ifdef RFID_DBG 
    unsigned int g_rfid_dbg_counter = 0;
#endif

    rfid_init();

    PRINTF("RFID Reseted. \r\n");

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
    /* Enable TPM Channel1 interrupt */
    TPM_EnableInterrupts(RFID_TRANSMIT_TPM, RFID_TRANSMITTER_INTERRUPT_ENABLE);
    /* Enable at the NVIC */
    // gpio interrupt
    EnableIRQ(RFID_RECEIVER_DATA_INTERRUPT_NUMBER);
    // tpm interrupt
    EnableIRQ(RFID_TRANSMITTER_INTERRUPT_NUMBER);
  }

  TPM_StartTimer(RFID_TRANSMIT_TPM, kTPM_SystemClock);
  g_reader_state = WAIT_STABLE;
  PRINTF("RFID 125kHz Carrier Enabled. \r\n");
 return;
}

// disable transmit 125khz carrier
// clear carrier used gpio pin
void rfid_disable_carrier()
{
    TPM_StopTimer(RFID_TRANSMIT_TPM);
    DisableIRQ(RFID_RECEIVER_DATA_INTERRUPT_NUMBER);
    DisableIRQ(RFID_TRANSMITTER_INTERRUPT_NUMBER);
    g_reader_state = IDLE;
    PRINTF("RFID 125kHz Carrier Disabled. \r\n");
    return;
}

status_t rfid_parity_check()
{
    status_t result = kStatus_Fail;
    unsigned char column_parity[4] = {0, 0, 0, 0};
    unsigned char *check_bits_ptr = &g_rfid_bits_buffer[RFID_HEADER_BITS];

    // Bit31 .... Bit0
    // D18 D17 D16 P3 D15 D14 D13 D12 P2 D11 D10 D09 D08 P1 D07 D06 D05 P0 D04 D03 D02 D01 D00 1 1 1 1 1 1 1 1 1 
    // Bit63 ...  Bit32
    // S0 PC3 PC2 PC1 PC0 P9 D39 D38 D37 D36 P8 D35 D34 D33 D32 P7 D31 D30 D29 D28 P6 D27 D26 D25 D24 P5 D23 D22 D21 D20 P4 D19
    // 1 1 1 1 1 1 1 1 1  ---> 9 bit header
    // D00 D01 D02 D03 P0
    // D04 D05 D06 D07 P1
    // D08 D09 D10 D11 P2
    // D12 D13 D14 D15 P3
    // D16 D17 D18 D19 P4
    // D20 D21 D22 D23 P5
    // D24 D25 D26 D27 P6
    // D28 D29 D30 D31 P7
    // D32 D33 D34 D35 P8
    // D36 D37 D38 D39 P9
    // PC0 PC1 PC2 PC3 S0 ---> stop bit 0

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

void RFID_TRANSMITTER_HANDLER(void)
{

    if(g_rfid_count_carrier_cycle_flag)
    {
	g_rfid_carrier_cycle_counter++;
    }

    if(g_rfid_carrier_cycle_counter > RFID_TAG_LEAVE_COUNTER)
    {
	// reset rfid
	g_reader_state = RESET;
    }
	

    return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_TRNSIMIT_INTERRUPT_FLAG);
}

void RFID_RECEIVER_DATA_HANDLER(void)
{
    static unsigned char max_rise_edge_gap = 0;
    static unsigned char min_rise_edge_gap = 0xff;
    static unsigned char rise_edge_counter = 0;
    static unsigned char last_detected_gap = 0;
    static unsigned char received_bits = 9;

    unsigned char gap_upbound = 0;
    unsigned char gap_downbound = 0; 

    unsigned int pin_mask = PORT_GetPinsInterruptFlags(RFID_RECEIVER_DATA_PORT);

    // handle rise edge interrupt of rfid receiver pin 
    if(pin_mask & (1 << RFID_RECEIVER_DATA_GPIO_INDEX))
    {
	// detect first rise edge,
	// start count carrier cycle
	if(0 == g_rfid_carrier_cycle_counter)
	{
	    g_rfid_count_carrier_cycle_flag = true; 
	}
	//  
	else
	{
	    switch(g_reader_state)
	    {
		case WAIT_STABLE:
		    rise_edge_counter++;
		    if(rise_edge_counter > 100)
		    {
			g_reader_state = DETECT_PERIOD;
			rise_edge_counter = 0;
			g_rfid_carrier_cycle_counter = 0;
		    }
		    break;
		case DETECT_PERIOD:
#ifdef RFID_DBG_RECV
		    g_rfid_rise_edge_width[rise_edge_counter] = g_rfid_carrier_cycle_counter;
#endif
		    rise_edge_counter++;

		    if(min_rise_edge_gap > g_rfid_carrier_cycle_counter)
		    {
			min_rise_edge_gap = g_rfid_carrier_cycle_counter;
		    }
		    if(max_rise_edge_gap < g_rfid_carrier_cycle_counter)
		    {
			max_rise_edge_gap = g_rfid_carrier_cycle_counter;
		    }

		    if(rise_edge_counter > RFID_DETECT_RISE_EDGE_NUMBER)
		    {
			rise_edge_counter = 0;
			unsigned int max_gap_upbound = max_rise_edge_gap + RFID_CYCLE_OFFSET;
			unsigned int max_gap_downbound = max_rise_edge_gap - RFID_CYCLE_OFFSET;
			unsigned int half_max_rise_edge_gap = max_rise_edge_gap << 1;
			unsigned int one_half_min_rise_edge_gap = 3 * min_rise_edge_gap >> 1;

			// min rise edge gap is the bit length cycle
			g_rfid_tag.bit_length = min_rise_edge_gap;
			// max rise edge gap = 2 * min rise edge gap, biphase 
			if((max_gap_upbound > half_max_rise_edge_gap)
				&& (max_gap_downbound < half_max_rise_edge_gap))
			{
			    g_rfid_tag.encode_scheme = BIPHASE;
			}
			// max rise edge gap = 1.5 min rise edge gap, menchester or psk
			else if((max_gap_upbound > one_half_min_rise_edge_gap)
				&& (one_half_min_rise_edge_gap > max_gap_downbound))
			{
			    // min rise edge gap = 16, psk
			    if((RFID_CYCLE_PSK_UPBOUND > min_rise_edge_gap)
				    && (min_rise_edge_gap > RFID_CYCLE_PSK_DOWNBOUND))
			    {
				g_rfid_tag.encode_scheme = PSK;
			    }
			    else
			    {
				g_rfid_tag.encode_scheme = MANCHESTER;
			    }
			}
			// we already know the bit length, sync head by it.
			g_reader_state = SYNC_HEAD;
#ifdef RFID_DBG_RECV
			// terminate reader for debug reason 
			g_reader_state = PARSE_DATA;
#endif
			max_rise_edge_gap = 0;
			min_rise_edge_gap = 0xff;
			rise_edge_counter = 0;
		    }
		    break;
		case SYNC_HEAD:
		    gap_upbound = last_detected_gap + RFID_CYCLE_OFFSET;
		    gap_downbound = last_detected_gap - RFID_CYCLE_OFFSET;
#ifdef RFID_DBG_SYNC_HEAD

#endif

		    // same as previous gap
		    if((gap_upbound > g_rfid_carrier_cycle_counter)
			    && (g_rfid_carrier_cycle_counter > gap_downbound))
		    {
			rise_edge_counter++;
			if(MANCHESTER == g_rfid_tag.encode_scheme 
				&& (rise_edge_counter >=8))
			{
			    // sync head by MANCHESTER 
			    g_reader_state = READ_DATA;
			    last_detected_gap = 0;
			}
			else if(BIPHASE == g_rfid_tag.encode_scheme
				&& (rise_edge_counter >=5))
			{
			    // sync head by BIPHASE
			    g_reader_state = READ_DATA;
			    last_detected_gap = 0;
			}
		    }
		    // not same as previous one
		    else
		    {
			rise_edge_counter = 0;
		    }

		    // replace by new one
		    last_detected_gap = g_rfid_carrier_cycle_counter;

		    break;

		case READ_DATA:

		    if(MANCHESTER == g_rfid_tag.encode_scheme)
		    {
			gap_upbound = g_rfid_tag.bit_length + RFID_CYCLE_OFFSET;
			gap_downbound = g_rfid_tag.bit_length - RFID_CYCLE_OFFSET;
			// 1 bit cycle, receive bit same as previous
			// use one bound for easy
			if(g_rfid_carrier_cycle_counter < gap_upbound )
			{
                            g_rfid_bits_buffer[received_bits] = g_rfid_bits_buffer[received_bits - 1];
			    received_bits++;
			}
			// 1.5 bit cycle, receive 2bit, one same as previous, the other is reversed
			// use one bound for easy
			else if(g_rfid_carrier_cycle_counter < (3 * (g_rfid_tag.bit_length >> 2)))
			{
			    g_rfid_bits_buffer[received_bits] = g_rfid_bits_buffer[received_bits - 1];
			    received_bits++;

			    g_rfid_bits_buffer[received_bits] = 0x1 & (g_rfid_bits_buffer[received_bits - 1] + 1);
			    received_bits++;
			}
			
		    }
		    else if(BIPHASE == g_rfid_tag.encode_scheme)
		    {

		    }
		    else
		    {
			// only manchester and biphase supported, treat psk as unknown
			g_reader_state = RESET;
		    }

		    if(received_bits >= RFID_EM4100_DATA_BITS)
		    {
			g_reader_state = PARSE_DATA;
			received_bits = 9;
		    }

		    break;

		// something wrong, other state won't here.
		default:
		    break;
	    }
	    // count from current rise edge to next one.
	    g_rfid_carrier_cycle_counter = 0;
	} //else
    } //if(pin_mask & (1 << RFID_RECEIVER_DATA_GPIO_INDEX))

    // just clear what interrupts
    // we do not care other pin interrupt
    // and we also need clear RFID_RECEIVER_DATA_GPIO_INDEX
    return PORT_ClearPinsInterruptFlags(RFID_RECEIVER_DATA_PORT, pin_mask);
}
