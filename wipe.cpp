#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <sstream>

static std::mt19937_64 g_rng(std::random_device{}());

enum PassType { PASS_ZERO, PASS_ONE, PASS_PATTERN, PASS_RANDOM };

struct PassConfig {
    PassType type;
    unsigned char pattern = 0x00;
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... FILE/DIR...\n"
              << "Securely erase files and directory trees by overwriting data.\n\n"
              << "Options:\n"
              << "  -f, --force        force deletion without prompt and override read-only\n"
              << "  -r, -R, --recursive remove directories and their contents recursively\n"
              << "  -v, --verbose      verbose mode, output detailed wipe progress\n"
              << "  -i, --interactive   prompt before wiping each file\n"
              << "  -q, --quick        quick mode (2 passes: random + zeroes)\n"
              << "  -d, --dod          DoD 5220.22-M mode (3 passes: zeroes, ones, random)\n"
              << "  -p, --passes N     set custom number of overwrite passes (default: 4)\n"
              << "  -h, --help         display this help and exit\n";
}

std::string random_string(size_t length) {
    static const char charset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    std::string str;
    str.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        str += charset[dist(g_rng)];
    }
    return str;
}

std::string get_parent_dir(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return path.substr(0, pos + 1);
}

std::string get_random_filepath(const std::string& original_path) {
    std::string parent = get_parent_dir(original_path);
    return parent + random_string(12) + ".tmp";
}

std::vector<PassConfig> get_pass_configs(int passes, bool dod_mode, bool quick_mode) {
    std::vector<PassConfig> configs;
    if (quick_mode) {
        configs.push_back({PASS_RANDOM, 0});
        configs.push_back({PASS_ZERO, 0x00});
        return configs;
    }
    if (dod_mode) {
        configs.push_back({PASS_ZERO, 0x00});
        configs.push_back({PASS_ONE, 0xFF});
        configs.push_back({PASS_RANDOM, 0});
        return configs;
    }

    if (passes <= 1) {
        configs.push_back({PASS_RANDOM, 0});
    } else if (passes == 2) {
        configs.push_back({PASS_RANDOM, 0});
        configs.push_back({PASS_ZERO, 0x00});
    } else {
        configs.push_back({PASS_ZERO, 0x00});
        configs.push_back({PASS_ONE, 0xFF});
        for (int i = 2; i < passes - 1; ++i) {
            if (i % 2 == 0) configs.push_back({PASS_PATTERN, 0xAA});
            else configs.push_back({PASS_PATTERN, 0x55});
        }
        configs.push_back({PASS_RANDOM, 0});
    }
    return configs;
}

void fill_buffer(std::vector<char>& buf, const PassConfig& cfg) {
    if (cfg.type == PASS_ZERO) {
        std::fill(buf.begin(), buf.end(), 0x00);
    } else if (cfg.type == PASS_ONE) {
        std::fill(buf.begin(), buf.end(), static_cast<char>(0xFF));
    } else if (cfg.type == PASS_PATTERN) {
        std::fill(buf.begin(), buf.end(), static_cast<char>(cfg.pattern));
    } else if (cfg.type == PASS_RANDOM) {
        std::uniform_int_distribution<unsigned int> dist(0, 255);
        for (size_t i = 0; i < buf.size(); ++i) {
            buf[i] = static_cast<char>(dist(g_rng));
        }
    }
}

bool secure_rename_and_delete(const std::string& filepath, bool verbose) {
    std::string current_path = filepath;
    
    // Obfuscate MFT filename record by renaming multiple times
    for (int i = 0; i < 3; ++i) {
        std::string new_path = get_random_filepath(current_path);
        if (MoveFileA(current_path.c_str(), new_path.c_str())) {
            current_path = new_path;
        } else {
            break;
        }
    }

    if (DeleteFileA(current_path.c_str())) {
        if (verbose) {
            std::cout << "[+] Wiped & Deleted: " << filepath << "\n";
        }
        return true;
    } else {
        std::cerr << "[-] Failed to delete: " << current_path << " (Error " << GetLastError() << ")\n";
        return false;
    }
}

bool wipe_file(const std::string& filepath, const std::vector<PassConfig>& passes, bool force, bool verbose) {
    if (force) {
        SetFileAttributesA(filepath.c_str(), FILE_ATTRIBUTE_NORMAL);
    }

    HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_WRITE | GENERIC_READ,
                               FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_FLAG_WRITE_THROUGH, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "[-] Cannot open file: " << filepath << " (Error " << GetLastError() << ")\n";
        return false;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size)) {
        std::cerr << "[-] Cannot query size for: " << filepath << "\n";
        CloseHandle(hFile);
        return false;
    }

    uint64_t total_bytes = static_cast<uint64_t>(file_size.QuadPart);
    const size_t BUFFER_SIZE = 65536; // 64 KB chunk
    std::vector<char> buffer(BUFFER_SIZE);

    for (size_t p = 0; p < passes.size(); ++p) {
        if (verbose) {
            std::cout << "[*] Wiping " << filepath << " - Pass " << (p + 1) << "/" << passes.size() << "...\n";
        }

        LARGE_INTEGER zero = {0};
        SetFilePointerEx(hFile, zero, NULL, FILE_BEGIN);

        uint64_t bytes_written_total = 0;
        fill_buffer(buffer, passes[p]);

        while (bytes_written_total < total_bytes) {
            DWORD to_write = static_cast<DWORD>((std::min)(static_cast<uint64_t>(BUFFER_SIZE), total_bytes - bytes_written_total));
            
            if (passes[p].type == PASS_RANDOM) {
                fill_buffer(buffer, passes[p]);
            }

            DWORD written = 0;
            if (!WriteFile(hFile, buffer.data(), to_write, &written, NULL) || written == 0) {
                std::cerr << "[-] Write error on file: " << filepath << " (Error " << GetLastError() << ")\n";
                CloseHandle(hFile);
                return false;
            }
            bytes_written_total += written;
        }

        // Flush hardware write buffers to physical disk
        FlushFileBuffers(hFile);
    }

    // Truncate file to 0 bytes
    LARGE_INTEGER zero = {0};
    SetFilePointerEx(hFile, zero, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
    CloseHandle(hFile);

    return secure_rename_and_delete(filepath, verbose);
}

bool wipe_directory(const std::string& dirpath, const std::vector<PassConfig>& passes,
                    bool recursive, bool force, bool verbose, bool interactive) {
    
    std::string search_path = dirpath + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cerr << "[-] Cannot access directory: " << dirpath << "\n";
        return false;
    }

    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;

        std::string full_path = dirpath + "\\" + name;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                // Remove junction/symlink without entering target directory
                if (force) SetFileAttributesA(full_path.c_str(), FILE_ATTRIBUTE_NORMAL);
                RemoveDirectoryA(full_path.c_str());
            } else if (recursive) {
                wipe_directory(full_path, passes, recursive, force, verbose, interactive);
            }
        } else {
            if (interactive) {
                std::cout << "Wipe file '" << full_path << "'? (y/N): ";
                char ans = 'n';
                std::cin >> ans;
                if (ans != 'y' && ans != 'Y') continue;
            }
            wipe_file(full_path, passes, force, verbose);
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    if (force) SetFileAttributesA(dirpath.c_str(), FILE_ATTRIBUTE_NORMAL);

    std::string current_dir = dirpath;
    for (int i = 0; i < 3; ++i) {
        std::string new_dir = get_random_filepath(current_dir);
        if (MoveFileA(current_dir.c_str(), new_dir.c_str())) {
            current_dir = new_dir;
        } else {
            break;
        }
    }

    if (RemoveDirectoryA(current_dir.c_str())) {
        if (verbose) {
            std::cout << "[+] Removed directory: " << dirpath << "\n";
        }
        return true;
    } else {
        std::cerr << "[-] Failed to remove directory: " << current_dir << " (Error " << GetLastError() << ")\n";
        return false;
    }
}

bool confirm_action(const std::string& target) {
    std::cout << "Are you sure you want to securely wipe '" << target << "'? (y/N): ";
    std::string resp;
    std::getline(std::cin, resp);
    if (resp.empty()) return false;
    return (resp[0] == 'y' || resp[0] == 'Y');
}

int main(int argc, char* argv[]) {
    bool force = false;
    bool recursive = false;
    bool verbose = false;
    bool interactive = false;
    bool quick_mode = false;
    bool dod_mode = false;
    int passes = 4;

    std::vector<std::string> targets;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-f" || arg == "--force") {
            force = true;
        } else if (arg == "-r" || arg == "-R" || arg == "--recursive") {
            recursive = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        } else if (arg == "-q" || arg == "--quick") {
            quick_mode = true;
        } else if (arg == "-d" || arg == "--dod") {
            dod_mode = true;
        } else if (arg == "-p" || arg == "--passes") {
            if (i + 1 < argc) {
                passes = std::stoi(argv[++i]);
            } else {
                std::cerr << "wipe: option '-p' requires an argument\n";
                return 1;
            }
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "wipe: invalid option '" << arg << "'\n";
            return 1;
        } else {
            targets.push_back(arg);
        }
    }

    if (targets.empty()) {
        std::cerr << "wipe: missing file or directory operand\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }

    std::vector<PassConfig> pass_configs = get_pass_configs(passes, dod_mode, quick_mode);

    for (auto& target : targets) {
        // Normalize slashes
        std::replace(target.begin(), target.end(), '/', '\\');

        // Trim trailing slash
        while (target.length() > 1 && target.back() == '\\') {
            target.pop_back();
        }

        DWORD attrs = GetFileAttributesA(target.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            std::cerr << "wipe: cannot access '" << target << "': No such file or directory\n";
            continue;
        }

        if (!force && !interactive) {
            if (!confirm_action(target)) {
                std::cout << "Skipped '" << target << "'.\n";
                continue;
            }
        }

        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            if (!recursive) {
                std::cerr << "wipe: '" << target << "' is a directory (use -r to recurse)\n";
                continue;
            }
            wipe_directory(target, pass_configs, recursive, force, verbose, interactive);
        } else {
            wipe_file(target, pass_configs, force, verbose);
        }
    }

    return 0;
}