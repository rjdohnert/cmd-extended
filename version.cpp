#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "Advapi32.lib")

// Version: 2.0

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

// Helper to convert wide-character strings to UTF-8
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

// Dynamically queries the real OS version from ntdll.dll
bool GetRealOSVersion(RTL_OSVERSIONINFOW& ovi) {
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        auto pRtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hMod, "RtlGetVersion"));
        if (pRtlGetVersion) {
            ovi.dwOSVersionInfoSize = sizeof(ovi);
            return pRtlGetVersion(&ovi) == 0;
        }
    }
    return false;
}

// Helper to query string values from the Registry (copied from lscpu.cpp for simplicity)
std::wstring GetRegistryString(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(hKeyParent, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return L"";
    }

    DWORD valueType = 0;
    DWORD bufferSize = 0;
    std::wstring result;
    LONG status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &valueType, nullptr, &bufferSize);
    if (status == ERROR_SUCCESS && (valueType == REG_SZ || valueType == REG_EXPAND_SZ) && bufferSize >= sizeof(wchar_t)) {
        std::vector<wchar_t> buffer(bufferSize / sizeof(wchar_t));
        if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &bufferSize) == ERROR_SUCCESS) {
            result = buffer.data();
        }
    }

    RegCloseKey(hKey);
    return result;
}


// Queries the Update Build Revision (UBR) from the registry
DWORD GetUpdateBuildRevision() {
    HKEY hKey;
    DWORD ubr = 0;
    DWORD size = sizeof(DWORD);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"UBR", NULL, NULL, reinterpret_cast<LPBYTE>(&ubr), &size) != ERROR_SUCCESS) {
            ubr = 0;
        }
        RegCloseKey(hKey);
    }
    return ubr;
}

// Formats hostname
std::string GetNodeName() {
    DWORD size = 0;
    GetComputerNameExW(ComputerNameDnsHostname, NULL, &size);
    std::vector<wchar_t> buffer(size);
    if (GetComputerNameExW(ComputerNameDnsHostname, buffer.data(), &size)) {
        return WideToUtf8(std::wstring(buffer.data()));
    }
    return "localhost";
}

// Resolves system architecture mapping to Unix equivalents
std::string GetMachineArch(bool rawProcessor) {
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "x86_64";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "aarch64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "i686";
        case PROCESSOR_ARCHITECTURE_IA64:
            return "ia64";
        default:
            return rawProcessor ? "unknown" : "x86_64";
    }
}

void PrintHelp() {
    std::cout << "Usage: version [OPTION]...\n"
              << "Print certain system information. With no OPTION, same as -s.\n\n"
              << "  -a, --all                print all information, in the following order,\n"
              << "                             except omit -p and -i if they are unknown:\n"
              << "  -s, --sysname            print the kernel name\n"
              << "  -n, --nodename           print the network node hostname\n"
              << "  -r, --release            print the kernel release\n"
              << "  -v, --version            print the kernel version\n"
              << "  -m, --machine            print the machine hardware name\n"
              << "  -p, --processor          print the processor type (non-portable)\n"
              << "  -i, --hardware-platform  print the hardware platform (non-portable)\n"
              << "  -o, --operating-system   print the operating system\n"
              << "      --help     display this help and exit\n";
}

int main(int argc, char* argv[]) {
    // Flag variables
    bool opt_sysname = false;
    bool opt_nodename = false;
    bool opt_release = false;
    bool opt_version = false;
    bool opt_machine = false;
    bool opt_processor = false;
    bool opt_hardware = false;
    bool opt_os = false;

    bool has_option = false;

    // Standard Unix Command-Line Parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            PrintHelp();
            return 0;
        }

        if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
            // Grouped short options (e.g., -srvm)
            for (size_t j = 1; j < arg.length(); ++j) {
                has_option = true;
                switch (arg[j]) {
                    case 'a':
                        opt_sysname = opt_nodename = opt_release = opt_version = 
                        opt_machine = opt_processor = opt_hardware = opt_os = true;
                        break;
                    case 's': opt_sysname = true; break;
                    case 'n': opt_nodename = true; break;
                    case 'r': opt_release = true; break;
                    case 'v': opt_version = true; break;
                    case 'm': opt_machine = true; break;
                    case 'p': opt_processor = true; break;
                    case 'i': opt_hardware = true; break;
                    case 'o': opt_os = true; break;
                    default:
                        std::cerr << "version: invalid option -- '" << arg[j] << "'\n"
                                  << "Try 'version --help' for more information.\n";
                        return 1;
                }
            }
        } else if (arg.rfind("--", 0) == 0) {
            // Long options
            has_option = true;
            if (arg == "--all") {
                opt_sysname = opt_nodename = opt_release = opt_version = 
                opt_machine = opt_processor = opt_hardware = opt_os = true;
            } else if (arg == "--sysname") opt_sysname = true;
            else if (arg == "--nodename") opt_nodename = true;
            else if (arg == "--release") opt_release = true;
            else if (arg == "--version") opt_version = true;
            else if (arg == "--machine") opt_machine = true;
            else if (arg == "--processor") opt_processor = true;
            else if (arg == "--hardware-platform") opt_hardware = true;
            else if (arg == "--operating-system") opt_os = true;
            else {
                std::cerr << "version: unrecognized option '" << arg << "'\n"
                          << "Try 'version --help' for more information.\n";
                return 1;
            }
        } else {
            std::cerr << "version: extra operand '" << arg << "'\n"
                      << "Try 'version --help' for more information.\n";
            return 1;
        }
    }

    // Default to sysname if no flags are provided
    if (!has_option) {
        opt_sysname = true;
    }

    // Retrieve system information values
    RTL_OSVERSIONINFOW ovi = { 0 };
    std::string sysname = "Microsoft Windows";
    std::string nodename = GetNodeName();
    std::string release = "10.0.0";
    std::string version = "Build Unknown";
    std::wstring registryPath = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    std::string processor = WideToUtf8(GetRegistryString(HKEY_LOCAL_MACHINE, registryPath, L"ProcessorNameString"));
    std::string hardware = GetMachineArch(true); // Use raw arch for hardware platform
    std::string os = "Windows NT";

    if (GetRealOSVersion(ovi)) {
        std::stringstream ss_release;
        ss_release << ovi.dwMajorVersion << "." << ovi.dwMinorVersion << "." << ovi.dwBuildNumber;
        release = ss_release.str();

        DWORD ubr = GetUpdateBuildRevision();
        std::stringstream ss_version;
        ss_version << "Build " << ovi.dwBuildNumber << "." << ubr;
        version = ss_version.str();
    }

    // Output formatted components in POSIX defined order
    std::vector<std::string> output_parts;
    if (opt_sysname) output_parts.push_back(sysname);
    if (opt_nodename) output_parts.push_back(nodename);
    if (opt_release) output_parts.push_back(release);
    if (opt_version) output_parts.push_back(version);
    if (opt_processor) output_parts.push_back(processor);
    if (opt_hardware) output_parts.push_back(hardware);
    if (opt_os) output_parts.push_back(os);

    for (size_t i = 0; i < output_parts.size(); ++i) {
        std::cout << output_parts[i];
        if (i < output_parts.size() - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;

    return 0;
}