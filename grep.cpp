#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <filesystem>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#include <windows.h>
#else
#include <unistd.h>
#define _isatty isatty
#define _fileno fileno
#endif

struct GrepOptions {
    bool case_insensitive = false;
    bool invert_match = false;
    bool line_numbers = false;
    bool count_only = false;
    bool files_only = false;
    bool recursive = false;
    bool color = false;
};

// Enables Virtual Terminal sequences (ANSI codes) on Windows 10/11 consoles
void enable_ansi_colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#endif
}

// Wraps occurrences of matching patterns in ANSI escape sequences for terminal output
std::string colorize_line(const std::string& line, const std::regex& pattern) {
    std::string result;
    std::string::const_iterator search_start(line.cbegin());
    std::smatch match;
    
    while (std::regex_search(search_start, line.cend(), match, pattern)) {
        // Append text preceding match
        result.append(search_start, match[0].first);
        // Append bold red matched text
        result += "\x1b[1;31m" + match.str() + "\x1b[0m";
        // Move search boundary past match
        search_start = match[0].second;
    }
    result.append(search_start, line.cend());
    return result;
}

// Processes a single file path line by line
void process_file(const std::filesystem::path& file_path, const std::regex& pattern, const GrepOptions& opts, bool show_filename) {
    std::ifstream infile(file_path);
    if (!infile.is_open()) {
        std::cerr << "grep: " << file_path.string() << ": No such file or directory\n";
        return;
    }

    std::string line;
    size_t line_num = 0;
    size_t match_count = 0;
    bool file_has_match = false;

    while (std::getline(infile, line)) {
        line_num++;
        bool is_match = std::regex_search(line, pattern);
        if (opts.invert_match) {
            is_match = !is_match;
        }

        if (is_match) {
            file_has_match = true;
            match_count++;

            if (opts.files_only) {
                break; // Stop parsing since we only care about file presence
            }

            if (!opts.count_only) {
                if (show_filename) {
                    if (opts.color) {
                        std::cout << "\x1b[35m" << file_path.string() << "\x1b[0m:"; // Purple filename
                    } else {
                        std::cout << file_path.string() << ":";
                    }
                }
                if (opts.line_numbers) {
                    if (opts.color) {
                        std::cout << "\x1b[32m" << line_num << "\x1b[0m:"; // Green line number
                    } else {
                        std::cout << line_num << ":";
                    }
                }
                if (opts.color && !opts.invert_match) {
                    std::cout << colorize_line(line, pattern) << "\n";
                } else {
                    std::cout << line << "\n";
                }
            }
        }
    }

    if (opts.files_only && file_has_match) {
        if (opts.color) {
            std::cout << "\x1b[35m" << file_path.string() << "\x1b[0m\n";
        } else {
            std::cout << file_path.string() << "\n";
        }
    } else if (opts.count_only) {
        if (show_filename) {
            if (opts.color) {
                std::cout << "\x1b[35m" << file_path.string() << "\x1b[0m:";
            } else {
                std::cout << file_path.string() << ":";
            }
        }
        std::cout << match_count << "\n";
    }
}

// Processes standard input pipeline
void process_stdin(const std::regex& pattern, const GrepOptions& opts) {
    std::string line;
    size_t line_num = 0;
    size_t match_count = 0;

    while (std::getline(std::cin, line)) {
        line_num++;
        bool is_match = std::regex_search(line, pattern);
        if (opts.invert_match) {
            is_match = !is_match;
        }

        if (is_match) {
            match_count++;
            if (opts.files_only) {
                std::cout << "(standard input)\n";
                return;
            }
            if (!opts.count_only) {
                if (opts.line_numbers) {
                    if (opts.color) {
                        std::cout << "\x1b[32m" << line_num << "\x1b[0m:";
                    } else {
                        std::cout << line_num << ":";
                    }
                }
                if (opts.color && !opts.invert_match) {
                    std::cout << colorize_line(line, pattern) << "\n";
                } else {
                    std::cout << line << "\n";
                }
            }
        }
    }

    if (opts.count_only) {
        std::cout << match_count << "\n";
    }
}

int main(int argc, char* argv[]) {
    GrepOptions opts;
    std::string pattern_str;
    std::vector<std::string> path_args;

    // Parse options (supports grouped flags e.g., -inv)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                switch (c) {
                    case 'i': opts.case_insensitive = true; break;
                    case 'v': opts.invert_match = true; break;
                    case 'n': opts.line_numbers = true; break;
                    case 'c': opts.count_only = true; break;
                    case 'l': opts.files_only = true; break;
                    case 'r':
                    case 'R': opts.recursive = true; break;
                    default:
                        std::cerr << "grep: Unknown option: -" << c << "\n";
                        return 1;
                }
            }
        } else {
            if (pattern_str.empty()) {
                pattern_str = arg;
            } else {
                path_args.push_back(arg);
            }
        }
    }

    if (pattern_str.empty()) {
        std::cerr << "Usage: grep [-ivnclrR] pattern [file...]\n";
        return 1;
    }

    // Determine output coloring (only colorize if output destination is a physical terminal)
    opts.color = (_isatty(_fileno(stdout)) != 0);
    if (opts.color) {
        enable_ansi_colors();
    }

    // Set regex flags and construct pattern
    auto regex_flags = std::regex_constants::ECMAScript;
    if (opts.case_insensitive) {
        regex_flags |= std::regex_constants::icase;
    }
    
    std::regex pattern;
    try {
        pattern = std::regex(pattern_str, regex_flags);
    } catch (const std::regex_error& e) {
        std::cerr << "grep: Invalid regular expression: " << e.what() << "\n";
        return 1;
    }

    // Process stdin if no target files/paths are provided
    if (path_args.empty()) {
        process_stdin(pattern, opts);
        return 0;
    }

    // Expand search target paths (supporting directory recursion)
    std::vector<std::filesystem::path> targets;
    for (const auto& path_str : path_args) {
        std::filesystem::path p(path_str);
        if (std::filesystem::is_directory(p)) {
            if (opts.recursive) {
                try {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(p)) {
                        if (std::filesystem::is_regular_file(entry.path())) {
                            targets.push_back(entry.path());
                        }
                    }
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "grep: " << e.what() << "\n";
                }
            } else {
                std::cerr << "grep: " << p.string() << ": Is a directory\n";
            }
        } else if (std::filesystem::is_regular_file(p)) {
            targets.push_back(p);
        } else {
            std::cerr << "grep: " << p.string() << ": No such file or directory\n";
        }
    }

    bool show_filenames = (targets.size() > 1 || opts.recursive);

    for (const auto& target_file : targets) {
        process_file(target_file, pattern, opts, show_filenames);
    }

    return 0;
}