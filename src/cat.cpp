#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cwchar>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

struct CatOptions {
    bool number_nonblank = false;   // -b
    bool print_dollar = false;       // -e
    bool number_all = false;        // -n
    bool squeeze_blank = false;     // -s
    bool print_tabs = false;        // -t
    bool unbuffered = false;        // -u
    bool print_nonprinting = false;  // -v
};

struct CatState {
    int line_number = 1;
    int consecutive_empty_lines = 0;
    std::vector<unsigned char> current_line_buffer;
};

// Formats and prints a character according to BSD non-printing rules
void print_char_formatted(unsigned char ch, const CatOptions& opts) {
    if (ch == '\t') {
        if (opts.print_tabs) {
            std::printf("^I");
        } else {
            std::putchar('\t');
        }
        return;
    }

    if (ch == '\n') {
        if (opts.print_dollar) {
            std::putchar('$');
        }
        std::putchar('\n');
        return;
    }

    if (opts.print_nonprinting) {
        if (ch >= 128) {
            std::printf("M-");
            ch &= 0x7F; // Map down to low 7-bits
        }

        if (ch < 32) {
            std::printf("^%c", ch + 64);
        } else if (ch == 127) {
            std::printf("^?");
        } else {
            std::putchar(ch);
        }
    } else {
        std::putchar(ch);
    }
}

// Processes and outputs a complete line
void process_line(const std::vector<unsigned char>& line_buffer, CatOptions& opts, CatState& state) {
    bool is_empty = line_buffer.empty();
    
    if (is_empty) {
        state.consecutive_empty_lines++;
    } else {
        state.consecutive_empty_lines = 0;
    }

    // -s option: Squeeze multiple empty lines
    if (opts.squeeze_blank && is_empty && state.consecutive_empty_lines > 1) {
        return;
    }

    // Print line numbers (BSD format is right-aligned to width 6, followed by a TAB)
    if (opts.number_all || (opts.number_nonblank && !is_empty)) {
        std::printf("%6d\t", state.line_number++);
    }

    // Print characters
    for (unsigned char ch : line_buffer) {
        print_char_formatted(ch, opts);
    }

    // Print end of line indicator if required
    if (opts.print_dollar) {
        std::putchar('$');
    }
    std::putchar('\n');

    if (opts.unbuffered) {
        std::fflush(stdout);
    }
}

// Processes a trailing partial line that lacks a terminating newline
void process_partial_line(const std::vector<unsigned char>& line_buffer, CatOptions& opts, CatState& state) {
    if (line_buffer.empty()) return;

    if (opts.number_all || opts.number_nonblank) {
        std::printf("%6d\t", state.line_number++);
    }

    for (unsigned char ch : line_buffer) {
        print_char_formatted(ch, opts);
    }

    if (opts.unbuffered) {
        std::fflush(stdout);
    }
}

// Reads input character-by-character
void process_stream(std::istream& in, CatOptions& opts, CatState& state) {
    char ch;
    while (in.get(ch)) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (uch == '\n') {
            process_line(state.current_line_buffer, opts, state);
            state.current_line_buffer.clear();
        } else {
            state.current_line_buffer.push_back(uch);
        }
    }
}

int wmain(int argc, wchar_t* argv[]) {
    CatOptions opts;
    std::vector<std::wstring> files;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'b':
                        opts.number_nonblank = true;
                        opts.number_all = false; // -b takes precedence over -n
                        break;
                    case L'e':
                        opts.print_dollar = true;
                        opts.print_nonprinting = true; // Implies -v
                        break;
                    case L'n':
                        if (!opts.number_nonblank) {
                            opts.number_all = true;
                        }
                        break;
                    case L's':
                        opts.squeeze_blank = true;
                        break;
                    case L't':
                        opts.print_tabs = true;
                        opts.print_nonprinting = true; // Implies -v
                        break;
                    case L'u':
                        opts.unbuffered = true;
                        break;
                    case L'v':
                        opts.print_nonprinting = true;
                        break;
                    default:
                        std::wcerr << L"cat: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: cat [-benstuv] [-] [file ...]" << std::endl;
                        return 1;
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    // Prevent local translations for stdout/stdin to maintain byte accuracy
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    CatState state;
    int exit_status = 0;

    if (files.empty()) {
        process_stream(std::cin, opts, state);
        process_partial_line(state.current_line_buffer, opts, state);
        state.current_line_buffer.clear();
    } else {
        for (const auto& file_str : files) {
            if (file_str == L"-") {
                process_stream(std::cin, opts, state);
                process_partial_line(state.current_line_buffer, opts, state);
                state.current_line_buffer.clear();
            } else {
                fs::path p(file_str);
                std::ifstream file(p, std::ios::binary);
                if (!file) {
                    std::wcerr << L"cat: " << p.wstring() << L": No such file or directory" << std::endl;
                    exit_status = 1;
                    continue;
                }
                process_stream(file, opts, state);
                process_partial_line(state.current_line_buffer, opts, state);
                state.current_line_buffer.clear();
            }
        }
    }

    return exit_status;
}