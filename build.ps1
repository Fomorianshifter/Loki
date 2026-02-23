# Loki Build Script for Windows PowerShell
# Requires: arm-linux-gnueabihf-gcc cross-compiler

param(
    [string]$Mode = "debug",
    [switch]$Install = $false,
    [string]$HostName = "orange-pi.local",
    [string]$User = "pi"
)

# Define build directories
$debugFlag = if ($Mode -eq "debug") { 1 } else { 0 }
$buildDir = if ($Mode -eq "debug") { "build\debug" } else { "build\release" }
$optimizeFlag = if ($Mode -eq "debug") { "-g -O0" } else { "-O3" }
$logLevel = if ($Mode -eq "debug") { 4 } else { 2 }

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Loki Build Script for Windows PowerShell" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Check for compiler
Write-Host "[>] Checking for ARM cross-compiler..." -ForegroundColor Cyan
$compiler = Get-Command arm-linux-gnueabihf-gcc -ErrorAction SilentlyContinue
if (-not $compiler) {
    Write-Host "[X] arm-linux-gnueabihf-gcc not found!" -ForegroundColor Red
    Write-Host "[!] Install from: https://developer.arm.com" -ForegroundColor Yellow
    exit 1
}
Write-Host "[+] Compiler found" -ForegroundColor Green
Write-Host ""

# Display configuration
Write-Host "Configuration:" -ForegroundColor Cyan
Write-Host "  Mode:       $Mode"
Write-Host "  Build dir:  $buildDir"
Write-Host "  Optimizer:  $optimizeFlag"
Write-Host ""

# Create build directories
$dirs = @(
    $buildDir,
    "$buildDir\hal\gpio",
    "$buildDir\hal\spi",
    "$buildDir\hal\i2c",
    "$buildDir\hal\uart",
    "$buildDir\hal\pwm",
    "$buildDir\drivers\tft",
    "$buildDir\drivers\sdcard",
    "$buildDir\drivers\flash",
    "$buildDir\drivers\eeprom",
    "$buildDir\drivers\flipper_uart",
    "$buildDir\core",
    "$buildDir\utils"
)

Write-Host "[>] Creating build directories..." -ForegroundColor Cyan
foreach ($dir in $dirs) {
    New-Item -ItemType Directory -Force -Path $dir > $null
}
Write-Host "[+] Directories created" -ForegroundColor Green
Write-Host ""

# List of source files
$sources = @(
    "hal/gpio/gpio.c",
    "hal/spi/spi.c",
    "hal/i2c/i2c.c",
    "hal/uart/uart.c",
    "hal/pwm/pwm.c",
    "drivers/tft/tft_driver.c",
    "drivers/sdcard/sdcard_driver.c",
    "drivers/flash/flash_driver.c",
    "drivers/eeprom/eeprom_driver.c",
    "drivers/flipper_uart/flipper_uart.c",
    "core/system.c",
    "core/main.c",
    "utils/log.c",
    "utils/memory.c",
    "utils/retry.c"
)

Write-Host "[>] Compiling sources..." -ForegroundColor Cyan

$cflags = @(
    "-Wall",
    "-Wextra",
    "-march=armv7-a",
    "-mtune=cortex-a7",
    "-I.",
    "-I.\includes",
    "-I.\config",
    "-I.\utils"
) + $optimizeFlag.Split() + @(
    "-DDEBUG=$debugFlag",
    "-DLOG_LEVEL=$logLevel"
)

$objects = @()
$errors = $false

foreach ($src in $sources) {
    $obj = "$buildDir/$($src -replace '\.c$', '.o')"
    $objPath = $obj -replace '/', '\'
    
    Write-Host "[CC] $src" -ForegroundColor Gray
    
    & arm-linux-gnueabihf-gcc @cflags -c $src -o $objPath 2>&1 | ForEach-Object {
        if ($_ -match "error") {
            Write-Host "     ERROR: $_" -ForegroundColor Red
            $errors = $true
        } else {
            Write-Host "     $_" -ForegroundColor Gray
        }
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[X] Compilation failed for $src" -ForegroundColor Red
        $errors = $true
    }
    
    $objects += $objPath
}

if ($errors) {
    Write-Host "[X] Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "[+] Compilation complete" -ForegroundColor Green
Write-Host ""
Write-Host "[>] Linking..." -ForegroundColor Cyan

$target = "$buildDir\loki_app"

& arm-linux-gnueabihf-gcc -o $target $objects -lm -lpthread 2>&1 | ForEach-Object {
    if ($_ -match "error") {
        Write-Host "[X] $_" -ForegroundColor Red
    }
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "[X] Linking failed" -ForegroundColor Red
    exit 1
}

# Get file size
$fileInfo = Get-Item $target
$sizeKB = [math]::Round($fileInfo.Length / 1024, 2)

Write-Host "[+] Successfully built loki_app ($sizeKB KB)" -ForegroundColor Green
Write-Host ""

if ($Install) {
    Write-Host "[>] Checking SSH connection to port 22..." -ForegroundColor Cyan
    $ssh_check = ssh $User@$HostName "echo OK" 2>&1 | Out-String
    
    if ($LASTEXITCODE -ne 0 -or $ssh_check -notlike "*OK*") {
        Write-Host "[X] SSH connection failed to $User@$HostName" -ForegroundColor Red
        Write-Host "[!] Ensure:" -ForegroundColor Yellow
        Write-Host "    1. Orange Pi is powered on" -ForegroundColor Yellow
        Write-Host "    2. You can ping: ping $HostName" -ForegroundColor Yellow
        Write-Host "    3. SSH is enabled on Orange Pi" -ForegroundColor Yellow
        Write-Host "    4. Correct credentials (user: $User)" -ForegroundColor Yellow
        exit 1
    }
    
    Write-Host "[+] SSH connection OK" -ForegroundColor Green
    Write-Host ""
    Write-Host "[>] Deploying to $User@$HostName" -ForegroundColor Cyan
    
    scp $target "$User@$HostName`:/tmp/"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[X] SCP failed" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "[>] Setting executable permissions..." -ForegroundColor Cyan
    ssh $User@$HostName "chmod +x /tmp/loki_app"
    
    Write-Host "[+] Deployment complete!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next step: ssh $User@$HostName 'sudo /tmp/loki_app'" -ForegroundColor Green
} else {
    Write-Host "[+] Build successful" -ForegroundColor Green
    Write-Host "[!] To deploy: add -Install flag" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Example: .\build.ps1 -Mode release -Install -HostName orange-pi.local" -ForegroundColor Cyan
}

Write-Host ""
