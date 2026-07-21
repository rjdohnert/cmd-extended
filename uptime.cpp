#include <windows.h>
#include <wtsapi32.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

#pragma comment(lib, "wtsapi32.lib")

// Retrieves system-wide active CPU utilization over a 100ms window as a decimal fraction (0.00 - 1.00)
double GetCPUUtilization() {
    FILETIME idleTime1, kernelTime1, userTime1;
    FILETIME idleTime2, kernelTime2, userTime2;
    
    if (!GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) return 0.0;
    Sleep(100); // 100ms sampling window
    if (!GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) return 0.0;
    
    auto FileTimeToULL = [](const FILETIME& ft) {
        return (unsigned long long)ft.dwLowDateTime | ((unsigned long long)ft.dwHighDateTime << 32);
    };
    
    unsigned long long idle1 = FileTimeToULL(idleTime1);
    unsigned long long kernel1 = FileTimeToULL(kernelTime1);
    unsigned long long user1 = FileTimeToULL(userTime1);
    
    unsigned long long idle2 = FileTimeToULL(idleTime2);
    unsigned long long kernel2 = FileTimeToULL(kernelTime2);
    unsigned long long user2 = FileTimeToULL(userTime2);
    
    unsigned long long idleDiff = idle2 - idle1;
    unsigned long long kernelDiff = kernel2 - kernel1;
    unsigned long long userDiff = user2 - user1;
    
    unsigned long long totalSys = kernelDiff + userDiff;
    if (totalSys == 0) return 0.0;
    
    // Windows kernel times include idle time; subtract to isolate active cycles
    unsigned long long activeSys = totalSys - idleDiff;
    return static_cast<double>(activeSys) / totalSys;
}

// Counts the number of active interactive desktop sessions on the machine
int GetActiveUserCount() {
    PWTS_SESSION_INFOW pSessionInfo = NULL;
    DWORD dwCount = 0;
    int activeUsers = 0;
    
    if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &dwCount)) {
        for (DWORD i = 0; i < dwCount; ++i) {
            // Exclude Session 0 (non-interactive Windows services)
            if (pSessionInfo[i].SessionId != 0 && pSessionInfo[i].State == WTSActive) {
                activeUsers++;
            }
        }
        WTSFreeMemory(pSessionInfo);
    }
    // Fall back to 1 (current session) if query returns 0 (such as under certain restrictions)
    return activeUsers > 0 ? activeUsers : 1;
}

// Converts system tick count to a formatted BSD-style uptime string
std::string GetUptimeString() {
    ULONGLONG ms = GetTickCount64();
    ULONGLONG seconds = ms / 1000;
    ULONGLONG minutes = seconds / 60;
    ULONGLONG hours = minutes / 60;
    ULONGLONG days = hours / 24;
    
    minutes %= 60;
    hours %= 24;
    
    std::stringstream ss;
    ss << "up ";
    if (days > 0) {
        ss << days << (days == 1 ? " day, " : " days, ");
        ss << std::setw(2) << std::setfill('0') << hours << ":"
           << std::setw(2) << std::setfill('0') << minutes;
    } else if (hours > 0) {
        ss << hours << ":" << std::setw(2) << std::setfill('0') << minutes;
    } else {
        ss << minutes << (minutes == 1 ? " min" : " mins");
    }
    return ss.str();
}

// Formats the current local system time matching BSD layout (e.g. " 9:57 PM")
std::string GetCurrentTimeFormatted() {
    time_t now = time(nullptr);
    tm ltm;
    localtime_s(&ltm, &now);
    
    char buf[32];
    strftime(buf, sizeof(buf), "%I:%M %p", &ltm);
    
    // Trim leading zero if present for 12-hour format alignment
    std::string s(buf);
    if (!s.empty() && s[0] == '0') {
        s = s.substr(1);
    }
    return s;
}

void PrintHelp() {
    std::cout << "Usage: uptime\n"
              << "Display how long the system has been running, active user counts, and CPU load.\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            return 0;
        }
    }

    std::string time_str = GetCurrentTimeFormatted();
    std::string uptime_str = GetUptimeString();
    int user_count = GetActiveUserCount();
    double cpu_load = GetCPUUtilization();

    // Print to stdout matching the standard BSD spacing and alignment layout
    std::cout << " " << time_str << "  " 
              << uptime_str << ",  " 
              << user_count << (user_count == 1 ? " user,  " : " users,  ")
              << "load averages: " 
              << std::fixed << std::setprecision(2) << cpu_load << ", " 
              << cpu_load << ", " 
              << cpu_load << "\n";

    return 0;
}