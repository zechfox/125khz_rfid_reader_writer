/*
 * mcu MKL02Z32VFM4 header file
 *
 *
 */
#include "fsl_i2c.h"
#include "configuration.h"

extern volatile bool completionFlag;
extern volatile bool nakFlag;


bool initial_mcu(void);
bool initial_gpio(void);
bool initial_uart(void);
bool initial_master_spi(void);
bool initial_i2c(void);
bool initial_adc(void);
bool initial_run_clk(void);
bool initial_vlpr_clk(void);
bool initial_tpm(void);
void i2c_release_bus(void);
i2c_master_handle_t* get_i2c_handler();
