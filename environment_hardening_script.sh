echo "Hello, Rohit!"
date

BOOST_PATH_CHECK="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor" #replacing cpu* with cpu0 to pass if -f condition checks (if cpu0 has a scaling governor, the rest of the cores will too have it)
KERNEL_BOOST_MODIFICATION_PATH="/sys/devices/system/cpu/cpufreq/boost"

found=false

check_if_file_found() {
    if [ -f "$BOOST_PATH_CHECK" ]; then
        found=true
    fi
}

force_all_ZEN3_cores_to_base_frequency_floor() {
    check_if_file_found
    if $found; then
        echo performance | sudo tee "/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"
        echo "0" | sudo tee "$KERNEL_BOOST_MODIFICATION_PATH" # to reenable Turbo Boost - we replace 0 with 1 and then run the script. 
        echo "Operation successful: Turbo Boost has been disabled";
    else
        echo "Operation failed: Turbo boost could not be disabled as file was not found";
    fi
}

ensure_core_one_is_offline_or_idle() {
    # Since we would be working with core 0 and 2, we need core 1  (sibling of core 0 to be offline or idle to avoid resource contention in ALU)\
    CORE_ONE_FILE_PATH="/sys/devices/system/cpu/cpu1/online"
    echo 0 | sudo tee "$CORE_ONE_FILE_PATH" # to reenable core1 (change to active and online), replace 0 with 1 and follow the below given instructions
    core_one_offline_flag=$(cat $CORE_ONE_FILE_PATH)
    if [ "$core_one_offline_flag" = "0" ]; then
        echo "Operation successful: Core 1 is now offline or idle";
    else
        echo "Operation unsuccessful: Core 1 is still online";
    fi
}

force_all_ZEN3_cores_to_base_frequency_floor
ensure_core_one_is_offline_or_idle

# Reenable Turbo Boost guidelines:
# Remove amd_pstate=passive from GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub file 
# sudo update-grub
# sudo reboot
# then run this script after replacing 0 with 1 inside force_all_ZEN3_cores_to_base_frequency_floor function

# Flip Core 1 back to online and active state
# In the ensure_core_one_is_offline_or_idle function, we replace the echo 0 with echo 1 and just run the script (Ignore the message printed to the terminal about unsuccessful operation)
# To verify: use cat /sys/devices/system/cpu/cpu1/online command from the termina, if the output to terminal is 1, then you have succeeded and core 1 is back online and active
# 0 means offline and idle while 1 means online and active