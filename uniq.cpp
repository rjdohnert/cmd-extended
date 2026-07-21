#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <sstream>
#include <iomanip>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

struct UniqOptions {
    bool count = false;          // -c
    bool duplicate_only = false; // -d
    bool unique_only = false;    // -u
    bool ignore_case = false;    // -i
    int skip_fields_num = 0;     // -f
    int skip_chars_num = 0;      // -s
};

#ifdef _WIN32
// Robust conversion of UTF-8/ANSI standard strings to wide strings on Windows
std::wstring to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (size_needed == 0) {
        size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
    }
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
#else
std::wstring to_wstring(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}
#endif

// Skip N fields (sequences of spaces/tabs separated by non-blank characters)
size_t skip_fields(const std::wstring& s, int num_fields) {
    size_t idx = 0;
    size_t len = s.length();
    for (int f = 0; f < num_fields; ++f) {
        // Skip leading blanks before the field
        while (idx < len && (s[idx] == L' ' || s[idx] == L'\t')) {
            idx++;
        }
        if (idx >= len) return len;
        // Skip non-blanks (the field characters)
        while (idx < len && s[idx] != L' ' && s[idx] != L'\t') {
            idx++;
        }
    }
    return idx;
}

// Skip C characters after any fields have been skipped
size_t skip_chars(const std::wstring& s, size_t start_idx, int num_chars) {
    size_t len = s.length();
    if (start_idx + num_chars > len) {
        return len;
    }
    return start_idx + num_chars;
}

// Generate the final comparison key after skips and case fold mappings
std::wstring get_comparison_key(const std::wstring& s, int num_fields, int num_chars, bool ignore_case) {
    size_t start = skip_fields(s, num_fields);
    start = skip_chars(s, start, num_chars);
    if (start >= s.length()) {
        return L"";
    }
    std::wstring key = s.substr(start);
    if (ignore_case) {
        for (auto& c : key) {
            c = std::towlower(c);
        }
    }
    return key;
}

// Format right-justified counts (7 wide) followed by a space
std::string format_count(int count) {
    std::ostringstream oss;
    oss << std::setw(7) << count << " ";
    return oss.str();
}

int wmain(int argc, wchar_t* argv[]) {
    // Optimize performance of standard streams
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    UniqOptions opts;
    std::vector<std::wstring> files;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg != L"-") {
            for (size_t j = 1; j < arg.size(); ++j) {
                wchar_t opt = arg[j];
                switch (opt) {
                    case L'c':
                        opts.count = true;
                        break;
                    case L'd':
                        opts.duplicate_only = true;
                        break;
                    case L'u':
                        opts.unique_only = true;
                        break;
                    case L'i':
                        opts.ignore_case = true;
                        break;
                    case L'f':
                    case L's': {
                        std::wstring val_str;
                        if (j + 1 < arg.size()) {
                            val_str = arg.substr(j + 1);
                            j = arg.size(); // Terminate parsing of this flag segment
                        } else {
                            if (i + 1 < argc) {
                                val_str = argv[++i];
                            } else {
                                std::wcerr << L"uniq: option requires an argument -- " << opt << std::endl;
                                return 1;
                            }
                        }

                        try {
                            int val = std::stoi(val_str);
                            if (val < 0) throw std::invalid_argument("negative");
                            if (opt == L'f') {
                                opts.skip_fields_num = val;
                            } else {
                                opts.skip_chars_num = val;
                            }
                        } catch (...) {
                            std::wcerr << L"uniq: invalid argument for -" << opt << L": " << val_str << std::endl;
                            return 1;
                        }
                        break;
                    }
                    default:
                        std::wcerr << L"uniq: unknown option -- " << opt << std::endl;
                        std::wcerr << L"usage: uniq [-c | -d | -u] [-i] [-f fields] [-s chars] [input_file [output_file]]\n";
                        return 1;
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    if (files.size() > 2) {
        std::wcerr << L"uniq: too many arguments\n";
        std::wcerr << L"usage: uniq [-c | -d | -u] [-i] [-f fields] [-s chars] [input_file [output_file]]\n";
        return 1;
    }

    std::istream* in_stream = &std::cin;
    std::ostream* out_stream = &std::cout;

    std::ifstream in_file_stream;
    std::ofstream out_file_stream;

    // Open target streams or fallback to standard ones
    if (files.size() >= 1) {
        if (files[0] != L"-") {
            in_file_stream.open(fs::path(files[0]));
            if (!in_file_stream) {
                std::wcerr << L"uniq: " << files[0] << L": No such file or directory\n";
                return 1;
            }
            in_stream = &in_file_stream;
        }
    }

    if (files.size() == 2) {
        if (files[1] != L"-") {
            out_file_stream.open(fs::path(files[1]));
            if (!out_file_stream) {
                std::wcerr << L"uniq: " << files[1] << L": Permission denied or failed to open\n";
                return 1;
            }
            out_stream = &out_file_stream;
        }
    }

    std::string current_raw;
    std::wstring current_key;
    int current_count = 0;

    auto process_group = [&](const std::string& raw, int count, std::ostream& out) {
        if (count == 0) return;

        bool should_print = false;
        if (opts.duplicate_only) {
            if (count > 1) {
                should_print = true;
            }
        } else if (opts.unique_only) {
            if (count == 1) {
                should_print = true;
            }
        } else {
            should_print = true;
        }

        if (should_print) {
            if (opts.count) {
                out << format_count(count) << raw << "\n";
            } else {
                out << raw << "\n";
            }
        }
    };

    std::string line;
    while (std::getline(*in_stream, line)) {
        std::wstring wline = to_wstring(line);
        std::wstring key = get_comparison_key(wline, opts.skip_fields_num, opts.skip_chars_num, opts.ignore_case);

        if (current_count == 0) {
            current_raw = line;
            current_key = key;
            current_count = 1;
        } else {
            if (key == current_key) {
                current_count++;
            } else {
                process_group(current_raw, current_count, *out_stream);
                current_raw = line;
                current_key = key;
                current_count = 1;
            }
        }
    }

    // Process the final group remaining in buffer
    if (current_count > 0) {
        process_group(current_raw, current_count, *out_stream);
    }

    return 0;
}