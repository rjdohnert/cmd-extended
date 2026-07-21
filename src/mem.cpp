#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>

// Version: 2.0

int main(int argc, char* argv[]) {
    bool jsonMode = (argc > 1 && std::string(argv[1]) == "--json");

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    // Retrieve system memory status
    if (GlobalMemoryStatusEx(&memInfo)) {
        // Conversion factor from bytes to Gigabytes
        const double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;

        // Physical Memory calculations
        double totalPhysGB = memInfo.ullTotalPhys / BYTES_TO_GB;
        double availPhysGB = memInfo.ullAvailPhys / BYTES_TO_GB;
        double usedPhysGB = totalPhysGB - availPhysGB;

        // Virtual Memory calculations (System Commit Limit / Page File)
        double totalVirtGB = memInfo.ullTotalPageFile / BYTES_TO_GB;
        double availVirtGB = memInfo.ullAvailPageFile / BYTES_TO_GB;
        double usedVirtGB = totalVirtGB - availVirtGB;

        // Configure decimal output formatting
        std::cout << std::fixed << std::setprecision(2);

        if (jsonMode) {
            std::cout << "{\n";
            std::cout << "  \"physical\": {\n";
            std::cout << "    \"totalGB\": " << totalPhysGB << ",\n";
            std::cout << "    \"usedGB\": " << usedPhysGB << ",\n";
            std::cout << "    \"availableGB\": " << availPhysGB << ",\n";
            std::cout << "    \"memoryLoadPercent\": " << memInfo.dwMemoryLoad << "\n";
            std::cout << "  },\n";
            std::cout << "  \"virtual\": {\n";
            std::cout << "    \"totalGB\": " << totalVirtGB << ",\n";
            std::cout << "    \"usedGB\": " << usedVirtGB << ",\n";
            std::cout << "    \"availableGB\": " << availVirtGB << "\n";
            std::cout << "  }\n";
            std::cout << "}\n";
            return 0;
        }

        // Display Physical Memory Status
        std::wcout <<"\n";
        std::wcout << L"Physical Memory (RAM):\n";
        std::wcout << L"  Total Physical Memory:  " << totalPhysGB << " GB\n";
        std::wcout << L"  Used Physical Memory:   " << usedPhysGB << " GB (" << memInfo.dwMemoryLoad << "% in use)\n";
        std::wcout << L"  Available Physical:     " << availPhysGB << " GB\n\n";

        // Display Virtual Memory Status (RAM + Page File)
        std::wcout << L"Virtual Memory (Commit Charge):\n";
        std::wcout << L"  Total Virtual Memory:   " << totalVirtGB << " GB\n";
        std::wcout << L"  Used Virtual Memory:    " << usedVirtGB << " GB\n";
        std::wcout << L"  Available Virtual:      " << availVirtGB << " GB\n";
    } else {
        std::wcerr << L"Error: Unable to retrieve memory status. Error code: " << GetLastError() << L"\n";
        return 1;
    }

    return 0;
}