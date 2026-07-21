#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>
#include <fcntl.h>
#include <io.h>

enum SuffixType { SUFFIX_ALPHA, SUFFIX_NUMERIC, SUFFIX_HEX };
enum SplitMode { MODE_LINES, MODE_BYTES, MODE_CHUNKS };

struct Options {
    SplitMode mode = MODE_LINES;
    uint64_t line_count = 1000;
    uint64_t byte_count = 0;
    uint64_t chunk_count = 0;
    
    int suffix_len = 2;
    SuffixType suffix_type = SUFFIX_ALPHA;
    
    std::string input_file = "-";
    std::string prefix = "x";
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... [INPUT [PREFIX]]\n"
              << "Output pieces of INPUT to PREFIXaa, PREFIXab, ...; default\n"
              << "SIZE is 1000 lines, and default PREFIX is 'x'.\n\n"
              << "Options:\n"
              << "  -a, --suffix-length=N   use suffixes of length N (default: 2)\n"
              << "  -b, --bytes=SIZE        put SIZE bytes per output file\n"
              << "  -l, --lines=NUMBER      put NUMBER lines per output file\n"
              << "  -n, --number=CHUNKS     split into CHUNKS equal output files\n"
              << "  -d, --numeric-suffixes  use numeric suffixes instead of alphabetic\n"
              << "  -x                      use hexadecimal suffixes instead of alphabetic\n"
              << "  -h, --help              display this help and exit\n\n"
              << "SIZE may be followed by multiplier suffixes:\n"
              << "b=512, k=1024, m=1024^2, g=1024^3, t=1024^4.\n";
}

uint64_t parse_size(const std::string& str) {
    if (str.empty()) return 0;
    uint64_t mult = 1;
    std::string num_str = str;
    char last = static_cast<char>(std::tolower(static_cast<unsigned char>(str.back())));

    if (last == 'b') { mult = 512; num_str.pop_back(); }
    else if (last == 'k') { mult = 1024; num_str.pop_back(); }
    else if (last == 'm') { mult = 1024ULL * 1024; num_str.pop_back(); }
    else if (last == 'g') { mult = 1024ULL * 1024 * 1024; num_str.pop_back(); }
    else if (last == 't') { mult = 1024ULL * 1024 * 1024 * 1024; num_str.pop_back(); }

    return std::stoull(num_str) * mult;
}

std::string generate_suffix(uint64_t index, int len, SuffixType type) {
    std::string res(len, ' ');
    if (type == SUFFIX_ALPHA) {
        for (int i = len - 1; i >= 0; --i) {
            res[i] = 'a' + static_cast<char>(index % 26);
            index /= 26;
        }
    } else if (type == SUFFIX_NUMERIC) {
        for (int i = len - 1; i >= 0; --i) {
            res[i] = '0' + static_cast<char>(index % 10);
            index /= 10;
        }
    } else if (type == SUFFIX_HEX) {
        static const char hex_chars[] = "0123456789abcdef";
        for (int i = len - 1; i >= 0; --i) {
            res[i] = hex_chars[index % 16];
            index /= 16;
        }
    }
    return res;
}

uint64_t max_suffixes(int len, SuffixType type) {
    uint64_t base = (type == SUFFIX_ALPHA) ? 26 : ((type == SUFFIX_NUMERIC) ? 10 : 16);
    uint64_t res = 1;
    for (int i = 0; i < len; ++i) res *= base;
    return res;
}

void split_by_bytes(std::istream& in, const Options& opt) {
    const size_t BUF_SIZE = 65536; // 64 KB
    std::vector<char> buffer(BUF_SIZE);

    uint64_t file_index = 0;
    uint64_t max_files = max_suffixes(opt.suffix_len, opt.suffix_type);

    uint64_t bytes_in_current_file = 0;
    std::ofstream out;

    auto open_next_file = [&]() {
        if (file_index >= max_files) {
            std::cerr << "split: output file suffixes exhausted\n";
            exit(1);
        }
        std::string fname = opt.prefix + generate_suffix(file_index++, opt.suffix_len, opt.suffix_type);
        out.open(fname, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "split: cannot open '" << fname << "' for writing\n";
            exit(1);
        }
        bytes_in_current_file = 0;
    };

    open_next_file();

    while (in.read(buffer.data(), BUF_SIZE) || in.gcount() > 0) {
        std::streamsize bytes_read = in.gcount();
        size_t offset = 0;

        while (offset < static_cast<size_t>(bytes_read)) {
            uint64_t remaining_in_file = opt.byte_count - bytes_in_current_file;
            size_t to_write = static_cast<size_t>(std::min(static_cast<uint64_t>(bytes_read - offset), remaining_in_file));

            out.write(&buffer[offset], to_write);
            bytes_in_current_file += to_write;
            offset += to_write;

            if (bytes_in_current_file >= opt.byte_count) {
                out.close();
                if (offset < static_cast<size_t>(bytes_read) || in.peek() != EOF) {
                    open_next_file();
                }
            }
        }
    }

    if (out.is_open()) {
        out.close();
    }
}

void split_by_lines(std::istream& in, const Options& opt) {
    const size_t BUF_SIZE = 65536; // 64 KB
    std::vector<char> buffer(BUF_SIZE);

    uint64_t file_index = 0;
    uint64_t max_files = max_suffixes(opt.suffix_len, opt.suffix_type);

    uint64_t lines_in_current_file = 0;
    bool has_written_content = false;
    std::ofstream out;

    auto open_next_file = [&]() {
        if (file_index >= max_files) {
            std::cerr << "split: output file suffixes exhausted\n";
            exit(1);
        }
        std::string fname = opt.prefix + generate_suffix(file_index++, opt.suffix_len, opt.suffix_type);
        out.open(fname, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "split: cannot open '" << fname << "' for writing\n";
            exit(1);
        }
        lines_in_current_file = 0;
        has_written_content = false;
    };

    open_next_file();

    while (in.read(buffer.data(), BUF_SIZE) || in.gcount() > 0) {
        std::streamsize bytes_read = in.gcount();
        size_t start = 0;

        for (size_t i = 0; i < static_cast<size_t>(bytes_read); ++i) {
            if (buffer[i] == '\n') {
                lines_in_current_file++;
                if (lines_in_current_file == opt.line_count) {
                    size_t write_len = i - start + 1;
                    out.write(&buffer[start], write_len);
                    has_written_content = true;
                    out.close();

                    start = i + 1;

                    if (start < static_cast<size_t>(bytes_read) || in.peek() != EOF) {
                        open_next_file();
                    }
                }
            }
        }

        if (start < static_cast<size_t>(bytes_read)) {
            size_t remaining = static_cast<size_t>(bytes_read) - start;
            if (!out.is_open()) {
                open_next_file();
            }
            out.write(&buffer[start], remaining);
            has_written_content = true;
        }
    }

    if (out.is_open()) {
        out.close();
        if (!has_written_content && file_index > 1) {
            std::string empty_fname = opt.prefix + generate_suffix(file_index - 1, opt.suffix_len, opt.suffix_type);
            std::remove(empty_fname.c_str());
        }
    }
}

int main(int argc, char* argv[]) {
    // Ensure binary mode for stdio streams
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    Options opt;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-d" || arg == "--numeric-suffixes") {
            opt.suffix_type = SUFFIX_NUMERIC;
        } else if (arg == "-x") {
            opt.suffix_type = SUFFIX_HEX;
        } else if (arg == "-a") {
            if (i + 1 < argc) opt.suffix_len = std::stoi(argv[++i]);
            else { std::cerr << "split: option requires an argument -- 'a'\n"; return 1; }
        } else if (arg.rfind("-a", 0) == 0) {
            opt.suffix_len = std::stoi(arg.substr(2));
        } else if (arg.rfind("--suffix-length=", 0) == 0) {
            opt.suffix_len = std::stoi(arg.substr(16));
        } else if (arg == "-b") {
            if (i + 1 < argc) {
                opt.mode = MODE_BYTES;
                opt.byte_count = parse_size(argv[++i]);
            } else { std::cerr << "split: option requires an argument -- 'b'\n"; return 1; }
        } else if (arg.rfind("-b", 0) == 0) {
            opt.mode = MODE_BYTES;
            opt.byte_count = parse_size(arg.substr(2));
        } else if (arg.rfind("--bytes=", 0) == 0) {
            opt.mode = MODE_BYTES;
            opt.byte_count = parse_size(arg.substr(8));
        } else if (arg == "-l") {
            if (i + 1 < argc) {
                opt.mode = MODE_LINES;
                opt.line_count = std::stoull(argv[++i]);
            } else { std::cerr << "split: option requires an argument -- 'l'\n"; return 1; }
        } else if (arg.rfind("-l", 0) == 0) {
            opt.mode = MODE_LINES;
            opt.line_count = std::stoull(arg.substr(2));
        } else if (arg.rfind("--lines=", 0) == 0) {
            opt.mode = MODE_LINES;
            opt.line_count = std::stoull(arg.substr(8));
        } else if (arg == "-n") {
            if (i + 1 < argc) {
                opt.mode = MODE_CHUNKS;
                opt.chunk_count = std::stoull(argv[++i]);
            } else { std::cerr << "split: option requires an argument -- 'n'\n"; return 1; }
        } else if (arg.rfind("-n", 0) == 0) {
            opt.mode = MODE_CHUNKS;
            opt.chunk_count = std::stoull(arg.substr(2));
        } else if (arg.rfind("--number=", 0) == 0) {
            opt.mode = MODE_CHUNKS;
            opt.chunk_count = std::stoull(arg.substr(9));
        } else if (!arg.empty() && arg[0] == '-' && arg.length() > 1 && positional.empty()) {
            // Line count shortcut e.g. -500
            bool all_digits = true;
            for (size_t j = 1; j < arg.length(); ++j) {
                if (!std::isdigit(static_cast<unsigned char>(arg[j]))) { all_digits = false; break; }
            }
            if (all_digits) {
                opt.mode = MODE_LINES;
                opt.line_count = std::stoull(arg.substr(1));
            } else {
                std::cerr << "split: invalid option '" << arg << "'\n";
                return 1;
            }
        } else {
            positional.push_back(arg);
        }
    }

    if (!positional.empty()) {
        opt.input_file = positional[0];
        if (positional.size() > 1) {
            opt.prefix = positional[1];
        }
    }

    std::ifstream infile;
    std::istream* in_stream = &std::cin;

    if (opt.input_file != "-") {
        infile.open(opt.input_file, std::ios::binary);
        if (!infile.is_open()) {
            std::cerr << "split: " << opt.input_file << ": No such file or directory\n";
            return 1;
        }
        in_stream = &infile;
    }

    if (opt.mode == MODE_CHUNKS) {
        if (opt.chunk_count == 0) {
            std::cerr << "split: number of chunks must be greater than 0\n";
            return 1;
        }

        uint64_t total_bytes = 0;
        if (opt.input_file != "-") {
            infile.seekg(0, std::ios::end);
            total_bytes = infile.tellg();
            infile.seekg(0, std::ios::beg);
        } else {
            std::vector<char> stdin_data((std::istreambuf_iterator<char>(std::cin)),
                                          std::istreambuf_iterator<char>());
            total_bytes = stdin_data.size();
            
            std::string data_str(stdin_data.begin(), stdin_data.end());
            std::stringstream ss(data_str);
            
            opt.mode = MODE_BYTES;
            opt.byte_count = (total_bytes + opt.chunk_count - 1) / opt.chunk_count;
            if (opt.byte_count == 0) opt.byte_count = 1;

            split_by_bytes(ss, opt);
            return 0;
        }

        opt.mode = MODE_BYTES;
        opt.byte_count = (total_bytes + opt.chunk_count - 1) / opt.chunk_count;
        if (opt.byte_count == 0) opt.byte_count = 1;
    }

    if (opt.mode == MODE_BYTES) {
        if (opt.byte_count == 0) {
            std::cerr << "split: byte count must be greater than 0\n";
            return 1;
        }
        split_by_bytes(*in_stream, opt);
    } else {
        if (opt.line_count == 0) {
            std::cerr << "split: line count must be greater than 0\n";
            return 1;
        }
        split_by_lines(*in_stream, opt);
    }

    return 0;
}