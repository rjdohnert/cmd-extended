#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

// Parses a single sleep argument (e.g., "1.5s", "10m", "2h", "1d", or "5")
// and returns the duration in milliseconds.
bool parse_sleep_argument(const std::wstring& arg, long long& total_ms) {
    if (arg.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    // Extract the numeric portion (supports floats like 1.5)
    double value = std::wcstod(arg.c_str(), &end_ptr);

    if (value < 0) {
        std::wcerr << L"sleep: bad delay value -- '" << arg << L"'\n";
        return false;
    }

    if (end_ptr == arg.c_str()) {
        // No characters were parsed as a number
        std::wcerr << L"sleep: bad delay value -- '" << arg << L"'\n";
        return false;
    }

    // Determine the unit suffix (if any)
    double multiplier = 1000.0; // Default is seconds -> milliseconds
    if (*end_ptr != L'\0') {
        std::wstring suffix(end_ptr);
        // Convert suffix to lowercase for uniform comparison
        for (auto& c : suffix) c = static_cast<wchar_t>(std::towlower(c));

        if (suffix == L"s" || suffix == L"sec" || suffix == L"secs") {
            multiplier = 1000.0;
        } else if (suffix == L"m" || suffix == L"min" || suffix == L"mins") {
            multiplier = 60.0 * 1000.0;
        } else if (suffix == L"h" || suffix == L"hour" || suffix == L"hours") {
            multiplier = 60.0 * 60.0 * 1000.0;
        } else if (suffix == L"d" || suffix == L"day" || suffix == L"days") {
            multiplier = 24.0 * 60.0 * 60.0 * 1000.0;
        } else {
            std::wcerr << L"sleep: unknown time unit -- '" << suffix << L"'\n";
            return false;
        }
    }

    total_ms += static_cast<long long>(value * multiplier);
    return true;
}

int wmain(int argc, wchar_t* argv[]) {
    // Check for standard help flags
    if (argc > 1) {
        std::wstring first_arg = argv[1];
        if (first_arg == L"--help" || first_arg == L"-h") {
            std::wcout << L"usage: sleep time [time ...]\n";
            std::wcout << L"       (time is a non-negative number optionally followed by a unit: s, m, h, d)\n";
            return 0;
        }
    }

    if (argc < 2) {
        std::wcerr << L"usage: sleep time [time ...]\n";
        return 1;
    }

    long long total_sleep_ms = 0;

    // Sum all requested delays together
    for (int i = 1; i < argc; ++i) {
        if (!parse_sleep_argument(argv[i], total_sleep_ms)) {
            return 1;
        }
    }

    if (total_sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(total_sleep_ms));
    }

    return 0;
}