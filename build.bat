@echo off
REM Loki Build Script for Windows CMD
REM Requires: arm-linux-gnueabihf-gcc cross-compiler

setlocal enabledelayedexpansion

set MODE=%1
if "%MODE%"=="" set MODE=debug

set HOST=%2
if "%HOST%"=="" set HOST=orange-pi.local

set USER=%3
if "%USER%"=="" set USER=pi

if "%MODE%"=="release" (
    set OPTIMIZE=-O3
    set BUILD_DIR=build\release
    set DEBUG=0
    set LOG_LEVEL=2
) else (
    set OPTIMIZE=-g -O0
    set BUILD_DIR=build\debug
    set DEBUG=1
    set LOG_LEVEL=4
)

echo [INFO] Build Configuration
echo   Mode: %MODE%
echo   Build directory: %BUILD_DIR%
echo.

REM Check for compiler
where arm-linux-gnueabihf-gcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] arm-linux-gnueabihf-gcc not found!
    echo [WARN] Install ARM cross-compiler from: https://developer.arm.com
    exit /b 1
)

REM Create build directories
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BUILD_DIR%\hal\gpio" mkdir "%BUILD_DIR%\hal\gpio"
if not exist "%BUILD_DIR%\hal\spi" mkdir "%BUILD_DIR%\hal\spi"
if not exist "%BUILD_DIR%\hal\i2c" mkdir "%BUILD_DIR%\hal\i2c"
if not exist "%BUILD_DIR%\hal\uart" mkdir "%BUILD_DIR%\hal\uart"
if not exist "%BUILD_DIR%\hal\pwm" mkdir "%BUILD_DIR%\hal\pwm"
if not exist "%BUILD_DIR%\drivers\tft" mkdir "%BUILD_DIR%\drivers\tft"
if not exist "%BUILD_DIR%\drivers\sdcard" mkdir "%BUILD_DIR%\drivers\sdcard"
if not exist "%BUILD_DIR%\drivers\flash" mkdir "%BUILD_DIR%\drivers\flash"
if not exist "%BUILD_DIR%\drivers\eeprom" mkdir "%BUILD_DIR%\drivers\eeprom"
if not exist "%BUILD_DIR%\drivers\flipper_uart" mkdir "%BUILD_DIR%\drivers\flipper_uart"
if not exist "%BUILD_DIR%\core" mkdir "%BUILD_DIR%\core"
if not exist "%BUILD_DIR%\utils" mkdir "%BUILD_DIR%\utils"

echo [INFO] Compiling sources...

set CFLAGS=-Wall -Wextra -march=armv7-a -mtune=cortex-a7 -I. -I.\includes -I.\config -I.\utils %OPTIMIZE% -DDEBUG=%DEBUG% -DLOG_LEVEL=%LOG_LEVEL%

REM Compile HAL
echo [CC] hal/gpio/gpio.c
arm-linux-gnueabihf-gcc %CFLAGS% -c hal/gpio/gpio.c -o %BUILD_DIR%/hal/gpio/gpio.o
if errorlevel 1 goto compile_error

echo [CC] hal/spi/spi.c
arm-linux-gnueabihf-gcc %CFLAGS% -c hal/spi/spi.c -o %BUILD_DIR%/hal/spi/spi.o
if errorlevel 1 goto compile_error

echo [CC] hal/i2c/i2c.c
arm-linux-gnueabihf-gcc %CFLAGS% -c hal/i2c/i2c.c -o %BUILD_DIR%/hal/i2c/i2c.o
if errorlevel 1 goto compile_error

echo [CC] hal/uart/uart.c
arm-linux-gnueabihf-gcc %CFLAGS% -c hal/uart/uart.c -o %BUILD_DIR%/hal/uart/uart.o
if errorlevel 1 goto compile_error

echo [CC] hal/pwm/pwm.c
arm-linux-gnueabihf-gcc %CFLAGS% -c hal/pwm/pwm.c -o %BUILD_DIR%/hal/pwm/pwm.o
if errorlevel 1 goto compile_error

REM Compile drivers
echo [CC] drivers/tft/tft_driver.c
arm-linux-gnueabihf-gcc %CFLAGS% -c drivers/tft/tft_driver.c -o %BUILD_DIR%/drivers/tft/tft_driver.o
if errorlevel 1 goto compile_error

echo [CC] drivers/sdcard/sdcard_driver.c
arm-linux-gnueabihf-gcc %CFLAGS% -c drivers/sdcard/sdcard_driver.c -o %BUILD_DIR%/drivers/sdcard/sdcard_driver.o
if errorlevel 1 goto compile_error

echo [CC] drivers/flash/flash_driver.c
arm-linux-gnueabihf-gcc %CFLAGS% -c drivers/flash/flash_driver.c -o %BUILD_DIR%/drivers/flash/flash_driver.o
if errorlevel 1 goto compile_error

echo [CC] drivers/eeprom/eeprom_driver.c
arm-linux-gnueabihf-gcc %CFLAGS% -c drivers/eeprom/eeprom_driver.c -o %BUILD_DIR%/drivers/eeprom/eeprom_driver.o
if errorlevel 1 goto compile_error

echo [CC] drivers/flipper_uart/flipper_uart.c
arm-linux-gnueabihf-gcc %CFLAGS% -c drivers/flipper_uart/flipper_uart.c -o %BUILD_DIR%/drivers/flipper_uart/flipper_uart.o
if errorlevel 1 goto compile_error

REM Compile core
echo [CC] core/system.c
arm-linux-gnueabihf-gcc %CFLAGS% -c core/system.c -o %BUILD_DIR%/core/system.o
if errorlevel 1 goto compile_error

echo [CC] core/main.c
arm-linux-gnueabihf-gcc %CFLAGS% -c core/main.c -o %BUILD_DIR%/core/main.o
if errorlevel 1 goto compile_error

REM Compile utilities
echo [CC] utils/log.c
arm-linux-gnueabihf-gcc %CFLAGS% -c utils/log.c -o %BUILD_DIR%/utils/log.o
if errorlevel 1 goto compile_error

echo [CC] utils/memory.c
arm-linux-gnueabihf-gcc %CFLAGS% -c utils/memory.c -o %BUILD_DIR%/utils/memory.o
if errorlevel 1 goto compile_error

echo [CC] utils/retry.c
arm-linux-gnueabihf-gcc %CFLAGS% -c utils/retry.c -o %BUILD_DIR%/utils/retry.o
if errorlevel 1 goto compile_error

echo [OK] Compilation successful
echo.
echo [INFO] Linking...

REM Link all objects
arm-linux-gnueabihf-gcc -o %BUILD_DIR%/loki_app ^
    %BUILD_DIR%/hal/gpio/gpio.o ^
    %BUILD_DIR%/hal/spi/spi.o ^
    %BUILD_DIR%/hal/i2c/i2c.o ^
    %BUILD_DIR%/hal/uart/uart.o ^
    %BUILD_DIR%/hal/pwm/pwm.o ^
    %BUILD_DIR%/drivers/tft/tft_driver.o ^
    %BUILD_DIR%/drivers/sdcard/sdcard_driver.o ^
    %BUILD_DIR%/drivers/flash/flash_driver.o ^
    %BUILD_DIR%/drivers/eeprom/eeprom_driver.o ^
    %BUILD_DIR%/drivers/flipper_uart/flipper_uart.o ^
    %BUILD_DIR%/core/system.o ^
    %BUILD_DIR%/core/main.o ^
    %BUILD_DIR%/utils/log.o ^
    %BUILD_DIR%/utils/memory.o ^
    %BUILD_DIR%/utils/retry.o ^
    -lm -lpthread

if errorlevel 1 (
    echo [ERROR] Linking failed
    exit /b 1
)

for %%F in (%BUILD_DIR%\loki_app) do set SIZE=%%~zF
echo [OK] Successfully built loki_app (%SIZE% bytes)
echo.

if "%4"=="--install" (
    echo [INFO] Checking SSH connection to %USER%@%HOST%...
    ssh %USER%@%HOST% "echo OK" >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] SSH connection failed to %USER%@%HOST%
        echo [WARN] Ensure:
        echo   1. Orange Pi is powered on
        echo   2. Host can reach %HOST%
        echo   3. SSH is enabled on Orange Pi
        echo   4. Credentials: user=%USER%
        exit /b 1
    )
    
    echo [INFO] Copying binary to %USER%@%HOST%:/tmp/
    scp %BUILD_DIR%\loki_app %USER%@%HOST%:/tmp/
    
    echo [INFO] Making executable...
    ssh %USER%@%HOST% "chmod +x /tmp/loki_app"
    
    echo [OK] Installation complete
    echo [INFO] To run: ssh %USER%@%HOST% "sudo /tmp/loki_app"
) else (
    echo [INFO] Build complete. Use --install to deploy to Orange Pi
    echo.
    echo Usage:
    echo   build.bat debug
    echo   build.bat release
    echo   build.bat debug orange-pi.local pi --install
)

exit /b 0

:compile_error
echo [ERROR] Compilation failed
exit /b 1
