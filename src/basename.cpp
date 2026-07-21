#include <iostream>
#include <string>
#include <vector>
#include <cctype>

// Display basic utility usage
void print_usage(const char* prog_name) {
    std::cerr << "usage: " << prog_name << " string [suffix]\n";
    std::cerr << "       " << prog_name << " [-a] [-s suffix] string [...]\n";
}

// core basename parsing algorithm
std::string get_basename(std::string path, const std::string& suffix) {
    if (path.empty()) {
        return "";
    }

    // Step 1: Strip trailing path separators (supporting both Unix / and Windows \)
    size_t last_non_slash = path.find_last_not_of("/\\");
    if (last_non_slash == std::string::npos) {
        // Path consists entirely of slashes. Return a single slash of the matching type.
        return std::string(1, path[0]);
    }
    path = path.substr(0, last_non_slash + 1);

    // Step 2: Extract the last component of the path
    size_t last_slash = path.find_last_of("/\\");
    std::string result;
    if (last_slash != std::string::npos) {
        result = path.substr(last_slash + 1);
    } else {
        result = path;
    }

    // Step 3: Handle Windows drive prefixes if applicable (e.g., "C:foo" -> "foo")
    if (result.size() >= 2 && result[1] == ':' && std::isalpha(static_cast<unsigned char>(result[0]))) {
        if (result.size() > 2) {
            result = result.substr(2);
        }
    }

    // Step 4: Remove suffix if specified and not identical to the base string
    if (!suffix.empty() && result != suffix) {
        if (result.size() >= suffix.size()) {
            size_t suffix_pos = result.size() - suffix.size();
            if (result.compare(suffix_pos, suffix.size(), suffix) == 0) {
                result = result.substr(0, suffix_pos);
            }
        }
    }

    return result;
}

int main(int argc, char* argv[]) {
    // Optimize standard I/O operations
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    bool multiple = false;
    std::string suffix = "";
    std::vector<std::string> strings;

    int i = 1;
    // Manual command-line option parser to support combined options (e.g., -as .ext)
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--") {
            i++;
            break;
        } else if (arg == "-a") {
            multiple = true;
        } else if (arg == "-s") {
            if (i + 1 < argc) {
                suffix = argv[++i];
                multiple = true;
            } else {
                std::cerr << "basename: option requires an argument -- s\n";
                print_usage(argv[0]);
                return 1;
            }
        } else if (arg[0] == '-' && arg.size() > 1) {
            bool valid = true;
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'a') {
                    multiple = true;
                } else if (arg[j] == 's') {
                    if (j + 1 < arg.size()) {
                        suffix = arg.substr(j + 1);
                        multiple = true;
                        break;
                    } else if (i + 1 < argc) {
                        suffix = argv[++i];
                        multiple = true;
                        break;
                    } else {
                        std::cerr << "basename: option requires an argument -- s\n";
                        print_usage(argv[0]);
                        return 1;
                    }
                } else {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                std::cerr << "basename: unknown option -- " << arg[1] << "\n";
                print_usage(argv[0]);
                return 1;
            }
        } else {
            // First non-option argument reached
            break;
        }
    }

    // Collect remaining arguments as targets
    for (; i < argc; ++i) {
        strings.push_back(argv[i]);
    }

    if (multiple) {
        if (strings.empty()) {
            print_usage(argv[0]);
            return 1;
        }
        for (const auto& str : strings) {
            std::cout << get_basename(str, suffix) << "\n";
        }
    } else {
        if (strings.empty()) {
            print_usage(argv[0]);
            return 1;
        } else if (strings.size() == 1) {
            std::cout << get_basename(strings[0], "") << "\n";
        } else if (strings.size() == 2) {
            std::cout << get_basename(strings[0], strings[1]) << "\n";
        } else {
            std::cerr << "basename: extra operand '" << strings[2] << "'\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    return 0;
}