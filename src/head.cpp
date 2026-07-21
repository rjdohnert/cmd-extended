#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// Print basic usage information
void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [-n count | -c bytes] [-q] [-v] [file ...]\n";
}

// Safely parse positive integers
bool parse_int(const std::string& str, long long& val) {
    if (str.empty()) return false;
    std::stringstream ss(str);
    ss >> val;
    return !ss.fail() && ss.eof() && val >= 0;
}

// Copy the requested lines or bytes from input to standard output
void head_stream(std::istream& in, long long limit, bool use_bytes) {
    if (use_bytes) {
        if (limit <= 0) return;
        char buffer[4096];
        long long remaining = limit;
        while (remaining > 0 && in) {
            long long to_read = (remaining < 4096) ? remaining : 4096;
            in.read(buffer, to_read);
            std::streamsize bytes_read = in.gcount();
            if (bytes_read > 0) {
                std::cout.write(buffer, bytes_read);
                remaining -= bytes_read;
            } else {
                break;
            }
        }
    } else {
        if (limit <= 0) return;
        char ch;
        long long count = 0;
        while (count < limit && in.get(ch)) {
            std::cout.put(ch);
            if (ch == '\n') {
                count++;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Optimize standard I/O operations
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

#ifdef _WIN32
    // Set standard input and output to binary mode to prevent CRLF translation.
    // This preserves exact file formats and guarantees precise byte-counting.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    long long line_count = 10;
    long long byte_count = 0;
    bool use_bytes = false;
    bool quiet = false;
    bool verbose = false;
    std::vector<std::string> files;

    // Manual argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            // End of options; treat everything else as files
            for (int j = i + 1; j < argc; ++j) {
                files.push_back(argv[j]);
            }
            break;
        } else if (arg == "-") {
            files.push_back("-");
        } else if (arg[0] == '-' && arg.size() > 1) {
            // Check for traditional BSD shortcut (e.g. -5 is equivalent to -n 5)
            if (std::isdigit(arg[1])) {
                long long val = 0;
                if (parse_int(arg.substr(1), val)) {
                    line_count = val;
                    use_bytes = false;
                } else {
                    std::cerr << "head: illegal line count -- " << arg.substr(1) << "\n";
                    return 1;
                }
            } else {
                // Parse options
                for (size_t j = 1; j < arg.size(); ++j) {
                    char option = arg[j];
                    if (option == 'h') {
                        print_usage(argv[0]);
                        return 0;
                    } else if (option == 'q') {
                        quiet = true;
                        verbose = false;
                    } else if (option == 'v') {
                        verbose = true;
                        quiet = false;
                    } else if (option == 'n' || option == 'c') {
                        std::string val_str;
                        // Read parameter value from current argument or next
                        if (j + 1 < arg.size()) {
                            val_str = arg.substr(j + 1);
                            j = arg.size(); // Finished parsing this cluster
                        } else {
                            if (i + 1 < argc) {
                                val_str = argv[++i];
                            } else {
                                std::cerr << "head: option requires an argument -- " << option << "\n";
                                print_usage(argv[0]);
                                return 1;
                            }
                        }
                        long long val = 0;
                        if (!parse_int(val_str, val)) {
                            if (option == 'n') {
                                std::cerr << "head: illegal line count -- " << val_str << "\n";
                            } else {
                                std::cerr << "head: illegal byte count -- " << val_str << "\n";
                            }
                            return 1;
                        }
                        if (option == 'n') {
                            line_count = val;
                            use_bytes = false;
                        } else {
                            byte_count = val;
                            use_bytes = true;
                        }
                    } else {
                        std::cerr << "head: unknown option -- " << option << "\n";
                        print_usage(argv[0]);
                        return 1;
                    }
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    // Default to stdin if no files are supplied
    if (files.empty()) {
        files.push_back("-");
    }

    // Decide whether to show headers
    bool show_headers = false;
    if (verbose) {
        show_headers = true;
    } else if (quiet) {
        show_headers = false;
    } else {
        show_headers = (files.size() > 1);
    }

    bool success = true;
    bool first_output = true;

    for (const auto& file : files) {
        long long limit = use_bytes ? byte_count : line_count;

        if (file == "-") {
            if (!first_output && show_headers) {
                std::cout << "\n";
            }
            if (show_headers) {
                std::cout << "==> standard input <==\n";
            }
            head_stream(std::cin, limit, use_bytes);
            first_output = false;
        } else {
            std::ifstream infile(file, std::ios_base::in | std::ios_base::binary);
            if (!infile.is_open()) {
                std::cerr << "head: " << file << ": No such file or directory\n";
                success = false;
                continue;
            }
            if (!first_output && show_headers) {
                std::cout << "\n";
            }
            if (show_headers) {
                std::cout << "==> " << file << " <==\n";
            }
            head_stream(infile, limit, use_bytes);
            first_output = false;
        }
    }

    return success ? 0 : 1;
}