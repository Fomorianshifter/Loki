# Loki Build Configuration for Orange Pi Zero 2W
# 
# PLATFORM NOTES:
# - Linux/Mac: Use this Makefile directly with `make` command
# - Windows: Use build.bat or build.ps1 script instead
#
# Requires: arm-linux-gnueabihf-gcc cross-compiler (32-bit) or aarch64-linux-gnu-gcc (64-bit)
# Install on Ubuntu/Debian: sudo apt-get install gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu

## Build Target Architecture
ARCH ?= 32
NATIVE ?= 1
PLATFORM ?= LINUX
HOST_ARCH := $(shell uname -m 2>/dev/null)

ifeq ($(NATIVE), 0)
ifneq (,$(filter armv6l armv7l armv8l aarch64,$(HOST_ARCH)))
ifeq ($(NATIVE), 1)
CC := gcc
CFLAGS := -Wall -Wextra
ifeq ($(HOST_ARCH),aarch64)
CFLAGS += -mcpu=cortex-a53
$(info [INFO] Native build for 64-bit ARM Linux)
else ifneq (,$(filter armv6l armv7l armv8l,$(HOST_ARCH)))
CFLAGS += -mcpu=cortex-a53
$(info [INFO] Native build for 32-bit ARM Linux)
else
$(info [INFO] Native build for host machine)
endif
else ifeq ($(ARCH),64)
CC := aarch64-linux-gnu-gcc
CFLAGS := -Wall -Wextra -mcpu=cortex-a53
$(info [INFO] Cross-compilation for 64-bit ARM Linux)
else
CC := arm-linux-gnueabihf-gcc
CFLAGS := -Wall -Wextra -mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard
$(info [INFO] Cross-compilation for 32-bit ARM Linux)
endif
CFLAGS += -I. -DPLATFORM=PLATFORM_$(PLATFORM)
endif
endif
DEBUG ?= 1
ifeq ($(DEBUG),1)
CFLAGS += -g -O0 -DDEBUG -DLOG_LEVEL=4
BUILD_DIR := build/debug
$(info [INFO] Debug build enabled)
else
CFLAGS += -O3 -DDEBUG=0 -DLOG_LEVEL=2
BUILD_DIR := build/release
$(info [INFO] Release build enabled)
endif
		$(info [INFO] Native build for 64-bit ARM Linux)
	else
CROSS_USER ?= pi
CROSS_HOST ?= orange-pi.local
CROSS_PATH ?= /tmp
	CC := aarch64-linux-gnu-gcc
	CFLAGS := -Wall -Wextra -mcpu=cortex-a53
SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJECTS := $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o))
DEPS := $(OBJECTS:.o=.d)
TARGET := loki_app
endif
CFLAGS += -I. -DPLATFORM=PLATFORM_$(PLATFORM) 
LDFLAGS := -lm -lpthread
## Debug/Release Build Modes
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CFLAGS += -g -O0 -DDEBUG -DLOG_LEVEL=4
    BUILD_DIR := build/debug
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[OK] Successfully built $(TARGET) ($(BUILD_DIR))"
    BUILD_DIR := build/release
    $(info [INFO] Release build enabled)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
	@echo "[CC] $<"
CROSS_USER ?= pi
CROSS_HOST ?= orange-pi.local
-include $(DEPS)
 
## Project Structure
SOURCES := $(wildcard *.c)
	@echo "[INFO] Uploading to $(CROSS_USER)@$(CROSS_HOST):$(CROSS_PATH)..."
	scp $(BUILD_DIR)/$(TARGET) $(CROSS_USER)@$(CROSS_HOST):$(CROSS_PATH)/
	ssh $(CROSS_USER)@$(CROSS_HOST) 'chmod +x $(CROSS_PATH)/$(TARGET)'
	@echo "[OK] Installation complete"
 
## Linker Settings
LDFLAGS := -lm -lpthread
	@echo "[INFO] Executing on target..."
	ssh $(CROSS_USER)@$(CROSS_HOST) 'sudo $(CROSS_PATH)/$(TARGET)'
all: $(BUILD_DIR)/$(TARGET)
 
$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	$(MAKE) DEBUG=1 CFLAGS+=' -DMOCK_HARDWARE'
	./build/debug/$(TARGET)
	@echo "[✓] Successfully built $(TARGET) ($(BUILD_DIR))"
	@echo "[✓] Binary size: $$(ls -lh $@ | awk '{print $$5}')"
 
	@command -v doxygen >/dev/null 2>&1 || { echo "[!] Doxygen not installed. Install with: sudo apt-get install doxygen"; exit 1; }
	@echo "[INFO] Generating Doxygen documentation..."
	doxygen Doxyfile
	@echo "[OK] Documentation generated in docs/html/"
 
## Include dependencies
-include $(DEPS)
	rm -rf $(BUILD_DIR)
	@echo "[OK] Build artifacts removed"
install: $(BUILD_DIR)/$(TARGET)
	@echo "[→] Uploading to $(CROSS_USER)@$(CROSS_HOST):$(CROSS_PATH)..."
	scp $(BUILD_DIR)/$(TARGET) $(CROSS_USER)@$(CROSS_HOST):$(CROSS_PATH)/
	rm -rf docs/ build/
	find . -name "*.o" -delete
	find . -name "*.d" -delete
	@echo "[OK] Fully cleaned"
run: install
	@echo "[→] Executing on target..."
	ssh $(CROSS_USER)@$(CROSS_HOST) 'sudo $(CROSS_PATH)/$(TARGET)'
	@echo "[INFO] Running static analysis..."
	cppcheck --enable=all --suppress=missingIncludeSystem ./
	@echo "[OK] Analysis complete"
	$(MAKE) DEBUG=1 CFLAGS+=-DMOCK_HARDWARE
	./build/debug/$(TARGET)
 
	@echo "[INFO] Binary size breakdown:"
	$(CC:%gcc=%size) $(BUILD_DIR)/$(TARGET) 2>/dev/null || size $(BUILD_DIR)/$(TARGET)
	@echo "[→] Generating Doxygen documentation..."
	doxygen Doxyfile
	@echo "[✓] Documentation generated in docs/html/"
	@echo "Compiler: $(CC)"
	@echo "Build dir: $(BUILD_DIR)"
	@echo "Target path: $(CROSS_PATH)"
	find . -name "*.o" -delete
	find . -name "*.d" -delete
	@echo "[✓] Fully cleaned"
 
## Static analysis
analyze:
	@echo "[→] Running static analysis..."
	cppcheck --enable=all --suppress=missingIncludeSystem ./
	@echo "[✓] Analysis complete"
 
## Size report
size: $(BUILD_DIR)/$(TARGET)
	@echo "[→] Binary size breakdown:"
	arm-linux-gnueabihf-size $(BUILD_DIR)/$(TARGET)
	arm-linux-gnueabihf-nm -tS $(BUILD_DIR)/$(TARGET) | head -20
 
## Print configuration
info:
	@echo "╔════════════════════════════════════════╗"
	@echo "║    Loki Build Configuration           ║"
	@echo "╠════════════════════════════════════════╣"
	@echo "║ Compiler: $(CC)"
	@echo "║ Build mode: $$([ '$(DEBUG)' = '1' ] && echo 'DEBUG' || echo 'RELEASE')"
	@echo "║ Build dir: $(BUILD_DIR)"
	@echo "║ Cross target: $(CROSS_USER)@$(CROSS_HOST)"
	@echo "║ Target path: $(CROSS_PATH)"
	@echo "╚════════════════════════════════════════╝"
 
.PHONY: all clean clean-all install run test docs analyze size info