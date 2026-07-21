#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <iomanip>
#include <climits>
#include <cstdio>
#include <algorithm>

enum PatternType {
    PAT_LINE_NUM,
    PAT_REGEX_MATCH, // /regex/
    PAT_REGEX_SKIP   // %regex%
};

struct Pattern {
    PatternType type;
    long long line_num = 0;
    std::string regex_str;
    long long offset = 0;
    int repeat = 0; // 0 means run 1 time; N means repeat N times; -1 means {*}
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... FILE PATTERN...\n"
              << "Output pieces of FILE separated by PATTERN(s) to files 'xx00', 'xx01', ...\n\n"
              << "Options:\n"
              << "  -f PREFIX, --prefix=PREFIX       use PREFIX instead of 'xx'\n"
              << "  -n DIGITS, --digits=DIGITS       use DIGITS digits instead of 2\n"
              << "  -s, --quiet, --silent            do not print counts of output file sizes\n"
              << "  -k, --keep-files                 do not remove output files on error\n"
              << "  -b FORMAT, --suffix-format=FORMAT use sprintf FORMAT for file suffixes\n"
              << "  -h, --help                       display this help and exit\n\n"
              << "PATTERNs may be:\n"
              << "  N          line number (split before line N)\n"
              << "  /REGEXP/[OFFSET]   split before line matching REGEXP\n"
              << "  %REGEXP%[OFFSET]   skip sections up to line matching REGEXP\n"
              << "  {N}        repeat previous pattern N times\n"
              << "  {*}        repeat previous pattern as many times as possible\n";
}

std::string format_filename(const std::string& prefix, int index, int digits, const std::string& custom_fmt) {
    if (!custom_fmt.empty()) {
        char buf[256];
        snprintf(buf, sizeof(buf), custom_fmt.c_str(), index);
        return prefix + buf;
    } else {
        std::ostringstream ss;
        ss << prefix << std::setfill('0') << std::setw(digits) << index;
        return ss.str();
    }
}

void cleanup_files(const std::vector<std::string>& files) {
    for (const auto& filename : files) {
        std::remove(filename.c_str());
    }
}

bool write_section(const std::vector<std::string>& lines,
                   size_t start_idx, size_t end_idx,
                   const std::string& out_filename,
                   bool quiet) {
    std::ofstream outfile(out_filename, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "csplit: cannot open '" << out_filename << "' for writing\n";
        return false;
    }

    uint64_t bytes_written = 0;
    for (size_t i = start_idx; i < end_idx; ++i) {
        outfile.write(lines[i].data(), lines[i].size());
        bytes_written += lines[i].size();
    }
    outfile.close();

    if (!quiet) {
        std::cout << bytes_written << "\n";
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::string prefix = "xx";
    int digits = 2;
    bool quiet = false;
    bool keep_files = false;
    std::string suffix_format = "";

    std::string input_filename;
    std::vector<std::string> pattern_args;

    // CLI option parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-s" || arg == "--quiet" || arg == "--silent") {
            quiet = true;
        } else if (arg == "-k" || arg == "--keep-files") {
            keep_files = true;
        } else if (arg == "-f") {
            if (i + 1 < argc) prefix = argv[++i];
            else { std::cerr << "csplit: option requires an argument -- 'f'\n"; return 1; }
        } else if (arg.rfind("-f", 0) == 0) {
            prefix = arg.substr(2);
        } else if (arg.rfind("--prefix=", 0) == 0) {
            prefix = arg.substr(9);
        } else if (arg == "-n") {
            if (i + 1 < argc) digits = std::stoi(argv[++i]);
            else { std::cerr << "csplit: option requires an argument -- 'n'\n"; return 1; }
        } else if (arg.rfind("-n", 0) == 0) {
            digits = std::stoi(arg.substr(2));
        } else if (arg.rfind("--digits=", 0) == 0) {
            digits = std::stoi(arg.substr(9));
        } else if (arg == "-b") {
            if (i + 1 < argc) suffix_format = argv[++i];
            else { std::cerr << "csplit: option requires an argument -- 'b'\n"; return 1; }
        } else if (arg.rfind("-b", 0) == 0) {
            suffix_format = arg.substr(2);
        } else if (arg.rfind("--suffix-format=", 0) == 0) {
            suffix_format = arg.substr(16);
        } else if (!arg.empty() && arg[0] == '-' && arg.length() > 1 && input_filename.empty()) {
            if (arg == "-") {
                input_filename = "-";
            } else {
                std::cerr << "csplit: invalid option '" << arg << "'\n";
                return 1;
            }
        } else {
            if (input_filename.empty()) {
                input_filename = arg;
            } else {
                pattern_args.push_back(arg);
            }
        }
    }

    if (input_filename.empty()) {
        std::cerr << "csplit: missing operand\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

    // Read input stream into line buffer
    std::vector<std::string> lines;
    std::ifstream infile;
    std::istream* in_stream = &std::cin;

    if (input_filename != "-") {
        infile.open(input_filename, std::ios::binary);
        if (!infile.is_open()) {
            std::cerr << "csplit: " << input_filename << ": No such file or directory\n";
            return 1;
        }
        in_stream = &infile;
    }

    std::string line;
    while (std::getline(*in_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            lines.push_back(line + "\n");
        } else {
            lines.push_back(line + "\n");
        }
    }

    // Parse patterns
    std::vector<Pattern> patterns;
    for (const auto& arg : pattern_args) {
        if (arg.empty()) continue;

        if (arg.front() == '{' && arg.back() == '}') {
            if (patterns.empty()) {
                std::cerr << "csplit: repeat count specified without preceding pattern\n";
                return 1;
            }
            std::string inner = arg.substr(1, arg.length() - 2);
            if (inner == "*") {
                patterns.back().repeat = -1;
            } else {
                try {
                    patterns.back().repeat = std::stoi(inner);
                } catch (...) {
                    std::cerr << "csplit: invalid repeat count: '" << arg << "'\n";
                    return 1;
                }
            }
        } else if (arg.front() == '/' || arg.front() == '%') {
            Pattern p;
            p.type = (arg.front() == '/') ? PAT_REGEX_MATCH : PAT_REGEX_SKIP;
            
            char delim = arg.front();
            size_t close_pos = arg.find(delim, 1);
            if (close_pos == std::string::npos) {
                std::cerr << "csplit: invalid pattern: '" << arg << "'\n";
                return 1;
            }

            p.regex_str = arg.substr(1, close_pos - 1);
            std::string offset_str = arg.substr(close_pos + 1);

            if (!offset_str.empty()) {
                try {
                    p.offset = std::stoll(offset_str);
                } catch (...) {
                    std::cerr << "csplit: invalid offset in pattern: '" << arg << "'\n";
                    return 1;
                }
            }
            patterns.push_back(p);
        } else {
            Pattern p;
            p.type = PAT_LINE_NUM;
            try {
                p.line_num = std::stoll(arg);
            } catch (...) {
                std::cerr << "csplit: invalid line number: '" << arg << "'\n";
                return 1;
            }
            patterns.push_back(p);
        }
    }

    // Execute split
    std::vector<std::string> created_files;
    size_t curr_line = 0;
    size_t total_lines = lines.size();
    int file_index = 0;
    bool error_occurred = false;

    for (const auto& pat : patterns) {
        int max_runs = (pat.repeat == -1) ? INT_MAX : (1 + pat.repeat);

        for (int run = 0; run < max_runs; ++run) {
            size_t target_line = 0;
            bool match_found = false;

            if (pat.type == PAT_LINE_NUM) {
                if (pat.line_num < 1) {
                    std::cerr << "csplit: line number out of range: " << pat.line_num << "\n";
                    error_occurred = true;
                    break;
                }
                target_line = static_cast<size_t>(pat.line_num - 1);
                if (target_line <= curr_line) {
                    std::cerr << "csplit: line number " << pat.line_num << " is smaller than or equal to current line\n";
                    error_occurred = true;
                    break;
                }
                if (target_line > total_lines) {
                    std::cerr << "csplit: line number " << pat.line_num << " out of range\n";
                    error_occurred = true;
                    break;
                }
                match_found = true;
            } else {
                std::regex reg;
                try {
                    reg = std::regex(pat.regex_str);
                } catch (const std::regex_error&) {
                    std::cerr << "csplit: invalid regex pattern: '" << pat.regex_str << "'\n";
                    error_occurred = true;
                    break;
                }

                size_t matched_idx = total_lines;
                for (size_t i = curr_line; i < total_lines; ++i) {
                    if (std::regex_search(lines[i], reg)) {
                        matched_idx = i;
                        break;
                    }
                }

                if (matched_idx == total_lines) {
                    if (pat.repeat == -1) {
                        break; // Graceful exit for {*}
                    } else {
                        std::cerr << "csplit: '" << pat.regex_str << "': match not found\n";
                        error_occurred = true;
                        break;
                    }
                }

                long long calc_target = static_cast<long long>(matched_idx) + pat.offset;
                if (calc_target < static_cast<long long>(curr_line) || calc_target > static_cast<long long>(total_lines)) {
                    if (pat.repeat == -1) {
                        break;
                    } else {
                        std::cerr << "csplit: offset out of range for pattern '" << pat.regex_str << "'\n";
                        error_occurred = true;
                        break;
                    }
                }

                target_line = static_cast<size_t>(calc_target);
                match_found = true;
            }

            if (!match_found) {
                break;
            }

            if (pat.type == PAT_LINE_NUM || pat.type == PAT_REGEX_MATCH) {
                std::string fname = format_filename(prefix, file_index, digits, suffix_format);
                if (!write_section(lines, curr_line, target_line, fname, quiet)) {
                    error_occurred = true;
                    break;
                }
                created_files.push_back(fname);
                file_index++;
            }

            if (target_line == curr_line && pat.repeat == -1) {
                break; // Prevent infinite loop if target line doesn't advance
            }

            curr_line = target_line;
        }

        if (error_occurred) {
            break;
        }
    }

    // Write remaining lines to residual file
    if (!error_occurred && curr_line < total_lines) {
        std::string fname = format_filename(prefix, file_index, digits, suffix_format);
        if (!write_section(lines, curr_line, total_lines, fname, quiet)) {
            error_occurred = true;
        } else {
            created_files.push_back(fname);
        }
    }

    if (error_occurred) {
        if (!keep_files) {
            cleanup_files(created_files);
        }
        return 1;
    }

    return 0;
}