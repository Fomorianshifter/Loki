# Build & Development Guide for Loki

## Quick Start

### Build Options

```bash
# Debug build (with symbols, optimization level 0, full logging)
make DEBUG=1

# Release build (optimized, minimal logging)
make DEBUG=0

# Default is Debug mode
make
```

### Cross-Compilation for Orange Pi

```bash
# Set your cross-compiler target
export CROSS_USER=pi                    # SSH username
export CROSS_HOST=orange-pi.local       # Hostname or IP
export CROSS_PATH=/tmp                  # Target directory

# Build and install to target
make install

# Or install and run directly
make run
```

### Local Testing (Mock Hardware)

Compile without hardware dependencies for testing on your development machine:

```bash
make test   # Compiles with MOCK_HARDWARE flag and runs locally
```

---

## Makefile Targets

| Target | Purpose |
|--------|---------|
| `make` / `make all` | Compile project (Debug mode default) |
| `make install` | Upload binary to Orange Pi via SCP |
| `make run` | Install and execute on target |
| `make test` | Local test build with mock hardware |
| `make docs` | Generate Doxygen documentation |
| `make analyze` | Run static analysis (cppcheck) |
| `make size` | Show binary size breakdown |
| `make clean` | Remove build artifacts |
| `make clean-all` | Remove all generated files |
| `make info` | Display build configuration |

---

## Logging System

The Loki system features a comprehensive logging framework accessible via easy-to-use macros:

### Log Levels

| Level | Macro | Purpose |
|-------|-------|---------|
| CRITICAL | `LOG_CRITICAL()` | Fatal errors, system failure |
| ERROR | `LOG_ERROR()` | Error conditions, operation failed |
| WARN | `LOG_WARN()` | Warnings, unexpected but recoverable |
| INFO | `LOG_INFO()` | Normal informational messages |
| DEBUG | `LOG_DEBUG()` | Debug information (DEBUG builds only) |

### Usage Examples

```c
#include "utils/log.h"

// Simple messages
LOG_INFO("System initialized");
LOG_ERROR("Failed to initialize SPI");
LOG_DEBUG("Register value: 0x%02X", reg_val);

// With format strings
LOG_WARN("Device timeout after %d attempts", retry_count);
LOG_CRITICAL("Out of memory: %zu bytes", size);

// Automatic source location tracking
// All logs include file name, line number, and function name
```

### Configure Log Level at Runtime

```c
log_set_level(LOG_DEBUG);    // Verbose logging
log_set_level(LOG_INFO);     // Normal logging
log_set_level(LOG_WARN);     // Warnings and errors only
log_set_level(LOG_ERROR);    // Only errors and critical
```

### Output Example

```
[15:30:42] [INFO] [system.c:52] system_init(): System initialization complete
[15:30:43] [DBG ] [gpio.c:38] gpio_configure(): Configuring GPIO pin 18 (mode=1, pull=0)
[15:30:43] [ERR ] [spi.c:102] spi_init(): Failed to open SPI device
```

---

## Memory Management

Safe memory allocation with automatic tracking (debug mode):

```c
#include "utils/memory.h"

// Safe allocation
uint8_t *buffer = malloc_safe(256);
if (buffer == NULL) {
    LOG_ERROR("Out of memory");
    return HAL_ERROR;
}

// Safe free
free_safe(&buffer);  // buffer is now NULL

// Get memory statistics (DEBUG only)
size_t used = memory_get_usage();
memory_report();     // Print detailed report
```

### Memory Tracking (DEBUG Mode)

In debug builds, all allocations are tracked and logged:

```
[15:30:42] [DBG ] [memory.c:60] track_allocation(): Allocated 256 bytes at 0x12345678 (total: 256 bytes)
[15:30:43] [DBG ] [memory.c:75] untrack_allocation(): Freed 256 bytes from 0x12345678 (total: 0 bytes)
```

When the system shuts down, a final report shows any memory leaks:

```
[15:31:00] [INFO] [memory.c:150] memory_report(): === Memory Allocation Report ===
[15:31:00] [INFO] [memory.c:151] memory_report(): Total allocated: 512 bytes
[15:31:00] [INFO] [memory.c:152] memory_report(): Active allocations: 2
```

---

## Retry Logic for Bus Operations

Automatic retry mechanism for transient failures (SPI, I2C, UART):

```c
#include "utils/retry.h"

// Simple retry with sensible defaults
hal_status_t status = RETRY(
    spi_write(SPI_BUS_0, CS_PIN, data, len),
    RETRY_BALANCED
);

// Retry strategies available:
// - RETRY_AGGRESSIVE: 10 attempts, 1ms initial delay, 2x backoff
// - RETRY_BALANCED: 5 attempts, 10ms initial delay, 2x backoff (default)
// - RETRY_CONSERVATIVE: 3 attempts, 50ms initial delay, 2x backoff
// - RETRY_NONE: Single attempt, no retry

// Only retryable errors are retried:
// - HAL_TIMEOUT
// - HAL_BUSY
// - HAL_NOT_READY
//
// Non-retryable errors fail immediately:
// - HAL_INVALID_PARAM
// - HAL_NOT_SUPPORTED
```

### Retry Example

```
[15:30:44] [WARN] [retry.c:78] retry_execute(): [spi_write() at drivers/tft/tft_driver.c:120] 
                   Retrying (1/5)... waiting 10 ms
[15:30:45] [WARN] [retry.c:78] retry_execute(): [spi_write() at drivers/tft/tft_driver.c:120] 
                   Retrying (2/5)... waiting 20 ms
[15:30:45] [INFO] [spi.c:95] spi_write(): Device ready, operation succeeded
```

---

## Documentation Generation

Generate API documentation with Doxygen:

```bash
make docs
```

This creates HTML documentation in `docs/html/index.html`. Open in a web browser:

```bash
open docs/html/index.html          # macOS
xdg-open docs/html/index.html      # Linux
start docs/html/index.html         # Windows
```

### Doxygen Features

- ✓ Function/API documentation
- ✓ Parameter and return value descriptions
- ✓ Usage examples
- ✓ Collaboration diagrams
- ✓ Call graphs
- ✓ TODO/BUG/DEPRECATED tracking

### Writing Doxygen Comments

```c
/**
 * @brief Brief description of function
 * 
 * Longer detailed description goes here.
 * Can span multiple lines.
 * 
 * @param[in] param1 Description of input parameter
 * @param[out] param2 Description of output parameter
 * @return Description of return value
 * 
 * @example
 * @code
 * result = my_function(input, &output);
 * @endcode
 * 
 * @note Important implementation note
 * @warning Potential issue users should know about
 */
hal_status_t my_function(uint32_t param1, uint32_t *param2);
```

---

## Static Analysis

Run static code analysis to find potential bugs:

```bash
make analyze
```

Requires `cppcheck` to be installed:

```bash
sudo apt-get install cppcheck      # Linux
brew install cppcheck              # macOS
```

---

## Binary Size Analysis

Inspect the final binary size:

```bash
make size
```

Output:

```
[→] Binary size breakdown:
   text  	data     bss     dec     hex filename
  45678    3456   12834   61968   f1f0 loki_app

[→] Top 20 symbols:
 0x0000abcd 0x2000 T tft_write_pixels
 0x0000cdef 0x1500 T spi_write
 0x0000efgh 0x0800 T log_message
 ...
```

---

## Debug Workflow

### Enable Verbose Output

```bash
# Set log level at runtime
export LOG_LEVEL=4  # DEBUG
make DEBUG=1 && make run
```

### Debugging on Target

```bash
# SSH into Orange Pi and run with GDB
ssh pi@orange-pi
sudo gdb /tmp/loki_app

(gdb) run
(gdb) bt           # Backtrace on crash
(gdb) print var    # Print variable
(gdb) quit
```

### Memory Debugging

Enable memory tracking to find leaks:

```c
// In your code:
memory_init();
// ... do work ...
memory_report();   // Shows unfreed allocations
memory_deinit();
```

---

## Performance Profiling

### Timing Measurements

```c
#include <time.h>

clock_t start = clock();
// ... operation ...
clock_t end = clock();
double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
LOG_INFO("Operation took %.3f seconds", elapsed);
```

### SPI Bus Speed Testing

```c
// Current configuration:
// - SPI0 (TFT): 40 MHz
// - SPI1 (SD): 25 MHz
// - SPI2 (Flash): 20 MHz

// To change speed, modify board_config.h:
#define TFT_SPI_FREQ    40000000  // Increase for faster TFT
#define SD_SPI_FREQ     25000000
#define FLASH_SPI_FREQ  20000000
```

---

## Troubleshooting

### Build Fails: "arm-linux-gnueabihf-gcc: not found"

Your ARM cross-compiler is not installed or not in PATH:

```bash
# Install on Ubuntu
sudo apt-get install gcc-arm-linux-gnueabihf

# Add to PATH if not automatic
export PATH="/usr/arm-linux-gnueabihf/bin:$PATH"

# Verify installation
arm-linux-gnueabihf-gcc --version
```

### Installation Fails: Cannot SSH to Orange Pi

Ensure SSH is configured and working:

```bash
# Test SSH connection
ssh pi@orange-pi hostname

# If fails, check:
# 1. Orange Pi is on network
# 2. SSH enabled on Orange Pi
# 3. Correct hostname/IP in Makefile
# 4. SSH key installed (optional but recommended)
```

### Logging Not Appearing

Check log level is appropriate:

```c
// In DEBUG build, logs should appear
LOG_DEBUG("This message should appear");

// In RELEASE build, DEBUG logs are suppressed
// Only INFO, WARN, ERROR, CRITICAL appear by default
```

---

## Best Practices

1. **Always use LOG_* macros** instead of printf
2. **Always use malloc_safe/free_safe** for dynamic memory
3. **Always use RETRY for bus operations** that might fail transiently
4. **Close resources in reverse order** of allocation
5. **Check status codes** before using returned values
6. **Use meaningful log messages** with context

---

## Environment Variables

```bash
DEBUG=1                    # Enable debug build
CROSS_USER=pi             # SSH user for cross-compilation
CROSS_HOST=192.168.1.100  # Orange Pi hostname or IP
CROSS_PATH=/tmp           # Target installation path
CC=arm-linux-gnueabihf-gcc # Override compiler
CFLAGS="-O3 -march=armv8"  # Override compiler flags
```

---

For more information, see [README.md](README.md) and generated Doxygen documentation.
