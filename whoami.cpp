#include <windows.h>
#include <lmcons.h> // Required for UNLEN (username max length)
#include <iostream>
#include <string>

#pragma comment(lib, "advapi32.lib")

// Helper to convert UTF-16 wide strings to UTF-8 narrow strings
std::string WideToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void PrintHelp() {
    std::cout << "Usage: whoami [OPTION]\n"
              << "Print the user name associated with the current effective user ID.\n\n"
              << "  -h, --help     display this help message and exit\n"
              << "  -V, --version  output version information and exit\n";
}

void PrintVersion() {
    std::cout << "whoami (whoami) 1.0\n"
              << "whoami utility for Windows.\n";
}

int main(int argc, char* argv[]) {
    // Process command line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            PrintVersion();
            return 0;
        } else {
            // BSD/GNU whoami rejects extra operands
            std::cerr << "whoami: extra operand '" << arg << "'\n"
                      << "Try 'whoami --help' for more information.\n";
            return 1;
        }
    }

    // Retrieve the username (up to UNLEN, which is 256 characters on Windows)
    wchar_t username[UNLEN + 1];
    DWORD size = UNLEN + 1;

    if (GetUserNameW(username, &size)) {
        std::wstring w_user(username);
        std::cout << WideToUTF8(w_user) << "\n";
        return 0;
    } else {
        std::cerr << "whoami: failed to get effective user name (Error code: " << GetLastError() << ")\n";
        return 1;
    }
}