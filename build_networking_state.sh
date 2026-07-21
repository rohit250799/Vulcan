date

file_found=false

verify_threaded_napi_status() {
    if ps -ef | grep napi; then
        return 0
    else
        return 1
    fi
}

check_if_threaded_file_exists_in_directory() {
    THREADED_FILE_PATH="/sys/class/net/wlo1/threaded"
    if [ -f "$THREADED_FILE_PATH" ]; then
        file_found=true
        return 0
    else
        echo "The file $THREADED_FILE_PATH does not exist"
        return 1
    fi
}

# explicitly control threaded NAPI mode for your network interface,
enable_threaded_napi() {
    if [ "$file_found" = true ]; then
        echo "Attempting to enable threaded NAPI..."
        if echo 1 | sudo tee /sys/class/net/wlo1/threaded 2>/dev/null; then
            echo "Threaded NAPI enabled successfully"
            return 0
        else
            echo "Failed: Operation not supported by your network driver"
            echo "This is expected for some wireless/Ethernet drivers"
            return 1
        fi
    else
        echo "Threaded file not found — NAPI cannot be enabled"
        return 1
    fi
}

enable_threaded_napi
