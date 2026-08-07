.DEFAULT_GOAL := all

DEBUG ?= 1
BUILD_DIR := $(if $(filter 1,$(DEBUG)),build/debug,build/release)
TARGET := $(BUILD_DIR)/loki_app
SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

ifeq ($(origin CC), default)
ifneq ($(shell command -v arm-linux-gnueabihf-gcc 2>/dev/null),)
CC := arm-linux-gnueabihf-gcc
else
CC := gcc
endif
endif

CFLAGS := -Wall -Wextra -I.
LDFLAGS := -lpthread -lm

ifeq ($(DEBUG),1)
CFLAGS += -g -O0 -DDEBUG=1 -DLOG_LEVEL=4
else
CFLAGS += -O3 -DDEBUG=0 -DLOG_LEVEL=2 -DNDEBUG
endif

ifeq ($(HAVE_LIBGPIOD),1)
CFLAGS += -DHAVE_LIBGPIOD
LDFLAGS += -lgpiod
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

.PHONY: all clean
