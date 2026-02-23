# Contributing to Loki

## Code Style Guidelines

### Naming Conventions

**Files**
- Header files: `lowercase_with_underscores.h`
- Implementation files: `lowercase_with_underscores.c`
- Driver directories: `device_name/` (e.g., `drivers/tft/`)

**Functions**
- Public HAL functions: `subsystem_verb_noun()` (e.g., `gpio_set()`, `spi_write()`)
- Private functions: prefix with `_` (e.g., `_gpio_export()`)
- Return type before function name: `hal_status_t function_name()`

**Variables**
- Local variables: `lowercase_with_underscores`
- Static variables: `s_lowercase_with_underscores`
- Global variables: avoid when possible, prefix with `g_` if necessary
- Structure members: `lowercase_with_underscores`

**Constants & Macros**
- Preprocessor constants: `UPPERCASE_WITH_UNDERSCORES`
- Enums: `enum_name_t` (e.g., `gpio_level_t`)
- Typedef structs: `struct_name_t`

### Code Formatting

```c
/**
 * @file example.h
 * @brief Brief description
 * 
 * Longer description of what this file does.
 * 
 * @defgroup GroupName Group short name
 * @{
 */

#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <stdint.h>
#include "types.h"

/* ===== CONSTANTS ===== */
#define BUFFER_SIZE     256
#define TIMEOUT_MS      1000

/* ===== TYPES ===== */
typedef enum {
    STATE_IDLE = 0,
    STATE_ACTIVE = 1,
} state_t;

typedef struct {
    uint32_t field1;
    uint8_t field2;
} config_t;

/* ===== PUBLIC API ===== */

/**
 * @brief Function description
 * 
 * @param[in] param1 Description
 * @param[out] param2 Description
 * @return @ref HAL_OK on success
 */
hal_status_t example_function(uint32_t param1, uint32_t *param2);

/** @} */ // end of GroupName

#endif /* EXAMPLE_H */
```

### Line Length

- Maximum 100 characters per line
- Indent with 4 spaces (not tabs)
- One statement per line

### Comments

```c
/* Single-line comment for simple code */

/**
 * Multi-line comment for complex logic
 * explaining why we're doing this, not just
 * what the code does.
 */

/* Invalid: doesn't explain the "why"
uint32_t delay = 10;
*/

/* Valid: explains reasoning */
/* Delay to allow capacitor discharge */
uint32_t delay = 10;
```

### Error Handling

```c
/**
 * GOOD: Check status and return immediately
 */
hal_status_t init_device(void)
{
    hal_status_t status = subsystem_init();
    if (status != HAL_OK) {
        LOG_ERROR("Subsystem init failed: %d", status);
        return status;
    }
    
    status = configure_device();
    if (status != HAL_OK) {
        LOG_ERROR("Device config failed: %d", status);
        return status;
    }
    
    return HAL_OK;
}

/**
 * AVOID: Nested error handling
 */
hal_status_t bad_init(void)
{
    if (subsystem_init() == HAL_OK) {
        if (configure_device() == HAL_OK) {
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}
```

### Memory Management

```c
/* GOOD: Use malloc_safe */
uint8_t *buffer = malloc_safe(256);
if (buffer == NULL) {
    LOG_ERROR("Failed to allocate buffer");
    return HAL_ERROR;
}

/* GOOD: Free when done */
free_safe(&buffer);

/* AVOID: Direct malloc/free without checking */
void *ptr = malloc(256);  // No error check
free(ptr);
ptr = NULL;  // Manual null assignment
```

---

## Development Workflow

### 1. Make a Feature Branch

```bash
git checkout -b feature/my-feature
git checkout -b bugfix/issue-number
git checkout -b refactor/component-name
```

### 2. Write Code with Proper Documentation

- Add Doxygen comments to all public functions
- Add LOG_* calls for debugging
- Include error checking and logging
- Use RETRY for potentially failing operations

### 3. Test Your Changes

```bash
make clean
make DEBUG=1      # Debug build with full symbols
make analyze      # Static analysis
make docs         # Check documentation generates

# If cross-compiling:
make install      # Deploy to target and test
make run          # Run with output
```

### 4. Before Submitting

```bash
# Verify builds both ways
make clean
make DEBUG=0      # Release build
make clean
make DEBUG=1      # Debug build

# Check for obvious issues
make analyze

# Generate and review documentation
make docs         # Open docs/html/index.html
```

---

## Adding a New Driver

### File Structure

```
drivers/
  new_device/
    new_device_driver.h
    new_device_driver.c
```

### Header Template

```c
#ifndef NEW_DEVICE_DRIVER_H
#define NEW_DEVICE_DRIVER_H

/**
 * @file new_device_driver.h
 * @brief Driver for new device
 * 
 * Brief description of what this device does
 * and how to use the driver.
 * 
 * @defgroup NewDevice New Device Driver
 * @{
 */

#include "../../includes/types.h"

/* ===== PUBLIC API ===== */

/**
 * @brief Initialize device
 * @return HAL_OK on success
 */
hal_status_t new_device_init(void);

/**
 * @brief Read data from device
 * @param[out] buffer Output buffer
 * @param[in] length Bytes to read
 * @return HAL_OK on success
 */
hal_status_t new_device_read(uint8_t *buffer, uint32_t length);

/**
 * @brief Deinitialize device
 * @return HAL_OK on success
 */
hal_status_t new_device_deinit(void);

/** @} */

#endif /* NEW_DEVICE_DRIVER_H */
```

### Implementation Template

```c
/**
 * @file new_device_driver.c
 * @brief New device driver implementation
 */

#include "new_device_driver.h"
#include "../../utils/log.h"
#include "../../hal/interface/interface.h"

/* ===== PRIVATE STATE ===== */

typedef struct {
    uint8_t initialized;
    /* Add state variables */
} device_context_t;

static device_context_t device_ctx = {0};

/* ===== PRIVATE FUNCTIONS ===== */

static hal_status_t _device_validate(void)
{
    /* Device-specific validation */
    return HAL_OK;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t new_device_init(void)
{
    if (device_ctx.initialized) {
        return HAL_OK;
    }
    
    LOG_INFO("Initializing new device");
    
    hal_status_t status = _device_validate();
    if (status != HAL_OK) {
        LOG_ERROR("Device validation failed");
        return status;
    }
    
    device_ctx.initialized = 1;
    return HAL_OK;
}

hal_status_t new_device_read(uint8_t *buffer, uint32_t length)
{
    if (buffer == NULL || length == 0) {
        LOG_ERROR("Invalid parameters");
        return HAL_INVALID_PARAM;
    }
    
    if (!device_ctx.initialized) {
        LOG_ERROR("Device not initialized");
        return HAL_NOT_READY;
    }
    
    LOG_DEBUG("Reading %u bytes from device", length);
    
    /* Read operation */
    return HAL_OK;
}

hal_status_t new_device_deinit(void)
{
    if (!device_ctx.initialized) {
        return HAL_OK;
    }
    
    LOG_INFO("Deinitializing new device");
    device_ctx.initialized = 0;
    return HAL_OK;
}
```

---

## Documentation Standards

### Function Documentation

Every public function must have:
- Brief description (one sentence)
- Detailed description (if needed)
- @param descriptions for all parameters
- @return description
- @example if helpful

### File Documentation

Every file must start with:
- @file directive
- @brief description
- Optional @defgroup for organizational grouping

### Doxygen Special Tags

- `@param[in]` - Input parameter
- `@param[out]` - Output parameter
- `@param[in,out]` - Both input and output
- `@return` - Return value description
- `@retval VALUE` - Specific return value
- `@warning` - Important warning
- `@note` - Important note
- `@example` - Usage example
- `@deprecated` - Function is obsolete
- `@todo` - Items that need work
- `@bug` - Known bugs

---

## Testing Checklist

Before committing code:

- [ ] Code compiles without warnings (`make DEBUG=1`)
- [ ] Code compiles in release mode (`make DEBUG=0`)
- [ ] No obvious defects (`make analyze`)
- [ ] Documentation builds (`make docs`)
- [ ] Function behavior tested
- [ ] Error cases handled
- [ ] Log messages are clear and helpful
- [ ] No memory leaks (check `memory_report()`)
- [ ] Cross-platform: works on target hardware

---

## Common Pit falls

### ❌ Don't do this

```c
/* Missing error checking */
gpio_set(pin, GPIO_LEVEL_HIGH);

/* Unclear log message */
LOG_INFO("Error");

/* Magic numbers */
for (int i = 0; i < 256; i++) { ... }

/* Ignoring return codes */
malloc(size);

/* No comments explaining WHY */
usleep(50000);
```

### ✅ Do this instead

```c
/* Check return status */
if (gpio_set(pin, GPIO_LEVEL_HIGH) != HAL_OK) {
    LOG_ERROR("Failed to set GPIO pin %u", pin);
    return HAL_ERROR;
}

/* Meaningful log messages */
LOG_INFO("GPIO pin %u set to HIGH", pin);

/* Use named constants */
#define BUFFER_SIZE 256
for (int i = 0; i < BUFFER_SIZE; i++) { ... }

/* Check allocation */
uint8_t *buffer = malloc_safe(256);
if (buffer == NULL) return HAL_ERROR;

/* Explain the WHY */
/* Wait 50ms for device to settle after reset */
usleep(50000);
```

---

## Questions?

If you have questions about the codebase or these guidelines, open an issue and we'll clarify!
