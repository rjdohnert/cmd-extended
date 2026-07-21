#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <fcntl.h>
#include <io.h>

enum StatusLevel { STATUS_DEFAULT, STATUS_NONE, STATUS_NOXFER, STATUS_PROGRESS };

struct Options {
    std::string if_path;
    std::string of_path;
    uint64_t ibs = 512;
    uint64_t obs = 512;
    uint64_t count = UINT64_MAX;
    uint64_t skip = 0;
    uint64_t seek = 0;
    StatusLevel status = STATUS_DEFAULT;

    bool conv_ucase = false;
    bool conv_lcase = false;
    bool conv_swab = false;
    bool conv_sync = false;
    bool conv_noerror = false;
    bool conv_notrunc = false;
    bool conv_excl = false;
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPERAND]...\n"
              << "Copy a file, converting and formatting according to the operands.\n\n"
              << "Operands:\n"
              << "  if=FILE        read from FILE instead of stdin\n"
              << "  of=FILE        write to FILE instead of stdout\n"
              << "  bs=BYTES       read and write up to BYTES bytes at a time (default 512)\n"
              << "  ibs=BYTES      read up to BYTES bytes at a time\n"
              << "  obs=BYTES      write BYTES bytes at a time\n"
              << "  count=N        copy only N input blocks\n"
              << "  skip=N         skip N ibs-sized blocks at start of input\n"
              << "  seek=N         skip N obs-sized blocks at start of output\n"
              << "  status=LEVEL   LEVEL: 'none', 'noxfer', or 'progress'\n"
              << "  conv=CONVS     comma-separated conversions:\n"
              << "                 ucase, lcase, swab, sync, noerror, notrunc, excl\n"
              << "  -h, --help     display this help and exit\n\n"
              << "BYTES may be followed by multiplier suffixes: b=512, k=1024, M=1024^2, G=1024^3.\n";
}

// Parses sizes like 512, 1k, 1M, 1G, 10b, or expressions like 512x1024
uint64_t parse_size(const std::string& str) {
    if (str.empty()) return 0;

    size_t x_pos = str.find_first_of("x*");
    if (x_pos != std::string::npos) {
        std::string left = str.substr(0, x_pos);
        std::string right = str.substr(x_pos + 1);
        return parse_size(left) * parse_size(right);
    }

    uint64_t mult = 1;
    std::string num_str = str;

    if (!num_str.empty()) {
        char last = num_str.back();
        if (last == 'b' || last == 'B') {
            if (num_str.length() > 1) {
                char prev = num_str[num_str.length() - 2];
                if (prev == 'k' || prev == 'K') { mult = 1024; num_str.pop_back(); num_str.pop_back(); }
                else if (prev == 'm' || prev == 'M') { mult = 1024ULL * 1024; num_str.pop_back(); num_str.pop_back(); }
                else if (prev == 'g' || prev == 'G') { mult = 1024ULL * 1024 * 1024; num_str.pop_back(); num_str.pop_back(); }
                else { mult = 512; num_str.pop_back(); }
            } else {
                mult = 512; num_str.pop_back();
            }
        } else if (last == 'k' || last == 'K') {
            mult = 1024; num_str.pop_back();
        } else if (last == 'm' || last == 'M') {
            mult = 1024ULL * 1024; num_str.pop_back();
        } else if (last == 'g' || last == 'G') {
            mult = 1024ULL * 1024 * 1024; num_str.pop_back();
        } else if (last == 'w' || last == 'W') {
            mult = 2; num_str.pop_back();
        } else if (last == 'c' || last == 'C') {
            mult = 1; num_str.pop_back();
        }
    }

    return std::stoull(num_str) * mult;
}

void parse_conv(const std::string& str, Options& opt) {
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item == "ucase") opt.conv_ucase = true;
        else if (item == "lcase") opt.conv_lcase = true;
        else if (item == "swab") opt.conv_swab = true;
        else if (item == "sync") opt.conv_sync = true;
        else if (item == "noerror") opt.conv_noerror = true;
        else if (item == "notrunc") opt.conv_notrunc = true;
        else if (item == "excl") opt.conv_excl = true;
        else {
            std::cerr << "dd: invalid conversion: '" << item << "'\n";
            exit(1);
        }
    }
}

std::string format_bytes(uint64_t bytes) {
    std::ostringstream ss;
    if (bytes >= 1024ULL * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (double)bytes / (1024 * 1024 * 1024) << " GB";
    } else if (bytes >= 1024ULL * 1024) {
        ss << std::fixed << std::setprecision(2) << (double)bytes / (1024 * 1024) << " MB";
    } else if (bytes >= 1024ULL) {
        ss << std::fixed << std::setprecision(2) << (double)bytes / 1024 << " kB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

void print_stats(uint64_t rec_in_f, uint64_t rec_in_p,
                 uint64_t rec_out_f, uint64_t rec_out_p,
                 uint64_t total_bytes, double elapsed_sec,
                 StatusLevel status, bool is_progress = false) {
    if (status == STATUS_NONE) return;

    if (!is_progress && status != STATUS_NOXFER) {
        std::cerr << rec_in_f << "+" << rec_in_p << " records in\n";
        std::cerr << rec_out_f << "+" << rec_out_p << " records out\n";
    }

    if (status != STATUS_NOXFER) {
        double speed = total_bytes / (elapsed_sec > 0.000001 ? elapsed_sec : 0.000001);
        std::cerr << total_bytes << " bytes (" << format_bytes(total_bytes) << ") copied, "
                  << std::fixed << std::setprecision(3) << elapsed_sec << " s, "
                  << format_bytes(static_cast<uint64_t>(speed)) << "/s";
        if (is_progress) {
            std::cerr << "\r";
        } else {
            std::cerr << "\n";
        }
    }
}

void perform_skip(HANDLE hIn, uint64_t bytes_to_skip, uint64_t ibs) {
    if (bytes_to_skip == 0) return;
    LARGE_INTEGER li;
    li.QuadPart = bytes_to_skip;
    if (SetFilePointerEx(hIn, li, NULL, FILE_CURRENT)) {
        return;
    }
    // Seek failed (e.g. pipe or stdin), read and discard instead
    std::vector<char> skip_buf(static_cast<size_t>(ibs));
    uint64_t remaining = bytes_to_skip;
    while (remaining > 0) {
        DWORD to_read = static_cast<DWORD>((std::min)(remaining, ibs));
        DWORD bytes_read = 0;
        if (!ReadFile(hIn, skip_buf.data(), to_read, &bytes_read, NULL) || bytes_read == 0) {
            break;
        }
        remaining -= bytes_read;
    }
}

void perform_seek(HANDLE hOut, uint64_t bytes_to_seek, uint64_t obs) {
    if (bytes_to_seek == 0) return;
    LARGE_INTEGER li;
    li.QuadPart = bytes_to_seek;
    if (SetFilePointerEx(hOut, li, NULL, FILE_CURRENT)) {
        return;
    }
    // Seek failed, write zero bytes forward
    std::vector<char> zero_buf(static_cast<size_t>(obs), 0);
    uint64_t remaining = bytes_to_seek;
    while (remaining > 0) {
        DWORD to_write = static_cast<DWORD>((std::min)(remaining, obs));
        DWORD bytes_written = 0;
        if (!WriteFile(hOut, zero_buf.data(), to_write, &bytes_written, NULL) || bytes_written == 0) {
            break;
        }
        remaining -= bytes_written;
    }
}

int main(int argc, char* argv[]) {
    // Ensure binary mode for stdio streams
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    Options opt;
    bool bs_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_usage(argv[0]);
            return 0;
        }

        size_t eq_pos = arg.find('=');
        if (eq_pos == std::string::npos) {
            std::cerr << "dd: invalid key=value argument: '" << arg << "'\n";
            return 1;
        }

        std::string key = arg.substr(0, eq_pos);
        std::string val = arg.substr(eq_pos + 1);

        try {
            if (key == "if") opt.if_path = val;
            else if (key == "of") opt.of_path = val;
            else if (key == "bs") { opt.ibs = opt.obs = parse_size(val); bs_set = true; }
            else if (key == "ibs") { if (!bs_set) opt.ibs = parse_size(val); }
            else if (key == "obs") { if (!bs_set) opt.obs = parse_size(val); }
            else if (key == "count") opt.count = parse_size(val);
            else if (key == "skip") opt.skip = parse_size(val);
            else if (key == "seek") opt.seek = parse_size(val);
            else if (key == "conv") parse_conv(val, opt);
            else if (key == "status") {
                if (val == "none") opt.status = STATUS_NONE;
                else if (val == "noxfer") opt.status = STATUS_NOXFER;
                else if (val == "progress") opt.status = STATUS_PROGRESS;
                else {
                    std::cerr << "dd: invalid status level: '" << val << "'\n";
                    return 1;
                }
            } else {
                std::cerr << "dd: unknown operand: '" << key << "'\n";
                return 1;
            }
        } catch (const std::exception&) {
            std::cerr << "dd: invalid numeric value for operand '" << key << "'\n";
            return 1;
        }
    }

    // Open Input File / Handle
    HANDLE hIn = INVALID_HANDLE_VALUE;
    if (opt.if_path.empty() || opt.if_path == "-") {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
    } else {
        hIn = CreateFileA(opt.if_path.c_str(), GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hIn == INVALID_HANDLE_VALUE) {
            std::cerr << "dd: failed to open input file '" << opt.if_path << "': Error " << GetLastError() << "\n";
            return 1;
        }
    }

    // Open Output File / Handle
    HANDLE hOut = INVALID_HANDLE_VALUE;
    if (opt.of_path.empty() || opt.of_path == "-") {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    } else {
        DWORD creation = CREATE_ALWAYS;
        if (opt.conv_excl) {
            creation = CREATE_NEW;
        } else if (opt.conv_notrunc) {
            creation = OPEN_ALWAYS;
        }

        hOut = CreateFileA(opt.of_path.c_str(), GENERIC_WRITE | GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOut == INVALID_HANDLE_VALUE) {
            std::cerr << "dd: failed to open output file '" << opt.of_path << "': Error " << GetLastError() << "\n";
            if (hIn != GetStdHandle(STD_INPUT_HANDLE)) CloseHandle(hIn);
            return 1;
        }
    }

    // Perform Skip / Seek
    perform_skip(hIn, opt.skip * opt.ibs, opt.ibs);
    perform_seek(hOut, opt.seek * opt.obs, opt.obs);

    std::vector<char> in_buf(static_cast<size_t>(opt.ibs));
    
    uint64_t rec_in_f = 0, rec_in_p = 0;
    uint64_t rec_out_f = 0, rec_out_p = 0;
    uint64_t total_bytes_copied = 0;
    uint64_t blocks_processed = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto last_progress_time = start_time;

    // Main Transfer Loop
    while (blocks_processed < opt.count) {
        DWORD bytes_to_read = static_cast<DWORD>(opt.ibs);
        DWORD bytes_read = 0;

        BOOL read_success = ReadFile(hIn, in_buf.data(), bytes_to_read, &bytes_read, NULL);

        if (!read_success) {
            if (opt.conv_noerror) {
                std::cerr << "dd: error reading input file: Error " << GetLastError() << "\n";
                std::fill(in_buf.begin(), in_buf.end(), 0);
                bytes_read = static_cast<DWORD>(opt.ibs);
            } else {
                std::cerr << "dd: error reading input file: Error " << GetLastError() << "\n";
                break;
            }
        }

        if (bytes_read == 0) {
            break; // EOF reached
        }

        if (bytes_read == opt.ibs) rec_in_f++;
        else rec_in_p++;

        blocks_processed++;

        // Apply conv=sync
        if (opt.conv_sync && bytes_read < opt.ibs) {
            std::fill(in_buf.begin() + bytes_read, in_buf.end(), 0);
            bytes_read = static_cast<DWORD>(opt.ibs);
        }

        // Apply conv=ucase / conv=lcase
        if (opt.conv_ucase) {
            for (DWORD i = 0; i < bytes_read; ++i) {
                in_buf[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(in_buf[i])));
            }
        } else if (opt.conv_lcase) {
            for (DWORD i = 0; i < bytes_read; ++i) {
                in_buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(in_buf[i])));
            }
        }

        // Apply conv=swab
        if (opt.conv_swab) {
            for (DWORD i = 0; i + 1 < bytes_read; i += 2) {
                std::swap(in_buf[i], in_buf[i + 1]);
            }
        }

        // Write buffer to output
        DWORD bytes_written = 0;
        BOOL write_success = WriteFile(hOut, in_buf.data(), bytes_read, &bytes_written, NULL);

        if (!write_success || bytes_written < bytes_read) {
            std::cerr << "dd: error writing output file: Error " << GetLastError() << "\n";
            if (bytes_written > 0) {
                total_bytes_copied += bytes_written;
                if (bytes_written == opt.obs) rec_out_f++; else rec_out_p++;
            }
            break;
        }

        total_bytes_copied += bytes_written;
        if (bytes_written == opt.obs) rec_out_f++;
        else rec_out_p++;

        // Status progress updates
        if (opt.status == STATUS_PROGRESS) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed_since_last = std::chrono::duration<double>(now - last_progress_time).count();
            if (elapsed_since_last >= 0.5) {
                double elapsed_total = std::chrono::duration<double>(now - start_time).count();
                print_stats(rec_in_f, rec_in_p, rec_out_f, rec_out_p,
                            total_bytes_copied, elapsed_total, opt.status, true);
                last_progress_time = now;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_elapsed = std::chrono::duration<double>(end_time - start_time).count();

    if (opt.status == STATUS_PROGRESS) {
        std::cerr << "\n";
    }

    print_stats(rec_in_f, rec_in_p, rec_out_f, rec_out_p,
                total_bytes_copied, total_elapsed, opt.status, false);

    if (hIn != GetStdHandle(STD_INPUT_HANDLE)) CloseHandle(hIn);
    if (hOut != GetStdHandle(STD_OUTPUT_HANDLE)) CloseHandle(hOut);

    return 0;
}