#!/bin/bash

# diagnose.sh - Diagnostic script for Orange Pi

# Log file location
timestamp=$(date +'%Y-%m-%d %H:%M:%S')
logfile="/var/log/orange_pi_diagnostics.log"
echo "Diagnostics started at $timestamp" >> $logfile

### Function to check CPU
check_cpu() {
    echo "Checking CPU..." >> $logfile
    cpu_info=$(lscpu)
    echo "$cpu_info" >> $logfile
}

### Function to check RAM
check_ram() {
    echo "Checking RAM..." >> $logfile
    ram_info=$(free -h)
    echo "$ram_info" >> $logfile
}

### Function to check Disk Space
check_disk() {
    echo "Checking Disk Space..." >> $logfile
    disk_info=$(df -h)
    echo "$disk_info" >> $logfile
}

### Function to check Network
check_network() {
    echo "Checking Network..." >> $logfile
    network_info=$(ifconfig)
    echo "$network_info" >> $logfile
}

### Run checks
check_cpu
check_ram
check_disk
check_network

echo "Diagnostics completed at $(date +'%Y-%m-%d %H:%M:%S')" >> $logfile
