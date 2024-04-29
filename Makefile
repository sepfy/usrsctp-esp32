# Toolchain configuration
CC := xtensa-esp32s3-elf-cc
AR := xtensa-esp32s3-elf-ar

TOOL_DIR := ../esp-idf-v5.2.1
# Directories
SRC_DIRS := usrsctplib usrsctplib/netinet
INC_DIRS := . usrsctplib  usrsctplib/netinet $(TOOL_DIR)/components/newlib/platform_include $(TOOL_DIR)/components/freertos/config/include $(TOOL_DIR)/components/freertos/config/include/freertos $(TOOL_DIR)/components/freertos/config/xtensa/include $(TOOL_DIR)/components/freertos/FreeRTOS-Kernel/include $(TOOL_DIR)/components/freertos/FreeRTOS-Kernel/portable/xtensa/include $(TOOL_DIR)/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos $(TOOL_DIR)/components/freertos/esp_additions/include $(TOOL_DIR)/components/esp_hw_support/include $(TOOL_DIR)/components/esp_hw_support/include/soc $(TOOL_DIR)/components/esp_hw_support/include/soc/esp32s3 $(TOOL_DIR)/components/esp_hw_support/port/esp32s3/. $(TOOL_DIR)/components/heap/include $(TOOL_DIR)/components/log/include $(TOOL_DIR)/components/soc/include $(TOOL_DIR)/components/soc/esp32s3 $(TOOL_DIR)/components/soc/esp32s3/include $(TOOL_DIR)/components/hal/platform_port/include $(TOOL_DIR)/components/hal/esp32s3/include $(TOOL_DIR)/components/hal/include $(TOOL_DIR)/components/esp_rom/include $(TOOL_DIR)/components/esp_rom/include/esp32s3 $(TOOL_DIR)/components/esp_rom/esp32s3 $(TOOL_DIR)/components/esp_common/include $(TOOL_DIR)/components/esp_system/include $(TOOL_DIR)/components/esp_system/port/soc $(TOOL_DIR)/components/esp_system/port/include/private $(TOOL_DIR)/components/xtensa/esp32s3/include $(TOOL_DIR)/components/xtensa/include $(TOOL_DIR)/components/xtensa/deprecated_include $(TOOL_DIR)/components/lwip/include $(TOOL_DIR)/components/lwip/include/apps $(TOOL_DIR)/components/lwip/include/apps/sntp $(TOOL_DIR)/components/lwip/lwip/src/include $(TOOL_DIR)/components/lwip/port/include $(TOOL_DIR)/components/lwip/port/freertos/include $(TOOL_DIR)/components/lwip/port/esp32xx/include $(TOOL_DIR)/components/lwip/port/esp32xx/include/arch $(TOOL_DIR)/components/lwip/port/esp32xx/include/sys $(TOOL_DIR)/components/esp_ringbuf/include $(TOOL_DIR)/components/efuse/include $(TOOL_DIR)/components/efuse/esp32s3/include $(TOOL_DIR)/components/esp_mm/include $(TOOL_DIR)/components/driver/include $(TOOL_DIR)/components/lwip/lwip/src/include/compat/posix 

#	$(TOOL_DIR)/tools/mocks/freertos/include 
#	$(TOOL_DIR)/components/newlib/platform_include \

BUILD_DIR := build

# Compiler flags
# use -H to debug header issues
CFLAGS := -DHAVE_SIN_LEN -DHAVE_SA_LEN -DHAVE_SCONN_LEN -DIPPORT_RESERVED=1024 -DUIO_MAXIOV=1024 -D__linux__ -D__Userspace__ -DSCTP_PROCESS_LEVEL_LOCKS -DSCTP_SIMPLE_ALLOCATOR -DESP_PLATFORM -DIDF_VER=\"v5.2.1\" -DMBEDTLS_CONFIG_FILE=\"mbedtls/esp_config.h\" -DSOC_MMU_PAGE_SIZE=CONFIG_MMU_PAGE_SIZE -DSOC_XTAL_FREQ_MHZ=CONFIG_XTAL_FREQ -DUNITY_INCLUDE_CONFIG_H -D_GLIBCXX_HAVE_POSIX_SEMAPHORE -D_GLIBCXX_USE_POSIX_SEMAPHORE -D_GNU_SOURCE -D_POSIX_READER_WRITER_LOCKS -DPACKAGE_TARNAME=\"libusrsctp\" -DPACKAGE_VERSION=\"0.9.5.0\" -DPACKAGE_STRING=\"libusrsctp\ 0.9.5.0\" -DSCTP_DEBUG=1 -mlongcalls  -ffunction-sections -fdata-sections -Wall -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-enum-conversion -gdwarf-4 -ggdb -Og -fno-shrink-wrap -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion -std=gnu17 -Wno-old-style-declaration 

CFLAGS += $(addprefix -I,$(INC_DIRS))

# Find all .c files in SRC_DIRS
SRCS := $(shell find $(SRC_DIRS) -name '*.c')

# Object files corresponding to SRCS
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

# Name of the output library
LIBRARY := libusrsctp.a

# Default target
all: $(LIBRARY)


# How to build the library
$(LIBRARY): $(OBJS)
	$(AR) rcs $@ $(OBJS)

# How to compile .c files into .o files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	$(RM) -r $(BUILD_DIR) $(LIBRARY)

.PHONY: all clean
