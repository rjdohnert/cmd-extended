#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <iomanip>
#include <string>

// Version: 2.0

// Link with ntdll.lib for RtlGetVersion
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

typedef NTSTATUS(WINAPI* LPFN_RTLGETVERSION)(PRTL_OSVERSIONINFOEXW);

// Function to retrieve and display the Windows OS version details
void ShowOSVersion() {
    std::wcout << L"" << std::endl;

    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        LPFN_RTLGETVERSION pRtlGetVersion = (LPFN_RTLGETVERSION)GetProcAddress(hMod, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOEXW osvi = { 0 };
            osvi.dwOSVersionInfoSize = sizeof(osvi);

            if (pRtlGetVersion(&osvi) == 0) { // STATUS_SUCCESS is 0
                std::wstring osName = L"Windows (Unknown)";

                // Windows 11 and Windows 10 share Major 10, Minor 0.
                // Windows 11 is identified by Build Number >= 22000.
                if (osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0) {
                    if (osvi.dwBuildNumber >= 22000) {
                        osName = L"Windows 11";
                    } else {
                        osName = L"Windows 10";
                    }
                } else if (osvi.dwMajorVersion == 6) {
                    if (osvi.dwMinorVersion == 3) osName = L"Windows 8.1";
                    else if (osvi.dwMinorVersion == 2) osName = L"Windows 8";
                    else if (osvi.dwMinorVersion == 1) osName = L"Windows 7";
                    else if (osvi.dwMinorVersion == 0) osName = L"Windows Vista";
                } else if (osvi.dwMajorVersion == 5) {
                    if (osvi.dwMinorVersion == 2) osName = L"Windows XP";
                    else if (osvi.dwMinorVersion == 1) osName = L"Windows 2000";
                }

                std::wcout << L"OS Name:       " << osName << std::endl;
                std::wcout << L"Kernel Version: NT " << osvi.dwMajorVersion << L"." 
                           << osvi.dwMinorVersion << L" (Build " << osvi.dwBuildNumber << L")" << std::endl;
                return;
            }
        }
    }
    std::wcout << L"Unable to determine detailed OS version." << std::endl;
}

// Function to retrieve and display system uptime
void ShowSystemUptime() {
    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG totalSeconds = uptimeMs / 1000;

    ULONGLONG days = totalSeconds / 86400;
    ULONGLONG hours = (totalSeconds % 86400) / 3600;
    ULONGLONG minutes = (totalSeconds % 3600) / 60;
    ULONGLONG seconds = totalSeconds % 60;

    std::wcout << L"\n" << std::endl;
    std::wcout << days << L" days, " 
               << hours << L" hours, " 
               << minutes << L" minutes, " 
               << seconds << L" seconds" << std::endl;
}

// Function to retrieve and display current memory usage
void ShowMemoryUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    std::wcout << L"\n--- Memory Usage ---" << std::endl;
    std::wcout << L"\n" << std::endl;

    if (GlobalMemoryStatusEx(&memInfo)) {
        const double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;

        double totalPhysGB = memInfo.ullTotalPhys / BYTES_TO_GB;
        double availPhysGB = memInfo.ullAvailPhys / BYTES_TO_GB;
        double usedPhysGB = totalPhysGB - availPhysGB;

        double totalVirtGB = memInfo.ullTotalPageFile / BYTES_TO_GB;
        double availVirtGB = memInfo.ullAvailPageFile / BYTES_TO_GB;
        double usedVirtGB = totalVirtGB - availVirtGB;

        std::wcout << std::fixed << std::setprecision(2);
        std::wcout << L"Physical Memory (RAM):" << std::endl;
        std::wcout << L"  Total:    " << totalPhysGB << L" GB" << std::endl;
        std::wcout << L"  Used:     " << usedPhysGB << L" GB (" << memInfo.dwMemoryLoad << L"%)" << std::endl;
        std::wcout << L"  Available:" << availPhysGB << L" GB" << std::endl;

        std::wcout << L"\nVirtual Memory:" << std::endl;
        std::wcout << L"  Total:    " << totalVirtGB << L" GB" << std::endl;
        std::wcout << L"  Used:     " << usedVirtGB << L" GB" << std::endl;
        std::wcout << L"  Available:" << availVirtGB << L" GB" << std::endl;
    } else {
        std::wcerr << L"Unable to retrieve memory status. Error code: " << GetLastError() << std::endl;
    }
}

// Function to list all currently running processes
void ListRunningProcesses() {
    std::wcout << L"\n--- Current System Processes ---" << std::endl;
    std::wcout << L"\n" << std::endl;
    std::wcout << std::left << std::setw(10) << L"PID" << std::setw(28) << L"Process Name" << std::setw(16) << L"Memory (MB)" << std::endl;
    std::wcout << std::wstring(60, L'-') << std::endl;

    // Take a snapshot of all processes in the system
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create toolhelp snapshot. Error code: " << GetLastError() << std::endl;
        return;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    // Retrieve information about the first process
    if (!Process32FirstW(hProcessSnap, &pe32)) {
        std::wcerr << L"Failed to retrieve first process information." << std::endl;
        CloseHandle(hProcessSnap);
        return;
    }

    int processCount = 0;
    double totalMemMB = 0.0;

    // Iterate through the list of processes
    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
        double memMB = 0.0;

        if (hProcess) {
            PROCESS_MEMORY_COUNTERS_EX pmc;
            pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                memMB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
            }
            CloseHandle(hProcess);
        }

        processCount++;
        totalMemMB += memMB;

        std::wcout << std::left << std::setw(10) << pe32.th32ProcessID
                   << std::setw(28) << pe32.szExeFile
                   << std::setw(16) << std::fixed << std::setprecision(2) << memMB << std::endl;
    } while (Process32NextW(hProcessSnap, &pe32));

    std::wcout << std::wstring(60, L'-') << std::endl;
    std::wcout << L"Total processes: " << processCount << std::endl;
    std::wcout << L"Total memory used: " << std::fixed << std::setprecision(2) << totalMemMB << L" MB" << std::endl;
    std::wcout << L"\n" << std::endl;

    CloseHandle(hProcessSnap);
}

int main() {
    // Set console output to handle Unicode formatting correctly
    std::locale::global(std::locale(""));
    
    ShowOSVersion();
    ShowSystemUptime();
    ShowMemoryUsage();
    ListRunningProcesses();

    return 0;
}