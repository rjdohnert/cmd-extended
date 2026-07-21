#include <iostream>
#include <string>
#include <algorithm>
#include <windows.h>

// Helper to check if the program is running under the name "machine"
bool IsRunningAsMachine(std::string argv0) {
    // Strip file path
    size_t last_slash = argv0.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        argv0 = argv0.substr(last_slash + 1);
    }
    // Strip file extension
    size_t last_dot = argv0.find_last_of('.');
    if (last_dot != std::string::npos) {
        argv0 = argv0.substr(0, last_dot);
    }
    // Case-insensitive comparison
    std::transform(argv0.begin(), argv0.end(), argv0.begin(), ::tolower);
    return argv0 == "machine";
}

// Maps Windows processor architecture macros to Unix/BSD architecture names
std::string GetArchString(WORD archVal) {
    switch (archVal) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "amd64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "i386";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "arm64";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "arm";
        case PROCESSOR_ARCHITECTURE_IA64:
            return "ia64";
        default:
            return "unknown";
    }
}

void PrintHelpArch() {
    std::cout << "Usage: arch [-ks]\n"
              << "Display the system's architecture.\n\n"
              << "Options:\n"
              << "  -k    Display the kernel architecture instead of application architecture.\n"
              << "  -s    Display the chosen architecture in short form (without the operating system prefix).\n"
              << "  -h    Display this help message.\n";
}

void PrintHelpMachine() {
    std::cout << "Usage: machine [-a]\n"
              << "Display the system's machine hardware / kernel architecture.\n\n"
              << "Options:\n"
              << "  -a    Display the application architecture instead of the kernel architecture.\n"
              << "  -h    Display this help message.\n";
}

int main(int argc, char* argv[]) {
    bool as_machine = false;
    if (argc > 0 && argv[0] != nullptr) {
        as_machine = IsRunningAsMachine(argv[0]);
    }

    bool opt_kernel = false;
    bool opt_short = false;
    bool opt_app = false;
    bool opt_help = false;

    // Parse options (supports grouped flags, e.g. -ks)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            opt_help = true;
        } else if (arg.front() == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                if (as_machine) {
                    if (c == 'a') opt_app = true;
                    else if (c == 'h') opt_help = true;
                    else {
                        std::cerr << "machine: unknown option -- " << c << "\n";
                        PrintHelpMachine();
                        return 1;
                    }
                } else {
                    if (c == 'k') opt_kernel = true;
                    else if (c == 's') opt_short = true;
                    else if (c == 'h') opt_help = true;
                    else {
                        std::cerr << "arch: unknown option -- " << c << "\n";
                        PrintHelpArch();
                        return 1;
                    }
                }
            }
        } else {
            std::cerr << (as_machine ? "machine" : "arch") << ": extra operand '" << arg << "'\n";
            if (as_machine) PrintHelpMachine();
            else PrintHelpArch();
            return 1;
        }
    }

    if (opt_help) {
        if (as_machine) PrintHelpMachine();
        else PrintHelpArch();
        return 0;
    }

    // Retrieve active and native system metrics
    SYSTEM_INFO sys_info;
    SYSTEM_INFO native_sys_info;
    GetSystemInfo(&sys_info);
    GetNativeSystemInfo(&native_sys_info);

    if (as_machine) {
        // "machine" command logic
        WORD target_arch = native_sys_info.wProcessorArchitecture;
        if (opt_app) {
            target_arch = sys_info.wProcessorArchitecture;
        }
        std::cout << GetArchString(target_arch) << "\n";
    } else {
        // "arch" command logic
        WORD target_arch = sys_info.wProcessorArchitecture;
        bool use_kernel = opt_kernel;
        if (use_kernel) {
            target_arch = native_sys_info.wProcessorArchitecture;
        }
        
        std::string arch_str = GetArchString(target_arch);
        
        // Output format
        if (opt_short || use_kernel) {
            std::cout << arch_str << "\n";
        } else {
            std::cout << "windows." << arch_str << "\n";
        }
    }

    return 0;
}