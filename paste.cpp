#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

struct InputStream {
    bool is_stdin = false;
    std::ifstream file;
    bool eof = false;
};

// Help / usage information
void print_usage(const char* prog_name) {
    std::cerr << "usage: " << prog_name << " [-s] [-d list] [file ...]\n";
}

// Parse custom delimiter lists and handle backslash escapes
std::vector<std::string> parse_delimiters(const std::string& list) {
    std::vector<std::string> delims;
    if (list.empty()) {
        return delims;
    }
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] == '\\') {
            if (i + 1 < list.size()) {
                char next = list[i + 1];
                if (next == 't') {
                    delims.push_back("\t");
                } else if (next == 'n') {
                    delims.push_back("\n");
                } else if (next == '\\') {
                    delims.push_back("\\");
                } else if (next == '0') {
                    delims.push_back(""); // empty string = no separator
                } else {
                    delims.push_back(std::string(1, next));
                }
                i++; // skip escaped char
            } else {
                delims.push_back("\\");
            }
        } else {
            delims.push_back(std::string(1, list[i]));
        }
    }
    return delims;
}

// Safely read a single line from the given InputStream, stripping trailing \r characters
bool get_next_line(InputStream& stream, std::string& line) {
    if (stream.eof) {
        line = "";
        return false;
    }
    std::istream& in = stream.is_stdin ? std::cin : stream.file;
    if (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    } else {
        stream.eof = true;
        line = "";
        return false;
    }
}

int main(int argc, char* argv[]) {
    // Optimize standard I/O operations
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

#ifdef _WIN32
    // Set stdin and stdout to binary mode to ensure exact stream output
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::string delim_list = "\t";
    bool serial = false;
    std::vector<std::string> files;

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
                if (opt == 's') {
                    serial = true;
                } else if (opt == 'd') {
                    if (j + 1 < arg.size()) {
                        delim_list = arg.substr(j + 1);
                        break; 
                    } else {
                        if (i + 1 < argc) {
                            delim_list = argv[++i];
                            break;
                        } else {
                            std::cerr << "paste: option requires an argument -- d\n";
                            print_usage(argv[0]);
                            return 1;
                        }
                    }
                } else {
                    std::cerr << "paste: unknown option -- " << opt << "\n";
                    print_usage(argv[0]);
                    return 1;
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    std::vector<std::string> delimiters = parse_delimiters(delim_list);
    if (delimiters.empty()) {
        delimiters.push_back("");
    }

    std::vector<std::string> files_to_process = files;
    if (files_to_process.empty()) {
        files_to_process.push_back("-");
    }

    // Verify file accessibility before performing modifications (POSIX standard)
    bool any_open_error = false;
    for (const auto& file : files_to_process) {
        if (file != "-") {
            std::ifstream temp(file, std::ios::binary);
            if (!temp.is_open()) {
                std::cerr << "paste: " << file << ": No such file or directory\n";
                any_open_error = true;
            }
        }
    }
    if (any_open_error) {
        return 1;
    }

    if (serial) {
        // Serial mode execution (-s)
        for (const auto& file : files_to_process) {
            InputStream stream;
            if (file == "-") {
                stream.is_stdin = true;
            } else {
                stream.file.open(file, std::ios::binary);
            }

            std::string line;
            bool first = true;
            size_t delim_idx = 0;

            while (get_next_line(stream, line)) {
                if (!first) {
                    std::cout << delimiters[delim_idx % delimiters.size()];
                    delim_idx++;
                }
                std::cout << line;
                first = false;
            }
            if (!first) {
                std::cout << "\n";
            }
        }
    } else {
        // Parallel mode execution (Default)
        std::vector<InputStream> streams;
        for (const auto& file : files_to_process) {
            InputStream stream;
            if (file == "-") {
                stream.is_stdin = true;
            } else {
                stream.file.open(file, std::ios::binary);
            }
            streams.push_back(std::move(stream));
        }

        size_t num_streams = streams.size();
        while (true) {
            std::vector<std::string> current_lines(num_streams);
            std::vector<bool> success_flags(num_streams);
            bool any_active = false;

            for (size_t i = 0; i < num_streams; ++i) {
                if (!streams[i].eof) {
                    any_active = true;
                    std::string line;
                    bool ok = get_next_line(streams[i], line);
                    current_lines[i] = line;
                    success_flags[i] = ok;
                } else {
                    current_lines[i] = "";
                    success_flags[i] = false;
                }
            }

            if (!any_active) {
                break;
            }

            // If none of the active streams could read a new line, break to avoid trailing empty output
            bool any_success = false;
            for (size_t i = 0; i < num_streams; ++i) {
                if (success_flags[i]) {
                    any_success = true;
                    break;
                }
            }
            if (!any_success) {
                break;
            }

            for (size_t i = 0; i < num_streams; ++i) {
                std::cout << current_lines[i];
                if (i < num_streams - 1) {
                    std::cout << delimiters[i % delimiters.size()];
                }
            }
            std::cout << "\n";
        }
    }

    return 0;
}