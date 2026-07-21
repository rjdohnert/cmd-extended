#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// Print utility usage and exit
void print_usage(const char* prog_name) {
    std::cerr << "usage: " << prog_name << " [-123i] file1 file2\n";
}

// Perform character-by-character comparisons, optionally folding case
int compare_lines(const std::string& s1, const std::string& s2, bool ignore_case) {
    if (ignore_case) {
        size_t len1 = s1.length();
        size_t len2 = s2.length();
        size_t min_len = std::min(len1, len2);
        for (size_t i = 0; i < min_len; ++i) {
            unsigned char c1 = std::tolower(static_cast<unsigned char>(s1[i]));
            unsigned char c2 = std::tolower(static_cast<unsigned char>(s2[i]));
            if (c1 < c2) return -1;
            if (c1 > c2) return 1;
        }
        if (len1 < len2) return -1;
        if (len1 > len2) return 1;
        return 0;
    } else {
        if (s1 < s2) return -1;
        if (s1 > s2) return 1;
        return 0;
    }
}

// Read line-by-line while safely removing Windows trailing carriage returns
bool get_line(std::istream& in, std::string& line) {
    if (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    // Optimize standard I/O streams
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

#ifdef _WIN32
    // Prevent default Windows line ending translation for stream comparisons
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    bool suppress1 = false;
    bool suppress2 = false;
    bool suppress3 = false;
    bool ignore_case = false;
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
                if (opt == '1') suppress1 = true;
                else if (opt == '2') suppress2 = true;
                else if (opt == '3') suppress3 = true;
                else if (opt == 'i' || opt == 'f') ignore_case = true;
                else {
                    std::cerr << "comm: unknown option -- " << opt << "\n";
                    print_usage(argv[0]);
                    return 1;
                }
            }
        } else {
            files.push_back(arg);
        }
    }

    if (files.size() != 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (files[0] == "-" && files[1] == "-") {
        std::cerr << "comm: both files cannot be standard input\n";
        return 1;
    }

    std::istream* in1 = nullptr;
    std::istream* in2 = nullptr;
    std::ifstream file1;
    std::ifstream file2;

    // Open first file stream
    if (files[0] == "-") {
        in1 = &std::cin;
    } else {
        file1.open(files[0], std::ios::binary);
        if (!file1.is_open()) {
            std::cerr << "comm: " << files[0] << ": No such file or directory\n";
            return 1;
        }
        in1 = &file1;
    }

    // Open second file stream
    if (files[1] == "-") {
        in2 = &std::cin;
    } else {
        file2.open(files[1], std::ios::binary);
        if (!file2.is_open()) {
            std::cerr << "comm: " << files[1] << ": No such file or directory\n";
            return 1;
        }
        in2 = &file2;
    }

    // Dynamic prepended tab calculation logic
    auto print_col1 = [&](const std::string& line) {
        if (!suppress1) {
            std::cout << line << "\n";
        }
    };

    auto print_col2 = [&](const std::string& line) {
        if (!suppress2) {
            int tabs = !suppress1 ? 1 : 0;
            for (int t = 0; t < tabs; ++t) std::cout << "\t";
            std::cout << line << "\n";
        }
    };

    auto print_col3 = [&](const std::string& line) {
        if (!suppress3) {
            int tabs = (!suppress1 ? 1 : 0) + (!suppress2 ? 1 : 0);
            for (int t = 0; t < tabs; ++t) std::cout << "\t";
            std::cout << line << "\n";
        }
    };

    std::string line1, line2;
    bool has_line1 = get_line(*in1, line1);
    bool has_line2 = get_line(*in2, line2);

    // Merge comparison loop
    while (has_line1 && has_line2) {
        int cmp = compare_lines(line1, line2, ignore_case);
        if (cmp < 0) {
            print_col1(line1);
            has_line1 = get_line(*in1, line1);
        } else if (cmp > 0) {
            print_col2(line2);
            has_line2 = get_line(*in2, line2);
        } else {
            print_col3(line1);
            has_line1 = get_line(*in1, line1);
            has_line2 = get_line(*in2, line2);
        }
    }

    // Dump remaining elements of file1
    while (has_line1) {
        print_col1(line1);
        has_line1 = get_line(*in1, line1);
    }

    // Dump remaining elements of file2
    while (has_line2) {
        print_col2(line2);
        has_line2 = get_line(*in2, line2);
    }

    return 0;
}