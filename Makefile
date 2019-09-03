TARGET=125kzh_rfid_reader_writer
JLINK_DEVICE_NAME=MKL02Z32xxx4
LOAD_SCRIPT=load_script.jlink
PROJECT_PATH = ./src
PROJECT_SRCS := $(shell find $(PROJECT_PATH) -name '*.c')

OUT_DIR=build

DEVICE_MKL02Z4_PATH=$(KL02_SDK_DIR)/devices/MKL02Z4/
MKL02Z4_DRIVERS_PATH=$(DEVICE_MKL02Z4_PATH)/drivers/
MKL02Z4_UTILITIES_PATH=$(shell find $(DEVICE_MKL02Z4_PATH)/utilities/ -type d -print)
STARTUP_SRC=$(DEVICE_MKL02Z4_PATH)/gcc/startup_MKL02Z4.S
MKL02Z4_SYSTEM_SRC=$(DEVICE_MKL02Z4_PATH)/system_MKL02Z4.c
MKL02Z4_LD_SCRIPT=$(DEVICE_MKL02Z4_PATH)/gcc/MKL02Z32xxx4_flash.ld
CMSIS_PATH=$(KL02_SDK_DIR)/CMSIS/

MKL02Z4_DRIVER_SRCS := $(shell find $(MKL02Z4_DRIVERS_PATH) -name '*.c')
MKL02Z4_UTILITIES_SRCS := $(shell find $(MKL02Z4_UTILITIES_PATH) -name '*.c')


C_SRCS += $(MKL02Z4_SYSTEM_SRC)
C_SRCS += $(MKL02Z4_DRIVER_SRCS)
C_SRCS += $(PROJECT_SRCS)
C_SRCS += $(MKL02Z4_UTILITIES_SRCS)
ASM_SRCS=$(STARTUP_SRC)

INC += $(PROJECT_PATH)
INC += $(MKL02Z4_DRIVERS_PATH)
INC += $(DEVICE_MKL02Z4_PATH)
INC += $(MKL02Z4_UTILITIES_PATH)
INC += $(CMSIS_PATH)/Include/
INC += $(PROJECT_PATH)/devices/mcu/
INC += $(PROJECT_PATH)/devices/lcd/
INC += $(PROJECT_PATH)/devices/rfid/

TARGET_ELF=$(OUT_DIR)/$(TARGET).elf
TARGET_HEX=$(OUT_DIR)/$(TARGET).hex
TARGET_BIN=$(OUT_DIR)/$(TARGET).bin

C_ASRCS=$(realpath $(C_SRCS))
C_OBJS=$(patsubst %.c,%.o,$(C_ASRCS))
ASM_ASRCS=$(realpath $(ASM_SRCS))
ASM_OBJS=$(patsubst %.S,%.o,$(ASM_ASRCS))

TARGET_OBJS=$(addprefix $(OUT_DIR),$(C_OBJS) $(ASM_OBJS))

GNU_PREFIX=arm-none-eabi
CC 			:= $(GNU_PREFIX)-gcc
CXX 		:= $(GNU_PREFIX)-c++
AS  		:= $(GNU_PREFIX)-as
AR  		:= $(GNU_PREFIX)-ar
LD  		:= $(GNU_PREFIX)-ld
OBJDUMP := $(GNU_PREFIX)-objdump
OBJCOPY := $(GNU_PREFIX)-objcopy
SIZE	  := $(GNU_PREFIX)-size

CFLAGS += -mcpu=cortex-m0 -mthumb -mabi=aapcs
CFLAGS += -mfloat-abi=soft
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin --short-enums
CFLAGS += -Wall -Werror -O3 -g3
CFLAGS += -std=c99
CFLAGS += -MP -MD
CFLAGS += -DCPU_MKL02Z32VFM4
CFLAGS += $(addprefix -I,$(INC))

ASMFLAGS += -std=c99
ASMFLAGS += -MP -MD

LDFLAGS += -mcpu=cortex-m0 -mthumb -mabi=aapcs
LDFLAGS += -L$(DEVICE_MKL02Z4_PATH)/gcc
LDFLAGS += -T$(MKL02Z4_LD_SCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += --specs=nano.specs -lc -lnosys

.PHONY: all clean load show dbg dbg_recv dbg_parity

all:$(TARGET_HEX) $(TARGET_BIN) 

clean:
	rm -f $(TARGET_HEX)
	rm -f $(TARGET_ELF)
	rm -f $(TARGET_OBJS)
	rm -r $(TARGET_BIN)
	rm -f $(LOAD_SCRIPT)
	rm -f $(patsubst %.o,%.d,$(TARGET_OBJS))
	rm -r $(OUT_DIR)

$(TARGET_ELF):$(TARGET_OBJS)
	@echo "LD $(TARGET_ELF) ..."
	@$(CC) $(LDFLAGS) -o $@ $^ -lm
	@$(SIZE) $@

show:
	@echo "Target: $(TARGET)"
	@echo "JLink Device: $(JLINK_DEVICE_NAME)"
	@echo "JLink Path: $(JLINK_PATH)"

load:$(TARGET_BIN)
	@echo "Loading $(TARGET_BIN) to on-chip Flash ..."
	@echo "device $(JLINK_DEVICE_NAME)" > $(LOAD_SCRIPT)
	@echo "if SWD" >> $(LOAD_SCRIPT)
	@echo "speed 4000" >> $(LOAD_SCRIPT)
	@echo "r" >> $(LOAD_SCRIPT)
	@echo "h" >> $(LOAD_SCRIPT)
	@echo "loadbin $(TARGET_BIN),0x0" >> $(LOAD_SCRIPT)
	@echo "exit" >> $(LOAD_SCRIPT)
	@$(JLINK_PATH)/JLinkExe -commanderscript $(LOAD_SCRIPT)
	@rm $(LOAD_SCRIPT)

dbg: CFLAGS += -DRFID_DBG

dbg: all

dbg_recv: CFLAGS+=-DRFID_DBG_RECV

dbg_recv: all

dbg_parity: CFLAGS+=-DRFID_DBG_PARITiY

dbg_parity: all

dbg_parse: CFLAGS+=-DRFID_DBG_PARSE_DATA

dbg_parse: all

dbg_synchead: CFLAGS+=-DRFID_DBG_SYNC_HEAD

dbg_synchead: all

dbg_pwm_output: CFLAGS+=-DDBG_PWM_OUTPUT

dbg_pwm_output: all

$(OUT_DIR)/%.o:/%.c
	@echo "CC $@ ..."
	@mkdir -p $(dir $@)
	@echo $(CFLAGS)
	@$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_DIR)/%.o:/%.S
	@echo "AS $@ ..."
	@mkdir -p $(dir $@)
	@$(CC) $(ASMFLAGS) -c -o $@ $<

%.hex:%.elf
	@echo "CP $@ ..."
	@$(OBJCOPY) -O ihex $< $@

%.bin:%.elf
	@echo "CP $@ ..."
	@$(OBJCOPY) -O binary $< $@
