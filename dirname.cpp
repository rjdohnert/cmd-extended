#include <iostream>
#include <string>
#include <vector>
#include <cctype>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION] PATH...\n"
              << "Print each PATH with its trailing /component removed.\n\n"
              << "Options:\n"
              << "  -z, --zero    end each output line with NUL, not newline\n"
              << "  -h, --help    display this help and exit\n";
}

bool is_slash(char c) {
    return c == '/' || c == '\\';
}

std::string get_dirname(const std::string& path) {
    if (path.empty()) {
        return ".";
    }

    std::string prefix;
    std::string rest = path;

    // 1. Handle UNC Network Paths: \\server\share\...
    if (path.length() >= 2 && is_slash(path[0]) && is_slash(path[1]) && 
        (path.length() == 2 || !is_slash(path[2]))) {
        size_t server_end = path.find_first_of("/\\", 2);
        if (server_end != std::string::npos) {
            size_t share_end = path.find_first_of("/\\", server_end + 1);
            if (share_end != std::string::npos) {
                prefix = path.substr(0, share_end);
                rest = path.substr(share_end);
            } else {
                return path; // e.g. \\server\share
            }
        } else {
            return path; // e.g. \\server
        }
    }
    // 2. Handle Drive Letters: C:\... or C:foo
    else if (path.length() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        prefix = path.substr(0, 2);
        rest = path.substr(2);
    }

    // If rest is empty (e.g. "C:"), return prefix
    if (rest.empty()) {
        return prefix.empty() ? "." : prefix;
    }

    // Check if rest consists entirely of slashes (e.g., "/" or "\\\")
    bool all_slashes = true;
    for (char c : rest) {
        if (!is_slash(c)) {
            all_slashes = false;
            break;
        }
    }

    if (all_slashes) {
        if (!prefix.empty()) {
            return prefix + "\\"; // e.g. "C:\"
        } else {
            return std::string(1, rest[0]); // e.g. "/" or "\"
        }
    }

    // Strip trailing slashes from rest
    size_t end = rest.length();
    while (end > 0 && is_slash(rest[end - 1])) {
        --end;
    }
    rest = rest.substr(0, end);

    // Find the last directory separator in rest
    size_t last_slash = rest.find_last_of("/\\");

    if (last_slash == std::string::npos) {
        // No slash left in rest
        if (!prefix.empty()) {
            return prefix; // e.g. "C:file.txt" -> "C:"
        } else {
            return ".";    // e.g. "file.txt" -> "."
        }
    }

    // Trim any consecutive slashes preceding the component
    size_t dir_end = last_slash;
    while (dir_end > 0 && is_slash(rest[dir_end - 1])) {
        --dir_end;
    }

    if (dir_end == 0) {
        // Last slash was at the root of rest (e.g., "/usr" -> "/")
        if (!prefix.empty()) {
            return prefix + "\\";
        } else {
            return std::string(1, rest[last_slash]);
        }
    }

    return prefix + rest.substr(0, dir_end);
}

int main(int argc, char* argv[]) {
    bool zero_terminate = false;
    bool stop_flags = false;
    std::vector<std::string> paths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (!stop_flags) {
            if (arg == "--") {
                stop_flags = true;
                continue;
            } else if (arg == "-h" || arg == "--help" || arg == "/?") {
                print_usage(argv[0]);
                return 0;
            } else if (arg == "-z" || arg == "--zero" || arg == "-0") {
                zero_terminate = true;
                continue;
            }
        }
        paths.push_back(arg);
    }

    if (paths.empty()) {
        std::cerr << "dirname: missing operand\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

    for (const auto& path : paths) {
        std::string dir = get_dirname(path);
        std::cout << dir;
        if (zero_terminate) {
            std::cout.put('\0');
        } else {
            std::cout << '\n';
        }
    }

    return 0;
}