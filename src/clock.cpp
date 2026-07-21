#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

int main() {
    // Get the current system time
    auto now = std::chrono::system_clock::now();
    
    // Convert the system time to a time_t object
    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    
    std::tm local_time;

    // Use localtime_s on MSVC (Windows), otherwise fallback to standard localtime
#if defined(_MSC_VER)
    if (localtime_s(&local_time, &now_time_t) != 0) {
        std::cerr << "Error retrieving local time." << std::endl;
        return 1;
    }
#else
    std::tm* tm_ptr = std::localtime(&now_time_t);
    if (tm_ptr == nullptr) {
        std::cerr << "Error retrieving local time." << std::endl;
        return 1;
    }
    local_time = *tm_ptr;
#endif

    // Print the formatted date and time
    // Format: YYYY-MM-DD HH:MM:SS
    std::cout << "Current date and time: " 
              << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") 
              << std::endl;

    return 0;
}