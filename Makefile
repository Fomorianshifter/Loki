# Loki Build Configuration for Orange Pi Zero 2W
# 
# PLATFORM NOTES:
# - Linux/Mac: Use this Makefile directly with `make` command
# - Windows: Use build.bat or build.ps1 script instead
#
# Requires: arm-linux-gnueabihf-gcc cross-compiler
# Install on Ubuntu/Debian: sudo apt-get install gcc-arm-linux-gnueabihf

## Compiler Settings
CC := arm-linux-gnueabihf-gcc
CFLAGS := -Wall -Wextra -march=armv7-a -mtune=cortex-a7
CFLAGS += -I. -I./includes -I./config -I./utils

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
CROSS_HOST ?= orange-pi.local
CROSS_PATH ?= /tmp

## Project Structure
SOURCES := $(wildcard hal/**/*.c drivers/**/*.c core/*.c utils/*.c)
HEADERS := $(wildcard includes/*.h config/*.h hal/**/*.h drivers/**/*.h core/*.h utils/*.h)
OBJECTS := $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o))
DEPS := $(OBJECTS:.o=.d)
TARGET := loki_app

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
	cppcheck --enable=all --suppress=missingIncludeSystem hal/ drivers/ core/ utils/
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
