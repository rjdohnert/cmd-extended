#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

// Helper function to safely escape command-line arguments that contain spaces
std::wstring escape_arg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    // If the argument has no spaces, tabs, or quotes, it does not need to be escaped
    if (arg.find(L' ') == std::wstring::npos &&
        arg.find(L'\t') == std::wstring::npos &&
        arg.find(L'"') == std::wstring::npos) {
        return arg;
    }
    std::wstring escaped = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') {
            escaped += L"\\\"";
        } else {
            escaped += c;
        }
    }
    escaped += L"\"";
    return escaped;
}

void print_help() {
    std::wcout << L"pkg: A BSD-style pkg wrapper for Windows winget\n\n";
    std::wcout << L"Usage:\n";
    std::wcout << L"  pkg install <package>  - Install a package (winget install)\n";
    std::wcout << L"  pkg delete <package>   - Uninstall a package (winget uninstall)\n";
    std::wcout << L"  pkg remove <package>   - Alias for delete\n";
    std::wcout << L"  pkg search <query>     - Search for a package (winget search)\n";
    std::wcout << L"  pkg upgrade            - Upgrade all packages (winget upgrade --all)\n";
    std::wcout << L"  pkg upgrade <package>  - Upgrade a specific package (winget upgrade)\n";
    std::wcout << L"  pkg info               - List installed packages (winget list)\n";
    std::wcout << L"  pkg info <package>     - Show details of a package (winget show)\n";
    std::wcout << L"  pkg update             - Update package sources (winget source update)\n";
    std::wcout << L"  pkg help               - Show this help message\n";
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::wstring command = argv[1];
    std::vector<std::wstring> args;
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    std::wstring winget_cmd = L"winget";

    // Map pkg commands to winget commands
    if (command == L"install") {
        winget_cmd += L" install";
    } else if (command == L"delete" || command == L"remove") {
        winget_cmd += L" uninstall";
    } else if (command == L"search") {
        winget_cmd += L" search";
    } else if (command == L"upgrade") {
        if (args.empty()) {
            winget_cmd += L" upgrade --all";
        } else {
            winget_cmd += L" upgrade";
        }
    } else if (command == L"info") {
        if (args.empty()) {
            winget_cmd += L" list";
        } else {
            winget_cmd += L" show";
        }
    } else if (command == L"update") {
        winget_cmd += L" source update";
    } else if (command == L"help" || command == L"-h" || command == L"--help") {
        print_help();
        return 0;
    } else {
        std::wcerr << L"Unknown command: " << command << L"\n\n";
        print_help();
        return 1;
    }

    // Append remaining arguments to the constructed command
    for (const auto& arg : args) {
        winget_cmd += L" " + escape_arg(arg);
    }

    // Execute the command using the Windows wide-character system call
    int result = _wsystem(winget_cmd.c_str());
    return result;
}