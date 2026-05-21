echo "Hello, Rohit!"
date

BOOST_PATH_CHECK="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
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
        echo "0" | sudo tee "$KERNEL_BOOST_MODIFICATION_PATH" #to reenable Turbo Boost - we replace 0 with 1 and then run the script. 
        echo "Operation successful: Turbo Boost has been disabled";
    else
        echo "Operation failed: Turbo boost could not be disabled as file was not found";
    fi
}

force_all_ZEN3_cores_to_base_frequency_floor

# Reenable Turbo Boost guidelines:
# Remove amd_pstate=passive from GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub file 
# sudo update-grub
# sudo reboot
# then run this script after replacing 0 with 1 inside force_all_ZEN3_cores_to_base_frequency_floor function