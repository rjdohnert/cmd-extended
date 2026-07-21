#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <fcntl.h>
#include <io.h>

// Retrieves the value of a specific environment variable
std::wstring get_env_var(const std::wstring& name, bool& exists) {
    exists = false;
    DWORD buffer_size = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (buffer_size == 0) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            exists = false;
            return L"";
        }
        exists = true; // Exists but is completely empty
        return L"";
    }

    std::wstring val(buffer_size, L'\0');
    DWORD copied = GetEnvironmentVariableW(name.c_str(), &val[0], buffer_size);
    if (copied == 0) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            exists = false;
            return L"";
        }
    }
    
    // Fallback: in the highly unlikely event the variable value grew between calls
    if (copied >= buffer_size) {
        val.resize(copied);
        copied = GetEnvironmentVariableW(name.c_str(), &val[0], copied);
    }

    exists = true;
    if (copied > 0) {
        val.resize(copied);
    } else {
        val.clear();
    }
    return val;
}

void print_help() {
    std::wcout << L"Usage: printenv [OPTION]... [VARIABLE]...\n";
    std::wcout << L"Print the values of the specified environment VARIABLE(s).\n";
    std::wcout << L"If no VARIABLE is specified, print name and value pairs for them all.\n\n";
    std::wcout << L"  -0, --null     end each output line with NUL, not newline\n";
    std::wcout << L"  -h, --help     display this help and exit\n";
    std::wcout << L"  -v, --version  output version information and exit\n";
}

void print_version() {
    std::wcout << L"printenv clone for Windows 1.0\n";
}

int wmain(int argc, wchar_t* argv[]) {
    // Set standard output and error streams to UTF-8 translation mode
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    bool null_terminated = false;
    std::vector<std::wstring> targets;
    bool parse_options = true;

    // Command-line parsing
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        
        if (parse_options && arg == L"--") {
            parse_options = false;
            continue;
        }

        if (parse_options && (arg == L"-0" || arg == L"--null")) {
            null_terminated = true;
        } else if (parse_options && (arg == L"-h" || arg == L"--help")) {
            print_help();
            return 0;
        } else if (parse_options && (arg == L"-v" || arg == L"--version")) {
            print_version();
            return 0;
        } else if (parse_options && arg[0] == L'-' && arg.size() > 1) {
            std::wcerr << L"printenv: unrecognized option: " << arg << L"\n";
            print_help();
            return 1;
        } else {
            targets.push_back(arg);
        }
    }

    // Case 1: No specific environment variables requested (print all)
    if (targets.empty()) {
        LPWCH env_block = GetEnvironmentStringsW();
        if (env_block == nullptr) {
            std::wcerr << L"printenv: failed to retrieve environment block\n";
            return 1;
        }

        LPWCH current = env_block;
        while (*current != L'\0') {
            std::wstring env_str(current);

            // Skip hidden Windows command variables (typically starting with '=')
            if (!env_str.empty() && env_str[0] != L'=') {
                std::wcout << env_str;
                if (null_terminated) {
                    std::wcout.put(L'\0');
                } else {
                    std::wcout.put(L'\n');
                }
            }
            current += env_str.length() + 1;
        }

        FreeEnvironmentStringsW(env_block);
        std::wcout.flush();
        return 0;
    }

    // Case 2: Specific environment variables requested (print values only)
    bool all_found = true;
    for (const auto& target : targets) {
        bool exists = false;
        std::wstring val = get_env_var(target, exists);
        if (exists) {
            std::wcout << val;
            if (null_terminated) {
                std::wcout.put(L'\0');
            } else {
                std::wcout.put(L'\n');
            }
        } else {
            all_found = false;
        }
    }

    std::wcout.flush();
    return all_found ? 0 : 1;
}