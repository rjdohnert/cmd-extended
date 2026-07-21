#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

struct HostnameOptions {
    bool short_name = false;
    bool domain_only = false;
    bool fqdn = false;
};

// Retrieve computer name formats using the native GetComputerNameExW API
std::wstring GetComputerNameStringW(COMPUTER_NAME_FORMAT format) {
    DWORD size = 0;
    // Initial call to determine the required buffer size
    GetComputerNameExW(format, nullptr, &size);
    if (size == 0) {
        return L"";
    }

    std::wstring buffer(size, L'\0');
    if (GetComputerNameExW(format, &buffer[0], &size)) {
        buffer.resize(size);
        return buffer;
    }
    return L"";
}

int wmain(int argc, wchar_t* argv[]) {
    HostnameOptions opts;
    std::wstring new_hostname = L"";
    bool set_name_triggered = false;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L's':
                        opts.short_name = true;
                        opts.domain_only = false;
                        opts.fqdn = false;
                        break;
                    case L'd':
                        opts.domain_only = true;
                        opts.short_name = false;
                        opts.fqdn = false;
                        break;
                    case L'f':
                        opts.fqdn = true;
                        opts.short_name = false;
                        opts.domain_only = false;
                        break;
                    default:
                        std::wcerr << L"hostname: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: hostname [-f] [-s | -d] [name-of-host]\n";
                        return 1;
                }
            }
        } else {
            if (set_name_triggered) {
                std::wcerr << L"hostname: too many arguments\n";
                std::wcerr << L"usage: hostname [-f] [-s | -d] [name-of-host]\n";
                return 1;
            }
            new_hostname = arg;
            set_name_triggered = true;
        }
    }

    // Set hostname logic (requires administrator privileges)
    if (set_name_triggered) {
        if (SetComputerNameExW(ComputerNamePhysicalDnsHostname, new_hostname.c_str())) {
            std::wcout << L"Hostname successfully changed to '" << new_hostname << L"'.\n";
            std::wcout << L"You must restart the computer for the changes to take effect.\n";
            return 0;
        } else {
            DWORD err = GetLastError();
            std::wcerr << L"hostname: failed to set hostname. Error code: " << err << L"\n";
            if (err == ERROR_ACCESS_DENIED) {
                std::wcerr << L"Error: Access Denied. Please run this command as Administrator.\n";
            }
            return 1;
        }
    }

    // Default to fully qualified domain name (-f) if no formatting flag is passed
    COMPUTER_NAME_FORMAT format = ComputerNameDnsFullyQualified;

    if (opts.short_name) {
        format = ComputerNameDnsHostname;
    } else if (opts.domain_only) {
        format = ComputerNameDnsDomain;
    } else if (opts.fqdn) {
        format = ComputerNameDnsFullyQualified;
    }

    std::wstring name = GetComputerNameStringW(format);
    
    // Note: ComputerNameDnsDomain can be empty if the computer is not domain-joined.
    // In that case, an empty line is printed to replicate standard Unix behavior.
    if (name.empty() && format != ComputerNameDnsDomain) {
        std::wcerr << L"hostname: failed to retrieve hostname.\n";
        return 1;
    }

    std::wcout << name << std::endl;
    return 0;
}