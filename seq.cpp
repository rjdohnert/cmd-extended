#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <cctype>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... LAST\n"
              << "  or:  " << prog_name << " [OPTION]... FIRST LAST\n"
              << "  or:  " << prog_name << " [OPTION]... FIRST INCREMENT LAST\n"
              << "Print numbers from FIRST to LAST, in steps of INCREMENT.\n\n"
              << "Options:\n"
              << "  -f FORMAT     use printf style floating-point FORMAT\n"
              << "  -s STRING     use STRING to separate numbers (default: \\n)\n"
              << "  -w            equalize width by padding with leading zeroes\n"
              << "  -h, --help    display this help and exit\n";
}

// Helper to unescape string literals like \n or \t passed from Windows CLI
std::string unescape(const std::string& input) {
    std::string res;
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            switch (input[i + 1]) {
                case 'n': res += '\n'; ++i; break;
                case 't': res += '\t'; ++i; break;
                case 'r': res += '\r'; ++i; break;
                case '\\': res += '\\'; ++i; break;
                default: res += input[i]; break;
            }
        } else {
            res += input[i];
        }
    }
    return res;
}

// Holds formatting info for a raw numeric argument string
struct NumberInfo {
    std::string raw;
    double value = 0.0;
    int int_width = 0;
    int frac_width = 0;
};

NumberInfo parse_num(const std::string& str) {
    NumberInfo info;
    info.raw = str;
    info.value = std::stod(str);
    
    // Check if string is negative
    size_t start = (str[0] == '-' || str[0] == '+') ? 1 : 0;
    size_t dot_pos = str.find('.', start);

    if (dot_pos == std::string::npos) {
        info.int_width = static_cast<int>(str.length() - start);
        info.frac_width = 0;
    } else {
        info.int_width = static_cast<int>(dot_pos - start);
        info.frac_width = static_cast<int>(str.length() - dot_pos - 1);
    }
    return info;
}

int main(int argc, char* argv[]) {
    std::string separator = "\n";
    std::string custom_format = "";
    bool equal_width = false;
    std::vector<std::string> positional;

    // Command-line parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-s") {
            if (i + 1 < argc) separator = unescape(argv[++i]);
            else { std::cerr << "seq: option requires an argument -- 's'\n"; return 1; }
        } else if (arg.rfind("-s", 0) == 0) {
            separator = unescape(arg.substr(2));
        } else if (arg == "-f") {
            if (i + 1 < argc) custom_format = argv[++i];
            else { std::cerr << "seq: option requires an argument -- 'f'\n"; return 1; }
        } else if (arg.rfind("-f", 0) == 0) {
            custom_format = arg.substr(2);
        } else if (arg == "-w") {
            equal_width = true;
        } else if (!arg.empty() && arg[0] == '-' && arg.length() > 1 && (std::isdigit(arg[1]) || arg[1] == '.')) {
            // Negative number argument (e.g., -5 or -.5)
            positional.push_back(arg);
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "seq: invalid option '" << arg << "'\n";
            return 1;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty() || positional.size() > 3) {
        std::cerr << "seq: invalid number of arguments\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

    // Resolve default positional arguments
    std::string s_first = "1";
    std::string s_incr = "1";
    std::string s_last;

    if (positional.size() == 1) {
        s_last = positional[0];
    } else if (positional.size() == 2) {
        s_first = positional[0];
        s_last = positional[1];
    } else {
        s_first = positional[0];
        s_incr = positional[1];
        s_last = positional[2];
    }

    try {
        NumberInfo n_first = parse_num(s_first);
        NumberInfo n_incr  = parse_num(s_incr);
        NumberInfo n_last  = parse_num(s_last);

        double first = n_first.value;
        double incr  = n_incr.value;
        double last  = n_last.value;

        if (incr == 0.0) {
            std::cerr << "seq: zero increment step\n";
            return 1;
        }

        // Return early if range direction contradicts increment
        if ((incr > 0 && first > last) || (incr < 0 && first < last)) {
            return 0;
        }

        int max_frac = std::max({n_first.frac_width, n_incr.frac_width, n_last.frac_width});
        int max_int = std::max({n_first.int_width, n_incr.int_width, n_last.int_width});

        // Calculate step count using floating-point rounding safety margin
        long long steps = static_cast<long long>(std::floor((last - first) / incr + 1e-10)) + 1;

        for (long long i = 0; i < steps; ++i) {
            double val = first + i * incr;

            if (i > 0) {
                std::cout << separator;
            }

            if (!custom_format.empty()) {
                char buf[256];
                snprintf(buf, sizeof(buf), custom_format.c_str(), val);
                std::cout << buf;
            } else if (equal_width) {
                // Pad zero width equalization
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(max_frac) << val;
                std::string str_val = ss.str();

                bool is_neg = (val < 0);
                std::string abs_str = is_neg ? str_val.substr(1) : str_val;
                
                int total_width = max_int + (max_frac > 0 ? max_frac + 1 : 0);
                int pad_len = total_width - static_cast<int>(abs_str.length());

                if (is_neg) std::cout << "-";
                if (pad_len > 0) std::cout << std::string(pad_len, '0');
                std::cout << abs_str;
            } else {
                // Standard default formatting
                if (max_frac == 0) {
                    std::cout << static_cast<long long>(std::round(val));
                } else {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(max_frac) << val;
                    std::cout << ss.str();
                }
            }
        }
        std::cout << "\n";

    } catch (const std::exception&) {
        std::cerr << "seq: invalid floating point argument\n";
        return 1;
    }

    return 0;
}