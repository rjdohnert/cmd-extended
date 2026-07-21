#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <chrono>
#include <thread>
#include <functional>

#pragma comment(lib, "pdh.lib")

// Struct to store disk metrics per drive instance
struct DiskStats {
    double reads_sec = 0.0;
    double writes_sec = 0.0;
    double read_bytes_sec = 0.0;
    double write_bytes_sec = 0.0;
    double pct_disk_time = 0.0;
    double queue_length = 0.0;
};

// Helper to convert UTF-16 wide strings (Windows default) to UTF-8 narrow strings for output
std::string WideToNarrow(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Safely registers performance counters using localized English names
PDH_HCOUNTER AddCounter(PDH_HQUERY hQuery, const std::wstring& path) {
    PDH_HCOUNTER hCounter = NULL;
    PDH_STATUS status = PdhAddEnglishCounterW(hQuery, path.c_str(), 0, &hCounter);
    if (status != ERROR_SUCCESS) {
        std::wcerr << L"Error: Failed to register counter: " << path 
                   << L" (Status code: 0x" << std::hex << status << L")\n";
    }
    return hCounter;
}

// Queries wildcard array values (like \PhysicalDisk(*)\...) and aligns them by disk name
void FetchDiskCounterArray(PDH_HCOUNTER hCounter, std::function<void(const std::wstring&, double)> assign_fn) {
    DWORD dwBufferSize = 0;
    DWORD dwItemCount = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
    
    if (status == PDH_MORE_DATA) {
        std::vector<BYTE> buffer(dwBufferSize);
        PDH_FMT_COUNTERVALUE_ITEM_W* pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        status = PdhGetFormattedCounterArrayW(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
        
        if (status == ERROR_SUCCESS) {
            for (DWORD i = 0; i < dwItemCount; ++i) {
                if (pItems[i].szName) {
                    assign_fn(pItems[i].szName, pItems[i].FmtValue.doubleValue);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int interval = 0; // Duration to wait between updates (0 runs a single report)
    int count = -1;   // Number of iterations to perform (-1 implies indefinite loop)

    // Command-line argument parsing
    if (argc > 1) {
        try {
            interval = std::stoi(argv[1]);
            if (interval < 1) interval = 1;
        } catch (...) {
            std::cerr << "Usage: wiostat [interval] [count]\n";
            return 1;
        }
    }
    if (argc > 2) {
        try {
            count = std::stoi(argv[2]);
            if (count < 1) count = 1;
        } catch (...) {
            std::cerr << "Usage: wiostat [interval] [count]\n";
            return 1;
        }
    }

    PDH_HQUERY hQuery = NULL;
    if (PdhOpenQueryW(NULL, 0, &hQuery) != ERROR_SUCCESS) {
        std::cerr << "Fatal Error: Failed to initialize PDH Query engine.\n";
        return 1;
    }

    // Register CPU stats counters
    PDH_HCOUNTER hCpuUser = AddCounter(hQuery, L"\\Processor(_Total)\\% User Time");
    PDH_HCOUNTER hCpuSys = AddCounter(hQuery, L"\\Processor(_Total)\\% Privileged Time");
    PDH_HCOUNTER hCpuIdle = AddCounter(hQuery, L"\\Processor(_Total)\\% Idle Time");

    // Register wild-carded Physical Disk counters
    PDH_HCOUNTER hReadsSec = AddCounter(hQuery, L"\\PhysicalDisk(*)\\Disk Reads/sec");
    PDH_HCOUNTER hWritesSec = AddCounter(hQuery, L"\\PhysicalDisk(*)\\Disk Writes/sec");
    PDH_HCOUNTER hReadBytesSec = AddCounter(hQuery, L"\\PhysicalDisk(*)\\Disk Read Bytes/sec");
    PDH_HCOUNTER hWriteBytesSec = AddCounter(hQuery, L"\\PhysicalDisk(*)\\Disk Write Bytes/sec");
    PDH_HCOUNTER hPctDiskTime = AddCounter(hQuery, L"\\PhysicalDisk(*)\\% Disk Time");
    PDH_HCOUNTER hQueueLength = AddCounter(hQuery, L"\\PhysicalDisk(*)\\Current Disk Queue Length");

    if (!hCpuUser || !hCpuSys || !hCpuIdle || !hReadsSec || !hWritesSec || 
        !hReadBytesSec || !hWriteBytesSec || !hPctDiskTime || !hQueueLength) {
        std::cerr << "Fatal Error: One or more critical performance counters could not be resolved.\n";
        PdhCloseQuery(hQuery);
        return 1;
    }

    // Collect baseline query data (required to establish rates for the first report)
    PdhCollectQueryData(hQuery);

    std::cout << "\n";
    std::cout << "Collecting metrics... Use Ctrl+C to terminate.\n";

    int iterations = 0;
    while (true) {
        if (count != -1 && iterations >= count) {
            break;
        }

        // Wait for the specified polling interval (defaults to 1s if running a single report)
        std::this_thread::sleep_for(std::chrono::seconds(interval == 0 ? 1 : interval));

        PDH_STATUS status = PdhCollectQueryData(hQuery);
        if (status != ERROR_SUCCESS) {
            std::cerr << "Error collecting query data. Status code: 0x" << std::hex << status << "\n";
            break;
        }

        // 1. Fetch CPU metrics
        double user_pct = 0.0, sys_pct = 0.0, idle_pct = 0.0;
        PDH_FMT_COUNTERVALUE cvUser, cvSys, cvIdle;
        if (PdhGetFormattedCounterValue(hCpuUser, PDH_FMT_DOUBLE, NULL, &cvUser) == ERROR_SUCCESS) {
            user_pct = cvUser.doubleValue;
        }
        if (PdhGetFormattedCounterValue(hCpuSys, PDH_FMT_DOUBLE, NULL, &cvSys) == ERROR_SUCCESS) {
            sys_pct = cvSys.doubleValue;
        }
        if (PdhGetFormattedCounterValue(hCpuIdle, PDH_FMT_DOUBLE, NULL, &cvIdle) == ERROR_SUCCESS) {
            idle_pct = cvIdle.doubleValue;
        }

        // 2. Fetch and align wild-carded Disk metrics
        std::map<std::wstring, DiskStats> disk_map;
        FetchDiskCounterArray(hReadsSec, [&](const std::wstring& name, double val) { disk_map[name].reads_sec = val; });
        FetchDiskCounterArray(hWritesSec, [&](const std::wstring& name, double val) { disk_map[name].writes_sec = val; });
        FetchDiskCounterArray(hReadBytesSec, [&](const std::wstring& name, double val) { disk_map[name].read_bytes_sec = val; });
        FetchDiskCounterArray(hWriteBytesSec, [&](const std::wstring& name, double val) { disk_map[name].write_bytes_sec = val; });
        FetchDiskCounterArray(hPctDiskTime, [&](const std::wstring& name, double val) { disk_map[name].pct_disk_time = val; });
        FetchDiskCounterArray(hQueueLength, [&](const std::wstring& name, double val) { disk_map[name].queue_length = val; });

        // Divide aggregated disk pools from raw disk volumes for formatting
        std::vector<std::pair<std::wstring, DiskStats>> individual_disks;
        std::pair<std::wstring, DiskStats> total_disk;
        bool has_total = false;

        for (auto const& pair : disk_map) {
            if (pair.first == L"_Total") {
                total_disk = pair;
                has_total = true;
            } else {
                individual_disks.push_back(pair);
            }
        }

        // 3. Print avg-cpu utilization layout
        std::cout << "\navg-cpu:  %user   %sys   %idle\n";
        std::cout << "          " 
                  << std::setw(5) << std::fixed << std::setprecision(2) << user_pct << "  "
                  << std::setw(5) << sys_pct << "  "
                  << std::setw(5) << idle_pct << "\n\n";

        // 4. Print physical device utilization layout
        std::cout << std::left << std::setw(22) << "Device"
                  << std::right << std::setw(10) << "tps"
                  << std::setw(14) << "kB_read/s"
                  << std::setw(14) << "kB_wrtn/s"
                  << std::setw(10) << "aqu-sz"
                  << std::setw(10) << "%util" << "\n";

        auto PrintRow = [](const std::wstring& wname, const DiskStats& stats) {
            std::string name = WideToNarrow(wname);
            double tps = stats.reads_sec + stats.writes_sec;
            double kb_read_s = stats.read_bytes_sec / 1024.0;
            double kb_wrtn_s = stats.write_bytes_sec / 1024.0;
            double aqu_sz = stats.queue_length;
            double util = stats.pct_disk_time;

            std::cout << std::left << std::setw(22) << name
                      << std::right << std::setw(10) << std::fixed << std::setprecision(2) << tps
                      << std::setw(14) << kb_read_s
                      << std::setw(14) << kb_wrtn_s
                      << std::setw(10) << aqu_sz
                      << std::setw(10) << util << "\n";
        };

        for (const auto& disk : individual_disks) {
            PrintRow(disk.first, disk.second);
        }

        if (has_total) {
            std::cout << std::string(80, '-') << "\n";
            PrintRow(total_disk.first, total_disk.second);
        }

        iterations++;
        if (interval == 0) {
            break; // Exit after single iteration if interval was not specified
        }
    }

    PdhCloseQuery(hQuery);
    return 0;
}