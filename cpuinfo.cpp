// Define the minimum Windows version required for GetLogicalProcessorInformationEx
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <winnt.h> // Required for KAFFINITY type
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include <algorithm>

// Link with Advapi32.lib for Registry functions
#pragma comment(lib, "Advapi32.lib")

// Version: 2.0

// Structures to organize cache information
struct CacheKey {
    DWORD level;
    PROCESSOR_CACHE_TYPE type;
    bool operator<(const CacheKey& other) const {
        if (level != other.level) return level < other.level;
        return type < other.type;
    }
};

struct CacheVal {
    DWORD sizeInBytes;
    DWORD count;
};

DWORD CountSetBits(KAFFINITY mask) {
    DWORD count = 0;
    while (mask) {
        if (mask & 1) {
            ++count;
        }
        mask >>= 1;
    }
    return count;
}

// Helper to query string values from the Registry
std::wstring GetRegistryString(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(hKeyParent, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return L"";
    }

    DWORD valueType = 0;
    DWORD bufferSize = 0;
    std::wstring result;

    LONG queryStatus = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &valueType, nullptr, &bufferSize);
    if (queryStatus == ERROR_SUCCESS && (valueType == REG_SZ || valueType == REG_EXPAND_SZ) && bufferSize >= sizeof(wchar_t)) {
        std::vector<wchar_t> buffer(bufferSize / sizeof(wchar_t));
        if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &bufferSize) == ERROR_SUCCESS) {
            result = buffer.data();
        }
    }

    RegCloseKey(hKey);
    return result;
}

// Helper to query DWORD values from the Registry (e.g., Clock Speed)
DWORD GetRegistryDword(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(hKeyParent, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return 0;
    }

    DWORD value = 0;
    DWORD bufferSize = sizeof(value);
    RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &bufferSize);
    RegCloseKey(hKey);
    return value;
}

// Queries and maps the CPU cache topology
void GetCacheInformation(std::map<CacheKey, CacheVal>& cacheMap) {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return;
    }

    std::vector<BYTE> buffer(length);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());

    if (GetLogicalProcessorInformationEx(RelationCache, info, &length)) {
        DWORD offset = 0;
        while (offset < length) {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX current = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
            if (current->Relationship == RelationCache) {
                CacheKey key = { current->Cache.Level, current->Cache.Type };
                if (cacheMap.find(key) == cacheMap.end()) {
                    cacheMap[key] = { current->Cache.CacheSize, 1 };
                } else {
                    cacheMap[key].count++;
                }
            }
            offset += current->Size;
        }
    }
}

// Queries physical cores and logical thread counts
bool GetProcessorTopology(DWORD& physicalCores, DWORD& logicalThreads) {
    physicalCores = 0;
    logicalThreads = 0;
    DWORD length = 0;

    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }

    std::vector<BYTE> buffer(length);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());

    if (GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) {
        DWORD offset = 0;
        while (offset < length) {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX current = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
            if (current->Relationship == RelationProcessorCore) {
                physicalCores++;
                for (WORD g = 0; g < current->Processor.GroupCount; ++g) {
                    KAFFINITY mask = current->Processor.GroupMask[g].Mask;
                    logicalThreads += CountSetBits(mask);
                }
            }
            offset += current->Size;
        }
        return true;
    }
    return false;
}

std::wstring GetCacheTypeString(PROCESSOR_CACHE_TYPE type) {
    switch (type) {
        case CacheUnified:     return L"Unified";
        case CacheInstruction: return L"Instruction";
        case CacheData:        return L"Data";
        case CacheTrace:       return L"Trace";
        default:               return L"Unknown";
    }
}

int main(int argc, char* argv[]) {
    bool jsonMode = (argc > 1 && std::string(argv[1]) == "--json");

    SYSTEM_INFO sysInfo = {};
    GetNativeSystemInfo(&sysInfo);

    // 1. Processor Architecture
    std::wstring architecture;
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: architecture = L"x64 (64-bit)"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: architecture = L"ARM64 (64-bit)"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: architecture = L"x86 (32-bit)"; break;
        default:                            architecture = L"Unknown Architecture"; break;
    }

    // 2. Hardware Registry Queries
    std::wstring registryPath = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    std::wstring processorName = GetRegistryString(HKEY_LOCAL_MACHINE, registryPath, L"ProcessorNameString");
    std::wstring vendorId      = GetRegistryString(HKEY_LOCAL_MACHINE, registryPath, L"VendorIdentifier");
    DWORD nominalMhz           = GetRegistryDword(HKEY_LOCAL_MACHINE, registryPath, L"~MHz");

    // Clean up extra spaces in the brand name string
    if (!processorName.empty()) {
        size_t first = processorName.find_first_not_of(L" ");
        size_t last = processorName.find_last_not_of(L" ");
        if (first != std::wstring::npos && last != std::wstring::npos) {
            processorName = processorName.substr(first, (last - first + 1));
        }
    }

    // 3. Topology Data
    DWORD physicalCores = 0;
    DWORD logicalThreads = 0;
    GetProcessorTopology(physicalCores, logicalThreads);

    // 4. Cache Data
    std::map<CacheKey, CacheVal> cacheMap;
    GetCacheInformation(cacheMap);

    if (jsonMode) {
        std::wcout << L"{\n";
        std::wcout << L"  \"processorName\": \"" << (processorName.empty() ? L"Unknown" : processorName) << L"\",\n";
        std::wcout << L"  \"manufacturer\": \"" << (vendorId.empty() ? L"Unknown" : vendorId) << L"\",\n";
        std::wcout << L"  \"architecture\": \"" << architecture << L"\",\n";
        std::wcout << L"  \"baseClockMhz\": " << nominalMhz << L",\n";
        std::wcout << L"  \"physicalCores\": " << physicalCores << L",\n";
        std::wcout << L"  \"logicalThreads\": " << logicalThreads << L",\n";
        std::wcout << L"  \"cache\": [\n";

        size_t idx = 0;
        for (std::map<CacheKey, CacheVal>::const_iterator it = cacheMap.begin(); it != cacheMap.end(); ++it, ++idx) {
            const CacheKey& key = it->first;
            const CacheVal& val = it->second;
            std::wcout << L"    {\n";
            std::wcout << L"      \"level\": " << key.level << L",\n";
            std::wcout << L"      \"type\": \"" << GetCacheTypeString(key.type) << L"\",\n";
            std::wcout << L"      \"sizeBytes\": " << val.sizeInBytes << L",\n";
            std::wcout << L"      \"count\": " << val.count << L"\n";
            std::wcout << L"    }" << ((idx + 1 < cacheMap.size()) ? L"," : L"") << L"\n";
        }

        std::wcout << L"  ]\n";
        std::wcout << L"}\n";
        return 0;
    }

    // Render Output
    std::wcout << L"\n";
    std::wcout << L"Processor Name:   " << (processorName.empty() ? L"Unknown" : processorName) << L"\n";
    std::wcout << L"Manufacturer ID:  " << (vendorId.empty() ? L"Unknown" : vendorId) << L"\n";
    std::wcout << L"Architecture:     " << architecture << L"\n";
    
    if (nominalMhz > 0) {
        double ghz = static_cast<double>(nominalMhz) / 1000.0;
        std::wcout << L"Base Clock Speed: " << std::fixed << std::setprecision(2) << ghz << L" GHz\n";
    }

    std::wcout << L"\n[Topology]\n";
    std::wcout << L"  Physical Cores: " << physicalCores << L"\n";
    std::wcout << L"  Logical Threads:" << logicalThreads << L"\n";

    if (!cacheMap.empty()) {
        std::wcout << L"\n[Cache Info]\n";
        for (std::map<CacheKey, CacheVal>::const_iterator it = cacheMap.begin(); it != cacheMap.end(); ++it) {
            const CacheKey& key = it->first;
            const CacheVal& val = it->second;
            double sizeKB = static_cast<double>(val.sizeInBytes) / 1024.0;
            std::wstring typeStr = GetCacheTypeString(key.type);
            
            std::wcout << L"  L" << key.level << L" " 
                       << std::left << std::setw(12) << typeStr 
                       << L": " << sizeKB << L" KB";
            if (val.count > 1) {
                std::wcout << L" (x" << val.count << L")";
            }
            std::wcout << L"\n";
        }
    }
    std::wcout <<"";

    return 0;
}
