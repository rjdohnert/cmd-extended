#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cwctype>
#include <windows.h>

struct TouchOptions {
    bool change_access = false;     // -a
    bool no_create = false;         // -c
    bool change_mod = false;        // -m
    std::wstring ref_file = L"";    // -r
    std::wstring time_str = L"";    // -t
};

// Helper to verify a string contains only digits
bool is_digits(const std::wstring& s) {
    return std::all_of(s.begin(), s.end(), [](wchar_t c) { return std::iswdigit(c); });
}

// Parses [[CC]YY]MMDDhhmm[.SS] and populates a UTC FILETIME
bool parse_t_time(const std::wstring& t_str, FILETIME& out_ft) {
    std::wstring clean_str = t_str;
    std::wstring ss_str = L"00";

    size_t dot_pos = t_str.find(L'.');
    if (dot_pos != std::wstring::npos) {
        ss_str = t_str.substr(dot_pos + 1);
        clean_str = t_str.substr(0, dot_pos);
    }

    if (ss_str.length() != 2 || !is_digits(ss_str) || !is_digits(clean_str)) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = std::stoi(ss_str);

    // Get current local time to extract defaults
    SYSTEMTIME current_st;
    GetLocalTime(&current_st);

    size_t len = clean_str.length();
    if (len == 8) { // MMDDhhmm
        year = current_st.wYear;
        month = std::stoi(clean_str.substr(0, 2));
        day = std::stoi(clean_str.substr(2, 2));
        hour = std::stoi(clean_str.substr(4, 2));
        minute = std::stoi(clean_str.substr(6, 2));
    } else if (len == 10) { // YYMMDDhhmm
        int yy = std::stoi(clean_str.substr(0, 2));
        year = (yy < 69) ? (2000 + yy) : (1900 + yy); // POSIX standard pivot
        month = std::stoi(clean_str.substr(2, 2));
        day = std::stoi(clean_str.substr(4, 2));
        hour = std::stoi(clean_str.substr(6, 2));
        minute = std::stoi(clean_str.substr(8, 2));
    } else if (len == 12) { // CCYYMMDDhhmm
        year = std::stoi(clean_str.substr(0, 4));
        month = std::stoi(clean_str.substr(4, 2));
        day = std::stoi(clean_str.substr(6, 2));
        hour = std::stoi(clean_str.substr(8, 2));
        minute = std::stoi(clean_str.substr(10, 2));
    } else {
        return false;
    }

    SYSTEMTIME local_st = { 0 };
    local_st.wYear = static_cast<WORD>(year);
    local_st.wMonth = static_cast<WORD>(month);
    local_st.wDay = static_cast<WORD>(day);
    local_st.wHour = static_cast<WORD>(hour);
    local_st.wMinute = static_cast<WORD>(minute);
    local_st.wSecond = static_cast<WORD>(second);

    // Translate local input system time to UTC system time
    SYSTEMTIME utc_st;
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &local_st, &utc_st)) {
        return false;
    }

    return SystemTimeToFileTime(&utc_st, &out_ft) != 0;
}

// Extract access/write times from a reference file
bool get_reference_times(const std::wstring& ref_path, FILETIME& out_at, FILETIME& out_mt) {
    HANDLE hFile = CreateFileW(
        ref_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    FILETIME ct;
    bool success = GetFileTime(hFile, &ct, &out_at, &out_mt) != 0;
    CloseHandle(hFile);
    return success;
}

int wmain(int argc, wchar_t* argv[]) {
    TouchOptions opts;
    std::vector<std::wstring> targets;

    // Parse options block
    int i = 1;
    while (i < argc) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                wchar_t flag = arg[j];
                if (flag == L'a') {
                    opts.change_access = true;
                } else if (flag == L'c') {
                    opts.no_create = true;
                } else if (flag == L'm') {
                    opts.change_mod = true;
                } else if (flag == L'r') {
                    if (j + 1 == arg.length()) {
                        if (i + 1 < argc) {
                            opts.ref_file = argv[++i];
                            break;
                        } else {
                            std::wcerr << L"touch: option requires an argument -- r\n";
                            return 1;
                        }
                    } else {
                        opts.ref_file = arg.substr(j + 1);
                        break;
                    }
                } else if (flag == L't') {
                    if (j + 1 == arg.length()) {
                        if (i + 1 < argc) {
                            opts.time_str = argv[++i];
                            break;
                        } else {
                            std::wcerr << L"touch: option requires an argument -- t\n";
                            return 1;
                        }
                    } else {
                        opts.time_str = arg.substr(j + 1);
                        break;
                    }
                } else {
                    std::wcerr << L"touch: unknown option -- " << flag << std::endl;
                    std::wcerr << L"usage: touch [-acm] [-r file] [-t time] file ...\n";
                    return 1;
                }
            }
        } else {
            targets.push_back(arg);
        }
        i++;
    }

    if (targets.empty()) {
        std::wcerr << L"usage: touch [-acm] [-r file] [-t time] file ...\n";
        return 1;
    }

    // Resolve time context
    FILETIME target_at, target_mt;
    if (!opts.ref_file.empty()) {
        if (!get_reference_times(opts.ref_file, target_at, target_mt)) {
            std::wcerr << L"touch: " << opts.ref_file << L": reference file could not be read\n";
            return 1;
        }
    } else if (!opts.time_str.empty()) {
        if (!parse_t_time(opts.time_str, target_at)) {
            std::wcerr << L"touch: out of range or bad time specification: " << opts.time_str << std::endl;
            return 1;
        }
        target_mt = target_at;
    } else {
        // Fallback to active system time
        SYSTEMTIME st;
        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &target_at);
        target_mt = target_at;
    }

    bool overall_success = true;
    for (const auto& target : targets) {
        DWORD creation_disposition = opts.no_create ? OPEN_EXISTING : OPEN_ALWAYS;
        HANDLE hFile = CreateFileW(
            target.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            creation_disposition,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            // Suppress error reporting if the no-create flag is passed and file does not exist
            if (opts.no_create && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) {
                continue;
            }
            std::wcerr << L"touch: " << target << L": cannot touch. Error: " << err << std::endl;
            overall_success = false;
            continue;
        }

        FILETIME* p_at = nullptr;
        FILETIME* p_mt = nullptr;

        // Apply access/modification filters
        if (opts.change_access && !opts.change_mod) {
            p_at = &target_at;
        } else if (opts.change_mod && !opts.change_access) {
            p_mt = &target_mt;
        } else {
            p_at = &target_at;
            p_mt = &target_mt;
        }

        if (!SetFileTime(hFile, nullptr, p_at, p_mt)) {
            std::wcerr << L"touch: " << target << L": setting time failed. Error: " << GetLastError() << std::endl;
            overall_success = false;
        }

        CloseHandle(hFile);
    }

    return overall_success ? 0 : 1;
}