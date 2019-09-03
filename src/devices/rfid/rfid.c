
#include <stdio.h>
#include <limits.h>

#include "fsl_debug_console.h"
#include <fsl_tpm.h>

#include "configuration.h"
#include "rfid.h"

workMode g_work_mode = READ;
volatile readerState g_reader_state = IDLE;
unsigned char g_rfid_bits_buffer[RFID_EM4100_DATA_BITS];
unsigned int g_rfid_bits[RFID_EM4100_DATA_BITS/ CHAR_BIT * sizeof (int)];
unsigned char g_rfid_mod_waveform[RFID_WAVEFORM_ARRAY_SIZE];
volatile unsigned int g_rfid_carrier_cycle_counter = 0;
unsigned char g_rfid_waveform_counter = 0;
unsigned int g_rfid_check_point = 0;
volatile bool g_rfid_is_edge_detected = false;
volatile bool g_rfid_is_check_point = false;
volatile unsigned char g_rise_edge_counter = 0;
#ifdef RFID_DBG 
unsigned int g_rfid_dbg_counter = 0;
#endif

// initial rfid
void rfid_init(rfidTag * rfid_tag_ptr)
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

  for(unsigned char i = 0;i < RFID_WAVEFORM_ARRAY_SIZE;i++)
  {
    g_rfid_mod_waveform[i] = 0x0;
  }

  if(rfid_tag_ptr)
  {
    rfid_tag_ptr->customer_spec_id = 0xFF;
    rfid_tag_ptr->tag_data = 0xFFFFFFFF;
    rfid_tag_ptr->encode_scheme = UNKNOWN;
    rfid_tag_ptr->bit_length = 0xFF;
  }

  PRINTF("RFID Initialized. \r\n");

  return;
}

// reset rfid state same as startup
void rfid_reset(rfidTag * rfid_tag_ptr)
{
  PRINTF("RFID Resetting. \r\n");
  g_work_mode = READ;
  g_reader_state = IDLE;
  g_rfid_carrier_cycle_counter = 0;
  g_rfid_check_point = 0;
  g_rfid_is_check_point = false;
  g_rfid_is_edge_detected = false;
  g_rfid_waveform_counter = 0;
#ifdef RFID_DBG 
  g_rfid_dbg_counter = 0;
#endif

  rfid_init(rfid_tag_ptr);

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

void rfid_format_for_parity_check(unsigned char * unformat_data, unsigned char * formatted_data)
{
  //    unformmatted                                  formatted
  //
  // D06 D05 D04 P0  D03 D02 D01 D00               P0 D03 D02 D01 D00
  // D12 P2  D11 D10 D09 D08 P1  D07               P1 D07 D06 D05 D04
  // D19 D18 D17 D16 P3  D15 D14 D13    ====\      P2 D11 D10 D09 D08
  // D25 D24 P5  D23 D22 D21 D20 P4     ====/      P3 D15 D14 D13 D12
  // P7  D31 D30 D29 D28 P6  D27 D26               P4 D19 D18 D17 D16
  // D38 D37 D36 P8  D35 D34 D33 D32               P5 D23 D22 D21 D20
  // xxx S0  PC3 PC2 PC1 PC0 P9  D39               P6 D27 D26 D25 D24
  //                                               P7 D31 D30 D29 D28
  //                                               P8 D35 D34 D33 D32
  //                                               P9 D39 D38 D37 D36
  //                                               S0 PC3 PC2 PC1 PC0
  unsigned char tmp_bit = 0;
#ifdef RFID_DBG_PARSE_DATA
  PRINTF(">>> PARSE DATA >>> Format data for parity check.\r\n");
  PRINTF(">>> PARSE DATA >>> Unformatted Data: \r\n");
  for(unsigned char i = 0;i < RFID_EM4100_DATA_BUFFER_SIZE; i++)
  {
    PRINTF("0x%04x == 0b%08b \r\n", unformat_data[i], unformat_data[i]);
  }
#endif
  for(unsigned char i = 0; i < RFID_BIT_GROUPS; i++)
  {
    formatted_data[i] = 0;
  }

  for(unsigned char i = 0;i < RFID_EM4100_DATA_BITS - RFID_HEADER_BITS; i++)
  {
    unsigned char src_array_index = i / 8;
    unsigned char src_bit_index = i % 8;
    unsigned char dest_array_index = i / 5;
    unsigned char dest_bit_index = i % 5;

    tmp_bit = (unformat_data[src_array_index] >> src_bit_index) & 1;
    formatted_data[dest_array_index] |= tmp_bit << dest_bit_index;
  }

#ifdef RFID_DBG_PARSE_DATA
  PRINTF(">>> PARSE DATA >>> Formatted data:\r\n");
  for(unsigned char i = 0; i < RFID_BIT_GROUPS; i++)
  {
    PRINTF("0x%04x == 0b%08b \r\n", formatted_data[i], formatted_data[i]);
  }
#endif
}

bool rfid_get_bit_length(unsigned char cycles, unsigned char * bit_length_ptr)
{
  unsigned char bit_length = cycles;
  bool is_cycle_valid = true;
  // bit length will be 16, 32, 64
  if ( cycles > 59 
       && cycles < 69)
  {
    bit_length = 64;
  }
  else if ( cycles > 27 
       && cycles < 37)
  {
    bit_length = 32;
  }
  else if ( cycles > 11 
       && cycles < 21)
  {
    bit_length = 16;
  }
  else
  {
    is_cycle_valid = false;
    PRINTF("ERROR: CAN'T determine bit length. cycles is %d.\r\n", cycles);
  }
  
  *bit_length_ptr = bit_length;

  return is_cycle_valid;
  
}

status_t rfid_receive_data(rfidTag * rfid_tag_ptr)
{
  status_t result = kStatus_Fail;
  static unsigned char s_min_rise_edge_gap = 0xff;
  if(NULL == rfid_tag_ptr)
  {
    PRINTF("ERROR: Tag is Null pointer!\r\n");
    return result;
  }

  if (WAIT_STABLE == g_reader_state)
  {
    if(g_rise_edge_counter > 200)
    {
      // definitly a card, try to detect its period.
      g_reader_state = DETECT_PERIOD;
      g_rise_edge_counter = 0;
      g_rfid_is_edge_detected = false;
    }
  }
  else if (DETECT_PERIOD == g_reader_state)
  {
    if(g_rfid_is_edge_detected)
    {
      unsigned char rise_edge_gap = 0xFF & g_rfid_carrier_cycle_counter;
      // ignore the counter if it's too small
      // TODO: remove the small check, it impedes PSK.
      if(s_min_rise_edge_gap > rise_edge_gap
        && rise_edge_gap > 2)
      {
        s_min_rise_edge_gap = rise_edge_gap;
      }

      if(g_rise_edge_counter > RFID_DETECT_RISE_EDGE_NUMBER)
      {
        // min rise edge gap is the bit length cycle
        if(rfid_get_bit_length(s_min_rise_edge_gap, &rfid_tag_ptr->bit_length))
        {
          // we already know the bit length, sync head by it.
          g_reader_state = COLLECT_DATA;

        }
        else
        {
          // TODO: double check PSK profile
          if (rfid_tag_ptr->bit_length < 5)
          {
            rfid_tag_ptr->encode_scheme = PSK;
          }
          g_reader_state = DETECT_PERIOD;
        }
        s_min_rise_edge_gap = 0xff;
        g_rfid_waveform_counter = 0;
      }
      g_rfid_is_edge_detected = false;
      g_rfid_carrier_cycle_counter = 0;
    }
  }
  else if (COLLECT_DATA == g_reader_state)
  {
    // collect data
    // 1 -> high level
    // 0 -> low level
    // each bit is half bit length period.

    if(g_rfid_is_edge_detected)
    {
      // rise edge
      // g_rfid_carrier_cycle_counter only reset after rise edge
      // because it's easy to know 1/4 bit length check point.
      // It's not record logic level when met rise edge, because
      // it could be a noise, check the logic 1/4 bit length later.
      g_rfid_carrier_cycle_counter = 0;
      g_rfid_check_point = rfid_tag_ptr->bit_length / 4;
      g_rfid_is_edge_detected = false;
    }
    else if(g_rfid_is_check_point)
    {
      // check point of logic level
      unsigned int logic_level  = GPIO_ReadPinInput(RFID_RECEIVER_DATA_GPIO_PORT, RFID_RECEIVER_DATA_GPIO_INDEX);
      unsigned char array_index = g_rfid_waveform_counter / 8;
      unsigned char bit_index   = g_rfid_waveform_counter % 8;
      if(g_rfid_carrier_cycle_counter < rfid_tag_ptr->bit_length / 2)
      {
        // it must be high level, because it's first check point after a rise
        // edge. 
        // If it's low level, just skip it.
        if(logic_level)
        {
          g_rfid_mod_waveform[array_index] |= (1 << bit_index);
          g_rfid_waveform_counter++;
          // push check point to 1/2 bit length further.
          g_rfid_check_point = g_rfid_carrier_cycle_counter + rfid_tag_ptr->bit_length / 2;
        }
      }
      else
      {
        // not the first check point
        g_rfid_mod_waveform[array_index] |= (logic_level << bit_index);
        g_rfid_waveform_counter++;
        // push check point to 1/2 bit length further.
        g_rfid_check_point = g_rfid_carrier_cycle_counter + rfid_tag_ptr->bit_length / 2;
      }

      if(g_rfid_waveform_counter == (RFID_WAVEFORM_RECORDS_NUMBER - 1))
      {
        // has recorded 256 waveform records,
        // goto parse state
        g_reader_state = PARSE_DATA;
        g_rfid_check_point = 0;
      }
      g_rfid_is_check_point = false;
    }
  }
  else if (PARSE_DATA == g_reader_state)
  {
    // got enough data, disable carrier before parse it.
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> Parse Data >>> Tag Period: %d, RFID Waveform records: . \r\n", rfid_tag_ptr->bit_length);
    for(unsigned char i = 0; i < RFID_WAVEFORM_ARRAY_SIZE; i++)
    {
      PRINTF("0x%02x \r\n", g_rfid_mod_waveform[i]);
    }
#endif
    result = rfid_parse_data(rfid_tag_ptr);
    if(kStatus_Fail == result)
    {
      // restart 
      PRINTF(">>> PARSE DATA >>> Failed, reset.\r\n");
      rfid_reset(rfid_tag_ptr);
    }
    g_reader_state = RESET;
  }
  else if ( RESET == g_reader_state )
  {
    PRINTF("INFO: RESET reader state. \r\n");
    rfid_disable_carrier();
    rfid_reset(rfid_tag_ptr);
    rfid_enable_carrier();
  }

  return result;
}

// parse tag data
status_t rfid_parse_data(rfidTag *rfid_tag_ptr)
{
  status_t result = kStatus_Fail;
  unsigned char tag_payload[RFID_EM4100_PAYLOAD_BUFFER_SIZE];
  unsigned char * modulated_data_ptr = g_rfid_mod_waveform;

  if(kStatus_Success == rfid_parse_data_manchester(modulated_data_ptr, tag_payload))
  {
    rfid_tag_ptr->encode_scheme = MANCHESTER;
  }
  else if(kStatus_Success == rfid_parse_data_biphase(modulated_data_ptr, tag_payload))
  {
    rfid_tag_ptr->encode_scheme = BIPHASE;
  }

  if(UNKNOWN != rfid_tag_ptr->encode_scheme)
  {
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> Parse Data >>> Tag Buffer: \r\n", rfid_tag_ptr->bit_length);
    for(unsigned char i = 0;i < RFID_EM4100_PAYLOAD_BUFFER_SIZE; i++)
    {
      PRINTF("0x%02x\r\n", tag_payload[i]);
    }
#endif
    // first 8bits is version number/customer ID
    rfid_tag_ptr->customer_spec_id = tag_payload[0];

    // next 32bits is tag id
    rfid_tag_ptr->tag_data = 0;
    for(unsigned char i = 1;i < RFID_EM4100_PAYLOAD_BUFFER_SIZE;i++)
    {
      rfid_tag_ptr->tag_data |= (tag_payload[i] << (8 * (i - 1))); 
    }
    result = kStatus_Success;
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> Parse Data >>> Got Tag, Customer Spec ID: 0x%02x, Tag ID: 0x%04x.\r\n", rfid_tag_ptr->customer_spec_id, rfid_tag_ptr->tag_data);
    PRINTF(">>> Parse Data >>> Tag Buffer: \r\n");
    for(unsigned char i = 0;i < RFID_EM4100_PAYLOAD_BUFFER_SIZE; i++)
    {
      PRINTF("0x%02x\r\n", tag_payload[i]);
    }
#endif

  }
  return result;
}

status_t rfid_parse_data_manchester(unsigned char * modulated_data_ptr, unsigned char * tag_payload_ptr)
{
  status_t result = kStatus_Fail;
  bool without_shift_phase = false;
  bool with_shift_phase = true;
  // contains demodulated data, except 9bits header.
  unsigned char bits_buffer[RFID_EM4100_DATA_BUFFER_SIZE];

  if(kStatus_Success == rfid_demodule_data(MANCHESTER, modulated_data_ptr, without_shift_phase, bits_buffer) 
     || kStatus_Success == rfid_demodule_data(MANCHESTER, modulated_data_ptr, with_shift_phase, bits_buffer))
  {
    // try get the tag payload from demoduled data
    // maybe not success if parity check failed.
    if(rfid_try_get_tag_payload(bits_buffer, tag_payload_ptr))
    {
      result = kStatus_Success;
    }
  }

  return result;
}

status_t rfid_parse_data_biphase(unsigned char * modulated_data_ptr, unsigned char * tag_payload_ptr)
{
  status_t result = kStatus_Fail;
  bool without_shift_phase = false;
  bool with_shift_phase = true;
  // contains demodulated data, except 9bits header.
  unsigned char bits_buffer[RFID_EM4100_DATA_BUFFER_SIZE];

  if(kStatus_Success == rfid_demodule_data(BIPHASE, modulated_data_ptr, without_shift_phase, bits_buffer) 
     || kStatus_Success == rfid_demodule_data(BIPHASE, modulated_data_ptr, with_shift_phase, bits_buffer))
  {
    // try get the tag payload from demoduled data
    // maybe not success if parity check failed.
    if(rfid_try_get_tag_payload(bits_buffer, tag_payload_ptr))
    {
      result = kStatus_Success;
    }
  }

  return result;
}

status_t rfid_demodule_data(encodeScheme code_scheme, unsigned char * mod_data, bool shift_phase, unsigned char * demod_data)
{
  status_t result = kStatus_Fail;
  unsigned char header_position = 0;
  bool is_ticked = false;
  bool found_header = false;
  unsigned char tick = 0;
  unsigned char tock = 0;
  unsigned char continue_one_counter = 0;
  // raw data is demodulated from mod_data
  unsigned char raw_data[RFID_EM4100_DATA_BITS * 2 / 8];
  unsigned char (*decoder_ptr)(unsigned char) = &rfid_decode_manchester;

#ifdef RFID_DBG_PARSE_DATA
  PRINTF(">>> PARSE DATA >>> Demodule data, code scheme: %s, shift_phase:%s.\r\n", code_scheme == BIPHASE?"BIPHASE":"MANCHESTER", shift_phase?"True":"False");
#endif
  if(BIPHASE == code_scheme)
  {
    decoder_ptr = &rfid_decode_biphase;
  }

  // clear buffer, before use it.
  for(unsigned char i = 0;i < RFID_EM4100_DATA_BUFFER_SIZE; i++)
  {
    demod_data[i] = 0;
  }

  if (shift_phase)
  {
    is_ticked = true;
    tick = 0;
  }

  for(unsigned short records_index = 0; records_index < RFID_WAVEFORM_RECORDS_NUMBER; records_index++)
  {
    unsigned char array_index = records_index / 8;
    unsigned char bit_index = records_index % 8;
    unsigned char level = (mod_data[array_index] >> bit_index) & 1;
    unsigned char modulated_code = 0;
    unsigned char decoded_bit = 0;
    if(is_ticked)
    {
      is_ticked = false;
      tock = level;
      modulated_code = (tick << 1) | tock;
      decoded_bit = (*decoder_ptr)(modulated_code);
      if(0xFF != decoded_bit)
      {
        // 2bits modulated data generates 1bit demoduled data.
        array_index = (records_index) / 2 / 8;
        bit_index = ((records_index) / 2) % 8;
        raw_data[array_index] |= decoded_bit << bit_index; 
        // check header: 9bits 1.
        if(decoded_bit)
        {
          continue_one_counter++;
        }
        else
        {
          continue_one_counter = 0;
        }

        if(!found_header
           && (9 == continue_one_counter))
        {
#ifdef RFID_DBG_PARSE_DATA
          PRINTF(">>> PARSE DATA >>> Demodule data.  Found Header in %dth records.\r\n", records_index);
#endif
          found_header = true;
          header_position = array_index * 8 + bit_index + 1;
          continue_one_counter = 0;
        }
      }
      else
      {
#ifdef RFID_DBG_PARSE_DATA
        PRINTF(">>> PARSE DATA >>> Demodule data.  Can't decode for %dth records: 0b%02b\r\n", records_index, modulated_code);
#endif       
        break;
      }
    }
    else
    {
      tick = level; 
      is_ticked = true;
    }
  }

  if(!found_header 
     && continue_one_counter > 0)
  {
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Demodule data. Try check wrappered header. Found %d continue bit1.\r\n", continue_one_counter);
#endif
    // continue search (RFID_HEADER_BITS - continue_one_counter) bits further,
    // in case header was wrapped.
    for(unsigned char index = 0; index < RFID_HEADER_BITS - continue_one_counter; index++)
    {
      unsigned char array_index = index / 8;
      unsigned char bit_index = index % 8;
      unsigned char decoded_bit = (raw_data[array_index] >> bit_index) & 1;

      if(1 == decoded_bit)
      {
        continue_one_counter++;
      }
      else
      {
        break;

      }
      if(9 == continue_one_counter)
      {
        found_header = true;
        header_position = RFID_EM4100_DATA_BITS * 2 / 8 - index; 
      }
    }
  }

  if(found_header)
  {
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Demoduled data. \r\n", continue_one_counter);
    for(unsigned char i = 0; i < RFID_EM4100_DATA_BITS * 2 / 8; i++)
    {
      PRINTF("0x%02x - 0b%08b \r\n", raw_data[i], raw_data[i]);
    }
#endif
    rfid_extract_bits(raw_data, demod_data, header_position, RFID_EM4100_DATA_BITS - RFID_HEADER_BITS);
    result = kStatus_Success;
  }

  return result;
}

inline unsigned char rfid_decode_manchester(unsigned char manchester_code)
{
  if(0x2 == manchester_code)
  {
    return 0;
  }
  else if(0x1 == manchester_code)
  {
    return 1;
  }
  else
  {
    return 0xFF;
  }

}

inline unsigned char rfid_decode_biphase(unsigned char biphase_code)
{
  if(0x3 == biphase_code)
  {
    return 1;
  }
  else if(0x1 == biphase_code)
  {
    return 0;
  }
  else if(0x2 == biphase_code)
  {
    return 0;
  }
  else if(0x0 == biphase_code)
  {
    return 1;
  }
  else
  {
    return 0xFF;
  }
}

void rfid_extract_bits(unsigned char * source, unsigned char * destination, unsigned char start_bit_index, unsigned char number)
{
  unsigned char slot1 = 0, slot2 = 0;
  unsigned char slot1_array_index = 0;
  unsigned char slot2_array_index = 0;
  unsigned char slot_boundary = start_bit_index % 8;
  unsigned char slot1_mask = (1 << slot_boundary) - 1;
  unsigned char slot2_mask = slot1_mask ^ 0xFF;
  unsigned char slot1_bits_num = slot_boundary;
  unsigned char slot2_bits_num = 8 - slot_boundary;
  unsigned char extracted_bits_num = 0;

#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Demodule data. Extract Bits from %d bit.\r\n", start_bit_index);
#endif
  // example:
  // source array1 |yyyyy xxx|, slot2 is yyyyy
  // source array2 |wwwww zzz|, slot1 is zzz
  // destination array |zzz yyyyy|
  do
  {
    slot2_array_index = (start_bit_index + extracted_bits_num) / 8;
    slot2 = ((source[slot2_array_index] & slot2_mask) >> slot1_bits_num);
    // this maybe cause overflow of unsigned char, but it's designd to be
    // We want to collect the bits from array beginining.
    slot1_array_index = (start_bit_index + extracted_bits_num + 8) / 8;
    slot1 = (source[slot1_array_index] & slot1_mask) << slot2_bits_num;

    destination[extracted_bits_num / 8] = (slot1 | slot2);
    extracted_bits_num += 8;
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Demodule data. Extract Bits: 0xb%08b.\r\n", slot1|slot2);
#endif
  }while(extracted_bits_num < number);

}

bool rfid_try_get_tag_payload(unsigned char * check_data, unsigned char * tag_payload_ptr)
{
  bool result = false;
  unsigned char formatted_data[RFID_BIT_GROUPS];

#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Try get tag payload.\r\n");
#endif

  rfid_format_for_parity_check(check_data, formatted_data);
  if(rfid_parity_check(formatted_data))
  {
    for(unsigned char i = 0; i < RFID_EM4100_PAYLOAD_BUFFER_SIZE; i++)
    {
      tag_payload_ptr[i] = 0;
    }
    // discards parity bits, get payload
    rfid_get_tag_payload(formatted_data, tag_payload_ptr);
    result = true;
  }

  return result;

}

bool rfid_parity_check(unsigned char * formatted_data)
{
  bool result = true;
  unsigned char column_parity = 0;
  unsigned char * bit_ptr = formatted_data;
  unsigned char parity_lookup_table[16] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};

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
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Parity Check.\r\n");
#endif
  // calculates column parity, ignore the last group of column parity.
  // check row pairty
  for(unsigned char i = 0;i < RFID_BIT_GROUPS - 1; i++)
  {
    unsigned char data = bit_ptr[i] & 0xF;
    unsigned char parity = (bit_ptr[i] & 0x10) >> 4;

    if(parity != parity_lookup_table[data])
    {
      PRINTF("ERROR: Row %d parity check failed.  Data:%d -> 0b%04b, Parity is: %d.\r\n", i, data, data, parity);
      result = false;
      break;
    }
    column_parity ^= data;
  }

  if( column_parity != ( bit_ptr[RFID_BIT_GROUPS - 1] & 0xF  ) )
  {
    PRINTF("ERROR: Column parity check failed.  Calculated Parity is: 0b%04b, Collected Parity is: 0b%04b.\r\n", column_parity, bit_ptr[RFID_BIT_GROUPS - 1] & 0xF);
    result = false;
  }

  // stop bits
  if( 0 != ( bit_ptr[RFID_BIT_GROUPS - 1] & 0x10 ) )
  {
    PRINTF("ERROR: Stop Bit is not 0.\r\n");
    result = false;
  }

  return result;
}

void rfid_get_tag_payload(unsigned char * formatted_data_ptr, unsigned char * tag_payload_ptr)
{
  unsigned char * src_ptr = formatted_data_ptr;
  unsigned char * dest_ptr = tag_payload_ptr;
  unsigned tmp_data = 0;
#ifdef RFID_DBG_PARSE_DATA
    PRINTF(">>> PARSE DATA >>> Get Tag Payload.\r\n");
#endif
  for(unsigned char i = 0;i < RFID_BIT_GROUPS - 1; i++)
  {
    if(1 == (i & 1))
    {
      tmp_data |= (src_ptr[i] & 0xF) << 4;
      *dest_ptr++ = tmp_data;
    }
    else
    {
      tmp_data = src_ptr[i] & 0xF;
    }
  }

}

void RFID_TRANSMITTER_HANDLER(void)
{

  // counter cycle if detect period
  if(g_reader_state >= DETECT_PERIOD)
  {
    g_rfid_carrier_cycle_counter++;
  }

  if(g_rfid_check_point
     && g_rfid_carrier_cycle_counter == g_rfid_check_point)
  {
    g_rfid_is_check_point = true;
  }

  if(g_rfid_carrier_cycle_counter > RFID_TAG_LEAVE_COUNTER)
  {
    // reset rfid
    g_reader_state = RESET;
    g_rfid_carrier_cycle_counter = 0;
  //  PRINTF("INFO: RESET reader state. \r\n");
  }

  return TPM_ClearStatusFlags(RFID_TRANSMIT_TPM, RFID_TRNSIMIT_INTERRUPT_FLAG);
}

void RFID_RECEIVER_DATA_HANDLER(void)
{
  unsigned int pin_mask = PORT_GetPinsInterruptFlags(RFID_RECEIVER_DATA_PORT);

  if(pin_mask & (1 << RFID_RECEIVER_DATA_GPIO_INDEX))
  {
    // something is detected, wait it stable. 
    if (IDLE == g_reader_state)
    {
      g_reader_state = WAIT_STABLE;
    }
    
    if(g_rfid_is_edge_detected && (g_reader_state > WAIT_STABLE))
    {
      // didn't handle last edge in time.
//      PRINTF("ERROR: Didn't handle last edge in time. cycle_counter:%d. reader state:%d.\r\n", g_rfid_carrier_cycle_counter, g_reader_state);
    }

    // handle rise edge interrupt of rfid receiver pin
    g_rfid_is_edge_detected = true;
    g_rise_edge_counter++;
  }

  // just clear what interrupts
  // we do not care other pin interrupt
  // and we also need clear RFID_RECEIVER_DATA_GPIO_INDEX
  return PORT_ClearPinsInterruptFlags(RFID_RECEIVER_DATA_PORT, pin_mask);
}
