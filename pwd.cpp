#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

struct PwdOptions {
    bool physical = false;
};

// Removes extended path prefixes (e.g., \\?\ and \\?\UNC\) added by Win32 final-path APIs
std::wstring strip_extended_prefix(const std::wstring& path) {
    if (path.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        return L"\\\\" + path.substr(8);
    } else if (path.rfind(L"\\\\?\\", 0) == 0) {
        return path.substr(4);
    }
    return path;
}

// Retrieves the logical current working directory
std::wstring get_logical_cwd() {
    DWORD len = GetCurrentDirectoryW(0, nullptr);
    if (len == 0) return L"";
    std::wstring buffer(len, L'\0');
    DWORD written = GetCurrentDirectoryW(len, &buffer[0]);
    if (written > 0 && written < len) {
        buffer.resize(written);
        return buffer;
    }
    return L"";
}

// Resolves physical targets (dereferencing symbolic links or NTFS junctions)
std::wstring get_physical_cwd(const std::wstring& logical_path) {
    HANDLE hDir = CreateFileW(
        logical_path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        return logical_path; // Fallback to logical path if directory cannot be opened
    }

    DWORD len = GetFinalPathNameByHandleW(hDir, nullptr, 0, FILE_NAME_NORMALIZED);
    if (len == 0) {
        CloseHandle(hDir);
        return logical_path;
    }

    std::wstring buffer(len, L'\0');
    DWORD written = GetFinalPathNameByHandleW(hDir, &buffer[0], len, FILE_NAME_NORMALIZED);
    CloseHandle(hDir);

    if (written > 0 && written < len) {
        buffer.resize(written);
        return strip_extended_prefix(buffer);
    }

    return logical_path;
}

int wmain(int argc, wchar_t* argv[]) {
    PwdOptions opts;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'L':
                        opts.physical = false;
                        break;
                    case L'P':
                        opts.physical = true;
                        break;
                    default:
                        std::wcerr << L"pwd: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: pwd [-L | -P]\n";
                        return 1;
                }
            }
        } else {
            std::wcerr << L"pwd: too many arguments\n";
            std::wcerr << L"usage: pwd [-L | -P]\n";
            return 1;
        }
    }

    std::wstring logical = get_logical_cwd();
    if (logical.empty()) {
        std::wcerr << L"pwd: failed to get current directory\n";
        return 1;
    }

    if (opts.physical) {
        std::wstring physical = get_physical_cwd(logical);
        std::wcout << physical << std::endl;
    } else {
        std::wcout << logical << std::endl;
    }

    return 0;
}