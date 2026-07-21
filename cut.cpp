#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

// Represents a range of positions (1-based index)
struct Range {
    int start;
    int end; // -1 means "to the end of the line"
};

// Manages the list of active ranges for character/byte/field extraction
class Selection {
public:
    std::vector<Range> ranges;

    void add_range(int start, int end) {
        ranges.push_back({start, end});
    }

    // Checks if a given 1-based index is selected by any of the ranges
    bool is_selected(int index) const {
        for (const auto& r : ranges) {
            if (r.end == -1) {
                if (index >= r.start) return true;
            } else {
                if (index >= r.start && index <= r.end) return true;
            }
        }
        return false;
    }
};

// Parses a comma-separated list of ranges (e.g., "1-3,5,7-")
bool parse_list(const std::string& list_str, Selection& sel) {
    std::stringstream ss(list_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (item.empty()) return false;
        
        // Ensure there is at most one dash in the token
        if (std::count(item.begin(), item.end(), '-') > 1) {
            return false;
        }

        size_t dash = item.find('-');
        if (dash == std::string::npos) {
            // Single value (e.g., "5")
            try {
                int val = std::stoi(item);
                if (val <= 0) return false;
                sel.add_range(val, val);
            } catch (...) {
                return false;
            }
        } else {
            // Range (e.g., "1-3", "5-", or "-3")
            std::string start_str = item.substr(0, dash);
            std::string end_str = item.substr(dash + 1);
            int start = 1;
            int end = -1;

            if (!start_str.empty()) {
                try {
                    start = std::stoi(start_str);
                    if (start <= 0) return false;
                } catch (...) {
                    return false;
                }
            }
            if (!end_str.empty()) {
                try {
                    end = std::stoi(end_str);
                    if (end <= 0) return false;
                } catch (...) {
                    return false;
                }
            }
            if (end != -1 && start > end) {
                return false; // Invalid range (e.g., 5-3)
            }
            sel.add_range(start, end);
        }
    }
    return !sel.ranges.empty();
}

// Processes a single input stream line by line
void process_stream(std::istream& in, const Selection& sel, char mode, char delim, bool s_flag) {
    std::string line;
    while (std::getline(in, line)) {
        // Strip trailing CR (\r) if present on Windows platforms
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (mode == 'b' || mode == 'c') {
            // Byte/Character mode (processed equivalently for standard input)
            for (size_t i = 0; i < line.size(); ++i) {
                if (sel.is_selected(static_cast<int>(i + 1))) {
                    std::cout << line[i];
                }
            }
            std::cout << "\n";
        } else if (mode == 'f') {
            // Field-based extraction
            std::vector<std::string> fields;
            size_t start = 0;
            size_t end = line.find(delim);
            while (end != std::string::npos) {
                fields.push_back(line.substr(start, end - start));
                start = end + 1;
                end = line.find(delim, start);
            }
            fields.push_back(line.substr(start));

            if (fields.size() == 1) {
                // If there's no delimiter found in the line
                if (!s_flag) {
                    std::cout << line << "\n";
                }
            } else {
                bool first = true;
                for (size_t i = 0; i < fields.size(); ++i) {
                    if (sel.is_selected(static_cast<int>(i + 1))) {
                        if (!first) {
                            std::cout << delim;
                        }
                        std::cout << fields[i];
                        first = false;
                    }
                }
                std::cout << "\n";
            }
        }
    }
}

void print_usage() {
    std::cerr << "usage: cut -b list [file ...]\n"
              << "       cut -c list [file ...]\n"
              << "       cut -f list [-d delim] [-s] [file ...]\n";
}

int main(int argc, char* argv[]) {
    std::string b_list, c_list, f_list;
    std::string delim = "\t";
    bool s_flag = false;
    std::vector<std::string> files;

    // Command-line arguments parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.size() > 1) {
            char opt = arg[1];
            std::string val;
            if (arg.size() > 2) {
                val = arg.substr(2);
            } else if (opt != 's') {
                if (i + 1 < argc) {
                    val = argv[++i];
                } else {
                    std::cerr << "cut: option requires an argument -- " << opt << "\n";
                    print_usage();
                    return 1;
                }
            }

            if (opt == 'b') b_list = val;
            else if (opt == 'c') c_list = val;
            else if (opt == 'f') f_list = val;
            else if (opt == 'd') {
                if (val.empty()) {
                    std::cerr << "cut: bad delimiter\n";
                    return 1;
                }
                delim = val;
            }
            else if (opt == 's') s_flag = true;
            else {
                std::cerr << "cut: unknown option -- " << opt << "\n";
                print_usage();
                return 1;
            }
        } else {
            files.push_back(arg);
        }
    }

    // Exclusivity checks: only one of -b, -c, or -f can be specified
    int modes = (b_list.empty() ? 0 : 1) + (c_list.empty() ? 0 : 1) + (f_list.empty() ? 0 : 1);
    if (modes != 1) {
        std::cerr << "cut: must specify only one of -b, -c, or -f\n";
        print_usage();
        return 1;
    }

    char mode = '\0';
    std::string list_str;
    if (!b_list.empty()) {
        mode = 'b';
        list_str = b_list;
    } else if (!c_list.empty()) {
        mode = 'c';
        list_str = c_list;
    } else if (!f_list.empty()) {
        mode = 'f';
        list_str = f_list;
    }

    Selection sel;
    if (!parse_list(list_str, sel)) {
        std::cerr << "cut: " << list_str << ": invalid list value\n";
        return 1;
    }

    char delim_char = delim[0]; // Standard cut takes the first character of the delimiter string

    // If no files are specified, or if "-" is listed, read from standard input
    if (files.empty()) {
        process_stream(std::cin, sel, mode, delim_char, s_flag);
    } else {
        bool success = true;
        for (const auto& file : files) {
            if (file == "-") {
                process_stream(std::cin, sel, mode, delim_char, s_flag);
            } else {
                std::ifstream ifs(file);
                if (!ifs.is_open()) {
                    std::cerr << "cut: " << file << ": No such file or directory\n";
                    success = false;
                    continue;
                }
                process_stream(ifs, sel, mode, delim_char, s_flag);
            }
        }
        if (!success) {
            return 1;
        }
    }

    return 0;
}