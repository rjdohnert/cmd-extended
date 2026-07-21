#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <string>
#include <thread>
#include <chrono>

// Link psapi.lib if using MSVC
#pragma comment(lib, "psapi.lib")

// Helper function to convert FILETIME to 64-bit integer
ULONGLONG FileTimeToULL(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// Structures to store process metrics
struct ProcessSnap {
    DWORD pid;
    std::wstring name;
    ULONGLONG processTime;
    SIZE_T workingSetSize;
    DWORD threads;
};

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    double cpuUsage;
    SIZE_T memoryMB;
    DWORD threads;
};

// Enable Virtual Terminal Sequences for ANSI escape codes (Windows 10+)
void EnableVTMode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

// Helper to format system memory
void GetMemoryInfo(ULONGLONG& totalMB, ULONGLONG& usedMB, ULONGLONG& freeMB) {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        totalMB = memStatus.ullTotalPhys / (1024 * 1024);
        freeMB = memStatus.ullAvailPhys / (1024 * 1024);
        usedMB = totalMB - freeMB;
    }
}

// Capture current process snapshot across system
std::unordered_map<DWORD, ProcessSnap> TakeProcessSnapshot() {
    std::unordered_map<DWORD, ProcessSnap> map;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return map;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            ProcessSnap snap;
            snap.pid = pe32.th32ProcessID;
            snap.name = pe32.szExeFile;
            snap.threads = pe32.cntThreads;
            snap.processTime = 0;
            snap.workingSetSize = 0;

            // Attempt to query process times and memory
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, snap.pid);
            if (hProc) {
                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    snap.processTime = FileTimeToULL(ftKernel) + FileTimeToULL(ftUser);
                }

                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                    snap.workingSetSize = pmc.WorkingSetSize;
                }

                CloseHandle(hProc);
            }

            map[snap.pid] = snap;
        } while (Process32NextW(hSnap, &pe32));
    }

    CloseHandle(hSnap);
    return map;
}

int main() {
    // Enable ANSI escapes for clean output formatting
    EnableVTMode();

    constexpr size_t kRowsToDisplay = 20;

    // Initial system times
    FILETIME ftIdle, ftKernel, ftUser;
    GetSystemTimes(&ftIdle, &ftKernel, &ftUser);
    ULONGLONG prevSysTime = FileTimeToULL(ftKernel) + FileTimeToULL(ftUser);
    ULONGLONG prevIdleTime = FileTimeToULL(ftIdle);
    ULONGLONG prevKernelTime = FileTimeToULL(ftKernel);
    ULONGLONG prevUserTime = FileTimeToULL(ftUser);

    auto prevSnaps = TakeProcessSnapshot();

    // Clear Screen once initially
    std::wcout << L"\x1b[2J";

    // Render static title and table header once.
    std::wcout << L"\x1b[H";
    std::wcout << L"\x1b[1m\x1b[0m\x1b[K\n";
    std::wcout << L"\n\n\n";
    std::wcout << L"\x1b[7m"
               << std::left
               << std::setw(8)  << L"PID"
               << std::setw(30) << L"COMMAND"
               << std::setw(10) << L"THREADS"
               << std::setw(12) << L"MEM(MB)"
               << std::setw(10) << L"CPU%"
               << L"\x1b[0m\x1b[K\n";

    while (true) {
        // Sample interval (1 second)
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get current system times
        GetSystemTimes(&ftIdle, &ftKernel, &ftUser);
        ULONGLONG currSysTime = FileTimeToULL(ftKernel) + FileTimeToULL(ftUser);
        ULONGLONG currIdleTime = FileTimeToULL(ftIdle);
        ULONGLONG currKernelTime = FileTimeToULL(ftKernel);
        ULONGLONG currUserTime = FileTimeToULL(ftUser);

        ULONGLONG sysTimeDelta = currSysTime - prevSysTime;
        ULONGLONG idleTimeDelta = currIdleTime - prevIdleTime;
        ULONGLONG kernelTimeDelta = currKernelTime - prevKernelTime;
        ULONGLONG userTimeDelta = currUserTime - prevUserTime;

        // Kernel time from Win32 includes Idle time; calculate active kernel time
        ULONGLONG pureKernelDelta = (kernelTimeDelta > idleTimeDelta) ? (kernelTimeDelta - idleTimeDelta) : 0;

        double userCpuPct = sysTimeDelta ? ((double)userTimeDelta / sysTimeDelta) * 100.0 : 0.0;
        double sysCpuPct  = sysTimeDelta ? ((double)pureKernelDelta / sysTimeDelta) * 100.0 : 0.0;
        double idleCpuPct = sysTimeDelta ? ((double)idleTimeDelta / sysTimeDelta) * 100.0 : 0.0;

        // Capture current snapshot
        auto currSnaps = TakeProcessSnapshot();

        // Calculate Process CPU % & collect display items
        std::vector<ProcessInfo> displayList;
        DWORD totalThreads = 0;

        for (const auto& [pid, snap] : currSnaps) {
            totalThreads += snap.threads;
            double procCpu = 0.0;

            if (prevSnaps.count(pid) && sysTimeDelta > 0) {
                ULONGLONG procTimeDelta = snap.processTime - prevSnaps[pid].processTime;
                procCpu = ((double)procTimeDelta / sysTimeDelta) * 100.0;
            }

            displayList.push_back({
                snap.pid,
                snap.name,
                procCpu,
                snap.workingSetSize / (1024 * 1024), // MB
                snap.threads
            });
        }

        // Sort processes by CPU usage descending (secondary sort by Memory)
        std::sort(displayList.begin(), displayList.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
            if (a.cpuUsage != b.cpuUsage)
                return a.cpuUsage > b.cpuUsage;
            return a.memoryMB > b.memoryMB;
        });

        // Memory Stats
        ULONGLONG totalRAM = 0, usedRAM = 0, freeRAM = 0;
        GetMemoryInfo(totalRAM, usedRAM, freeRAM);

        // Refresh only the dynamic stats block.
        std::wcout << L"\x1b[2;1H";
        std::wcout << L"Processes: " << displayList.size() << L" total, "
                   << totalThreads << L" threads\x1b[K\n";
        std::wcout << std::fixed << std::setprecision(1);
        std::wcout << L"CPU: " << userCpuPct << L"% user, "
                   << sysCpuPct << L"% sys, "
                   << idleCpuPct << L"% idle\x1b[K\n";
        std::wcout << L"Mem: " << totalRAM << L"M total, "
                   << usedRAM << L"M used, "
                   << freeRAM << L"M free\x1b[K\n";

        // Move cursor to first process row (line 6, column 1).
        std::wcout << L"\x1b[6;1H";

        size_t rowsToDisplay = (std::min)(kRowsToDisplay, displayList.size());
        for (size_t i = 0; i < kRowsToDisplay; ++i) {
            if (i < rowsToDisplay) {
                const auto& proc = displayList[i];

                // Truncate process name if too long
                std::wstring name = proc.name;
                if (name.length() > 28) name = name.substr(0, 25) + L"...";

                std::wcout << std::left  << std::setw(8)  << proc.pid
                           << std::setw(30) << name
                           << std::setw(10) << proc.threads
                           << std::setw(12) << proc.memoryMB
                           << std::right << std::setw(6) << std::fixed << std::setprecision(1) << proc.cpuUsage << L"%"
                           << L"\x1b[K\n";
            } else {
                // Clear remaining lines from previous refresh.
                std::wcout << L"\x1b[K\n";
            }
        }
        std::wcout.flush();

        // Save current snapshot for next cycle
        prevSnaps = std::move(currSnaps);
        prevSysTime = currSysTime;
        prevIdleTime = currIdleTime;
        prevKernelTime = currKernelTime;
        prevUserTime = currUserTime;
    }

    return 0;
}