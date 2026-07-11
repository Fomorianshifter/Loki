# Loki Build Configuration
#
# PLATFORM NOTES:
# - Target: Raspberry Pi (ARM) with RS-485 HAT
# - Linux/Mac: Use this Makefile directly with `make` command
# - Windows: Use build.bat or build.ps1 script instead
#
# Cross-compiler install (Ubuntu/Debian):
#   sudo apt-get install gcc-arm-linux-gnueabihf
# Native build (when running directly on the Pi):
#   make  (will use gcc if cross-compiler is absent)

.DEFAULT_GOAL := all

## Config generation (single source of truth: config.toml)
PYTHON      ?= python3
CONFIG_GEN  := tools/gen_config.py
CONFIG_TOML := config.toml
CONFIG_HDRS := board_config.h pinout.h config.h

## Compiler Settings
## On ARM hosts (e.g. Raspberry Pi), build natively with safe defaults.
## On non-ARM hosts, prefer the ARM cross-compiler when available.
HOST_ARCH := $(shell uname -m)
USING_CROSS := 0
ifeq ($(filter arm% aarch64%,$(HOST_ARCH)),)
    ifneq ($(shell which arm-linux-gnueabihf-gcc 2>/dev/null),)
        CC          := arm-linux-gnueabihf-gcc
        BASE_CFLAGS := -Wall -Wextra -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
        USING_CROSS := 1
    else
        CC          := gcc
        BASE_CFLAGS := -Wall -Wextra
        $(info [WARN] arm-linux-gnueabihf-gcc not found, using native gcc)
    endif
else
    CC          := gcc
    BASE_CFLAGS := -Wall -Wextra
    $(info [INFO] Native ARM host detected ($(HOST_ARCH)); using gcc defaults)
endif

CFLAGS := $(BASE_CFLAGS) -I.
 
## Debug/Release Build Modes
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CFLAGS += -g -O0 -DDEBUG -DLOG_LEVEL=4
    BUILD_DIR := build/debug
    $(info [INFO] Debug build enabled)
else
    CFLAGS += -O3 -DDEBUG=0 -DLOG_LEVEL=2
    BUILD_DIR := build/release
    $(info [INFO] Release build enabled)
endif
 
## Cross-compiler target (customize for your setup)
CROSS_USER ?= pi
CROSS_HOST ?= raspberrypi.local
CROSS_PATH ?= /tmp
SUDO ?= sudo
LOCAL_INSTALL_PATH ?= /usr/local/bin
  
## Project Structure
SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJECTS := $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o))
DEPS := $(OBJECTS:.o=.d)
TARGET := loki_app
 
## Linker Settings
LDFLAGS := -lm -lpthread

ifeq ($(USING_CROSS),1)
    SIZE_TOOL := arm-linux-gnueabihf-size
    NM_TOOL   := arm-linux-gnueabihf-nm
else
    SIZE_TOOL := size
    NM_TOOL   := nm
endif

## Optional: libgpiod for GPIO control (TFT pins + RS-485 DE)
## Install:  sudo apt-get install libgpiod-dev
## Enable:   make HAVE_LIBGPIOD=1
HAVE_LIBGPIOD ?= 0
ifeq ($(HAVE_LIBGPIOD), 1)
    CFLAGS  += -DHAVE_LIBGPIOD
    LDFLAGS += -lgpiod
    $(info [INFO] libgpiod enabled for RS-485 GPIO DE control)
endif
 
## Generated config headers — regenerated whenever config.toml or the script changes.
$(CONFIG_HDRS): $(CONFIG_TOML) $(CONFIG_GEN)
	$(PYTHON) $(CONFIG_GEN)

## Regenerate config headers only.
config: $(CONFIG_HDRS)

## Build Rules
all: $(CONFIG_HDRS) $(BUILD_DIR)/$(TARGET)
 
$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[✓] Successfully built $(TARGET) ($(BUILD_DIR))"
	@echo "[✓] Binary size: $$(ls -lh $@ | awk '{print $$5}')"
 
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
	@echo "[CC] $<"
 
## Include dependencies
-include $(DEPS)
 
## Installation target
install: $(BUILD_DIR)/$(TARGET)
	@echo "[→] Uploading to $(CROSS_USER)@$(CROSS_HOST):$(CROSS_PATH)..."
	scp $(BUILD_DIR)/$(TARGET) $(CROSS_USER)@$(CROSS_HOST):$(CROSS_PATH)/
	ssh $(CROSS_USER)@$(CROSS_HOST) 'chmod +x $(CROSS_PATH)/$(TARGET)'
	@echo "[✓] Installation complete"

## Local installation target (for running directly on this machine)
install-local: $(BUILD_DIR)/$(TARGET)
	@echo "[→] Installing locally to $(LOCAL_INSTALL_PATH)/$(TARGET)..."
	$(SUDO) install -m 755 $(BUILD_DIR)/$(TARGET) $(LOCAL_INSTALL_PATH)/$(TARGET)
	@echo "[✓] Local installation complete"
 
## Run on target
run: install
	@echo "[→] Executing on target..."
	ssh $(CROSS_USER)@$(CROSS_HOST) 'sudo $(CROSS_PATH)/$(TARGET)'
 
## Local testing (without hardware)
test: clean
	$(MAKE) DEBUG=1 CFLAGS+=-DMOCK_HARDWARE
	./build/debug/$(TARGET)
 
## Documentation generation (requires Doxygen)
docs:
	@command -v doxygen >/dev/null 2>&1 || { echo "[!] Doxygen not installed. Install with: sudo apt-get install doxygen"; exit 1; }
	@echo "[→] Generating Doxygen documentation..."
	doxygen Doxyfile
	@echo "[✓] Documentation generated in docs/html/"
 
## Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "[✓] Build artifacts removed"
 
## Clean everything including docs
clean-all: clean
	rm -rf docs/ build/
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
	$(SIZE_TOOL) $(BUILD_DIR)/$(TARGET)
	$(NM_TOOL) -tS $(BUILD_DIR)/$(TARGET) | head -20
 
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
 
.PHONY: all config clean clean-all install install-local run test docs analyze size info