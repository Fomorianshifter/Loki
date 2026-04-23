# Loki Build Configuration
#
# PLATFORM NOTES:
# - Linux/Mac: Use this Makefile directly with `make` command
# - Windows: Use build.bat or build.ps1 script instead
#
# Supported platforms (set PLATFORM=<value>):
#   armv7   - Raspberry Pi 2, older Orange Pi (ARMv7 Cortex-A7, 32-bit)
#   armv8   - Raspberry Pi 3/4/5, Orange Pi Zero 2W (ARMv8 Cortex-A53+, 64-bit) [default]
#   native  - Host build with MOCK_HARDWARE for compile/unit tests without target hardware
#
# Example usage:
#   make                          # armv8 release build
#   make PLATFORM=armv7 DEBUG=0   # RPi 2 release build
#   make PLATFORM=native DEBUG=1  # local compile check

## Platform / Target Selection
PLATFORM ?= armv8

ifeq ($(PLATFORM), armv7)
    CROSS_PREFIX := arm-linux-gnueabihf-
    CFLAGS_ARCH  := -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
    $(info [INFO] Platform: armv7  (RPi 2 / Orange Pi Zero  – arm-linux-gnueabihf))
else ifeq ($(PLATFORM), armv8)
    CROSS_PREFIX := aarch64-linux-gnu-
    CFLAGS_ARCH  := -march=armv8-a -mtune=cortex-a53
    $(info [INFO] Platform: armv8  (RPi 3/4/5 / Orange Pi Zero 2W  – aarch64-linux-gnu))
else ifeq ($(PLATFORM), native)
    CROSS_PREFIX :=
    CFLAGS_ARCH  :=
    $(info [INFO] Platform: native (host build with MOCK_HARDWARE))
else
    $(error Unknown PLATFORM '$(PLATFORM)'. Supported: armv7, armv8, native)
endif

CC := $(CROSS_PREFIX)gcc

## Compiler Settings
CFLAGS := -Wall -Wextra $(CFLAGS_ARCH)
CFLAGS += -I.
ifeq ($(PLATFORM), native)
    CFLAGS += -DMOCK_HARDWARE
endif

## Debug/Release Build Modes
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CFLAGS += -g -O0 -DDEBUG -DLOG_LEVEL=4
    BUILD_MODE := debug
    $(info [INFO] Debug build enabled)
else
    CFLAGS += -O3 -DDEBUG=0 -DLOG_LEVEL=2
    BUILD_MODE := release
    $(info [INFO] Release build enabled)
endif

BUILD_DIR := build/$(PLATFORM)/$(BUILD_MODE)

## Cross-compiler target (customize for your setup)
CROSS_USER ?= pi
CROSS_HOST ?= orange-pi.local
CROSS_PATH ?= /tmp

## Project Structure – find all .c files recursively
SOURCES := main.c $(shell find core hal drivers utils -name "*.c" 2>/dev/null)
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)
TARGET  := loki_app

## Linker Settings
LDFLAGS := -lm -lpthread

## Build Rules
all: $(BUILD_DIR)/$(TARGET)

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

## Run on target
run: install
	@echo "[→] Executing on target..."
	ssh $(CROSS_USER)@$(CROSS_HOST) 'sudo $(CROSS_PATH)/$(TARGET)'

## Local testing (without hardware) – builds natively with MOCK_HARDWARE
test:
	$(MAKE) PLATFORM=native DEBUG=1
	./build/native/debug/$(TARGET)

## Documentation generation (requires Doxygen)
docs:
	@command -v doxygen >/dev/null 2>&1 || { echo "[!] Doxygen not installed. Install with: sudo apt-get install doxygen"; exit 1; }
	@echo "[→] Generating Doxygen documentation..."
	doxygen Doxyfile
	@echo "[✓] Documentation generated in docs/html/"

## Clean build artifacts for the current PLATFORM/DEBUG combination
clean:
	rm -rf $(BUILD_DIR)
	@echo "[✓] Build artifacts removed ($(BUILD_DIR))"

## Clean all platforms and generated files
clean-all:
	rm -rf build/ docs/
	@echo "[✓] Fully cleaned"

## Static analysis
analyze:
	@echo "[→] Running static analysis..."
	cppcheck --enable=all --suppress=missingIncludeSystem ./
	@echo "[✓] Analysis complete"

## Size report
size: $(BUILD_DIR)/$(TARGET)
	@echo "[→] Binary size breakdown:"
	$(CROSS_PREFIX)size $(BUILD_DIR)/$(TARGET)
	$(CROSS_PREFIX)nm -tS $(BUILD_DIR)/$(TARGET) | head -20

## Print configuration
info:
	@echo "╔════════════════════════════════════════╗"
	@echo "║    Loki Build Configuration           ║"
	@echo "╠════════════════════════════════════════╣"
	@echo "║ Platform:   $(PLATFORM)"
	@echo "║ Compiler:   $(CC)"
	@echo "║ Build mode: $$([ '$(DEBUG)' = '1' ] && echo 'DEBUG' || echo 'RELEASE')"
	@echo "║ Build dir:  $(BUILD_DIR)"
	@echo "║ Cross target: $(CROSS_USER)@$(CROSS_HOST)"
	@echo "║ Target path:  $(CROSS_PATH)"
	@echo "╚════════════════════════════════════════╝"

.PHONY: all clean clean-all install run test docs analyze size info