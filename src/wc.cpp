#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

struct Counters {
    long long lines = 0;
    long long words = 0;
    long long bytes = 0;
    long long chars = 0;
    long long max_line_length = 0;
};

// Help / usage information
void print_usage() {
    std::cerr << "usage: wc [-clmwL] [file ...]\n";
}

// Format and print the requested counts right-aligned
void print_counts(const Counters& c, bool opt_lines, bool opt_words, bool opt_chars, bool opt_bytes, bool opt_max_len, const std::string& name) {
    if (opt_lines) {
        std::cout << std::setw(8) << c.lines;
    }
    if (opt_words) {
        std::cout << std::setw(8) << c.words;
    }
    if (opt_chars) {
        std::cout << std::setw(8) << c.chars;
    }
    if (opt_bytes) {
        std::cout << std::setw(8) << c.bytes;
    }
    if (opt_max_len) {
        std::cout << std::setw(8) << c.max_line_length;
    }
    if (!name.empty()) {
        std::cout << " " << name;
    }
    std::cout << "\n";
}

// Process an input stream in chunks using a local buffer
void process_stream(std::istream& in, Counters& c) {
    char buffer[16384];
    bool in_word = false;
    long long current_line_len = 0;

    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        std::streamsize bytes_read = in.gcount();
        c.bytes += bytes_read;

        for (std::streamsize i = 0; i < bytes_read; ++i) {
            unsigned char ch = static_cast<unsigned char>(buffer[i]);

            // UTF-8 Character Count (increment if the byte is not a continuation byte)
            if ((ch & 0xC0) != 0x80) {
                c.chars++;
            }

            // Word Count (checks standard whitespace characters)
            bool is_space = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f');
            if (is_space) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                c.words++;
            }

            // Line Count & Longest Line Tracker
            if (ch == '\n') {
                c.lines++;
                if (current_line_len > c.max_line_length) {
                    c.max_line_length = current_line_len;
                }
                current_line_len = 0;
            } else if (ch != '\r') {
                // If it is a UTF-8 character start, count toward line length
                if ((ch & 0xC0) != 0x80) {
                    current_line_len++;
                }
            }
        }
    }

    // Capture remaining line length if file does not end with a newline
    if (current_line_len > c.max_line_length) {
        c.max_line_length = current_line_len;
    }
}

int main(int argc, char* argv[]) {
    // Optimize standard I/O operations
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

#ifdef _WIN32
    // Set standard input to binary mode to prevent CRLF line ending translation
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    std::vector<std::string> files;
    bool opt_bytes = false;
    bool opt_chars = false;
    bool opt_lines = false;
    bool opt_words = false;
    bool opt_max_len = false;

    // Manual argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                files.push_back(argv[j]);
            }
            break;
        } else if (arg == "-") {
            files.push_back("-");
        } else if (arg[0] == '-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                char opt = arg[j];
                if (opt == 'c') opt_bytes = true;
                else if (opt == 'm') opt_chars = true;
                else if (opt == 'l') opt_lines = true;
                else if (opt == 'w') opt_words = true;
                else if (opt == 'L') opt_max_len = true;
                else {
                    std::cerr << "wc: unknown option -- " << opt << "\n";
                    print_usage();
                    return 1;
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    // Default flags if none are explicitly requested
    bool any_flag = (opt_bytes || opt_chars || opt_lines || opt_words || opt_max_len);
    if (!any_flag) {
        opt_lines = true;
        opt_words = true;
        opt_bytes = true;
    }

    // If no files are specified, read from standard input
    if (files.empty()) {
        files.push_back("-");
    }

    Counters total;
    bool success = true;

    for (const auto& file : files) {
        Counters file_c;
        if (file == "-") {
            process_stream(std::cin, file_c);
            print_counts(file_c, opt_lines, opt_words, opt_chars, opt_bytes, opt_max_len, "");
        } else {
            std::ifstream infile(file, std::ios_base::in | std::ios_base::binary);
            if (!infile.is_open()) {
                std::cerr << "wc: " << file << ": No such file or directory\n";
                success = false;
                continue;
            }
            process_stream(infile, file_c);
            print_counts(file_c, opt_lines, opt_words, opt_chars, opt_bytes, opt_max_len, file);
        }

        // Accumulate totals
        total.lines += file_c.lines;
        total.words += file_c.words;
        total.chars += file_c.chars;
        total.bytes += file_c.bytes;
        if (file_c.max_line_length > total.max_line_length) {
            total.max_line_length = file_c.max_line_length;
        }
    }

    // Print aggregated totals if there are multiple inputs
    if (files.size() > 1) {
        print_counts(total, opt_lines, opt_words, opt_chars, opt_bytes, opt_max_len, "total");
    }

    return success ? 0 : 1;
}