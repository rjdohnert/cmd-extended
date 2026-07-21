#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cwctype>
#include <limits>
#include <fstream>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

struct TailOptions {
    bool bytes = false;
    long long count = 10;        // Default: last 10 lines
    bool from_start = false;     // True if '+' prefix is used
    bool follow = false;         // -f
    bool reverse = false;        // -r
    bool count_specified = false;
};

struct FollowState {
    fs::path path;
    std::ifstream stream;
};

// Parses count arguments (handling '+' and '-' prefixes)
bool parse_count(const std::wstring& s, long long& count, bool& from_start) {
    if (s.empty()) return false;
    size_t start = 0;
    if (s[0] == L'+') {
        from_start = true;
        start = 1;
    } else if (s[0] == L'-') {
        from_start = false;
        start = 1;
    } else {
        from_start = false;
    }
    if (start >= s.length()) return false;
    try {
        count = std::stoll(s.substr(start));
        return true;
    } catch (...) {
        return false;
    }
}

// Emulates stream tailing behavior on non-seekable streams like stdin
void tail_unseekable(std::istream& in, const TailOptions& opts) {
    if (opts.bytes) {
        if (opts.from_start) {
            char ch;
            long long skipped = 0;
            while (skipped < opts.count - 1 && in.get(ch)) {
                skipped++;
            }
            while (in.get(ch)) {
                std::cout.put(ch);
            }
        } else {
            long long buffer_size = opts.count;
            std::vector<char> buffer(buffer_size);
            long long total_read = 0;
            char ch;
            while (in.get(ch)) {
                buffer[total_read % buffer_size] = ch;
                total_read++;
            }
            if (total_read > 0) {
                long long start = 0;
                long long len = total_read;
                if (total_read > buffer_size) {
                    start = total_read % buffer_size;
                    len = buffer_size;
                }
                for (long long i = 0; i < len; ++i) {
                    std::cout.put(buffer[(start + i) % buffer_size]);
                }
            }
        }
    } else {
        if (opts.from_start) {
            std::string line;
            long long lines_skipped = 0;
            while (lines_skipped < opts.count - 1 && std::getline(in, line)) {
                lines_skipped++;
            }
            while (std::getline(in, line)) {
                std::cout << line << "\n";
            }
        } else {
            std::vector<std::string> buffer;
            std::string line;
            while (std::getline(in, line)) {
                if (buffer.size() >= static_cast<size_t>(opts.count)) {
                    buffer.erase(buffer.begin());
                }
                buffer.push_back(line);
            }
            if (opts.reverse) {
                std::reverse(buffer.begin(), buffer.end());
            }
            for (const auto& l : buffer) {
                std::cout << l << "\n";
            }
        }
    }
}

// Direct tail processing for seekable disk files
void tail_file(const fs::path& path, const TailOptions& opts, long long& final_pos) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::wcerr << L"tail: " << path.wstring() << L": No such file or directory\n";
        return;
    }

    file.seekg(0, std::ios::end);
    long long file_size = file.tellg();
    final_pos = file_size;

    if (opts.bytes) {
        long long start = 0;
        if (opts.from_start) {
            start = opts.count - 1;
            if (start < 0) start = 0;
        } else {
            start = file_size - opts.count;
            if (start < 0) start = 0;
        }

        if (start < file_size) {
            file.seekg(start, std::ios::beg);
            char ch;
            while (file.get(ch)) {
                std::cout.put(ch);
            }
        }
    } else {
        long long pos = file_size;
        if (opts.from_start) {
            file.seekg(0, std::ios::beg);
            std::string line;
            long long lines_skipped = 0;
            while (lines_skipped < opts.count - 1 && std::getline(file, line)) {
                lines_skipped++;
            }
            while (std::getline(file, line)) {
                std::cout << line << "\n";
            }
        } else {
            long long lines_found = 0;
            long long target_lines = opts.count;
            const int block_size = 4096;
            std::vector<char> buffer(block_size);

            while (pos > 0 && lines_found < target_lines) {
                long long read_size = std::min(static_cast<long long>(block_size), pos);
                pos -= read_size;
                file.seekg(pos, std::ios::beg);
                file.read(buffer.data(), read_size);

                for (long long i = read_size - 1; i >= 0; --i) {
                    char ch = buffer[i];
                    if (pos + i == file_size - 1) {
                        if (ch == '\n') {
                            continue; // Ignore last trailing newline for accurate line boundaries
                        }
                    }
                    if (ch == '\n') {
                        lines_found++;
                        if (lines_found == target_lines) {
                            pos = pos + i + 1; // Align read-head right after the matched newline
                            break;
                        }
                    }
                }
            }
            if (lines_found < target_lines) {
                pos = 0;
            }

            file.clear();
            file.seekg(pos, std::ios::beg);

            if (opts.reverse) {
                std::vector<std::string> lines;
                std::string line;
                while (std::getline(file, line)) {
                    lines.push_back(line);
                }
                std::reverse(lines.begin(), lines.end());
                for (const auto& l : lines) {
                    std::cout << l << "\n";
                }
            } else {
                char ch;
                while (file.get(ch)) {
                    std::cout.put(ch);
                }
            }
        }
    }
}

// Multi-file polling loop to handle log follows (-f)
void follow_multiple_files(const std::vector<std::wstring>& files) {
    std::vector<FollowState> states;
    for (const auto& f_str : files) {
        FollowState state;
        state.path = f_str;
        state.stream.open(state.path, std::ios::binary);
        if (state.stream) {
            state.stream.seekg(0, std::ios::end);
            states.push_back(std::move(state));
        }
    }

    if (states.empty()) return;

    const FollowState* active_file = nullptr;

    while (true) {
        bool data_read_this_cycle = false;
        for (auto& s : states) {
            char ch;
            bool printed_header_for_file = false;
            s.stream.clear();

            // Handle rotations or file truncations
            std::error_code ec;
            auto current_size = fs::file_size(s.path, ec);
            if (!ec) {
                auto current_pos = s.stream.tellg();
                if (current_size < current_pos) {
                    s.stream.seekg(0, std::ios::beg);
                }
            }

            while (s.stream.get(ch)) {
                if (!printed_header_for_file && active_file != &s) {
                    if (active_file != nullptr) {
                        std::cout << "\n";
                    }
                    std::cout << "==> " << s.path.string() << " <==\n";
                    std::cout.flush();
                    printed_header_for_file = true;
                    active_file = &s;
                }
                std::cout.put(ch);
                data_read_this_cycle = true;
            }
            if (data_read_this_cycle) {
                std::cout.flush();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

int wmain(int argc, wchar_t* argv[]) {
    // Speed optimization for stdout/stdin streams
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    TailOptions opts;
    std::vector<std::wstring> files;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-f") {
            opts.follow = true;
        } else if (arg == L"-r") {
            opts.reverse = true;
        } else if (arg == L"-n" || arg == L"-c") {
            bool is_bytes = (arg == L"-c");
            if (i + 1 < argc) {
                std::wstring val = argv[++i];
                long long count = 0;
                bool from_start = false;
                if (parse_count(val, count, from_start)) {
                    opts.bytes = is_bytes;
                    opts.count = count;
                    opts.from_start = from_start;
                    opts.count_specified = true;
                } else {
                    std::wcerr << L"tail: illegal offset -- " << val << std::endl;
                    return 1;
                }
            } else {
                std::wcerr << L"tail: option requires an argument -- " << arg.substr(1) << std::endl;
                return 1;
            }
        } else if (arg[0] == L'-' && arg.length() > 1 && std::iswdigit(arg[1])) {
            long long count = 0;
            bool from_start = false;
            if (parse_count(arg, count, from_start)) {
                opts.count = count;
                opts.from_start = from_start;
                opts.bytes = false;
                opts.count_specified = true;
            }
        } else if (arg[0] == L'+' && arg.length() > 1 && std::iswdigit(arg[1])) {
            long long count = 0;
            bool from_start = false;
            if (parse_count(arg, count, from_start)) {
                opts.count = count;
                opts.from_start = from_start;
                opts.bytes = false;
                opts.count_specified = true;
            }
        } else {
            files.push_back(arg);
        }
    }

    // Default configuration adjustments
    if (opts.reverse && !opts.count_specified) {
        opts.count = std::numeric_limits<long long>::max();
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if (files.empty()) {
        tail_unseekable(std::cin, opts);
    } else {
        bool first_file = true;
        for (const auto& file_str : files) {
            if (!first_file) {
                std::cout << "\n";
            }
            first_file = false;

            if (files.size() > 1) {
                std::cout << "==> " << fs::path(file_str).string() << " <==\n";
                std::cout.flush();
            }

            long long final_pos = 0;
            tail_file(file_str, opts, final_pos);
            std::cout.flush();
        }

        if (opts.follow) {
            follow_multiple_files(files);
        }
    }

    return 0;
}