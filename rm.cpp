#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cwchar>
#include <random>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

struct RmOptions {
    bool dir_only = false;    // -d
    bool force = false;       // -f
    bool interactive = false; // -i
    bool secure = false;      // -P
    bool recursive = false;   // -r / -R
    bool verbose = false;     // -v
};

// Convert std::error_code message to a wide string for logging
std::wstring fs_error_to_wstring(const std::error_code& ec) {
    std::string msg = ec.message();
    return std::wstring(msg.begin(), msg.end());
}

// Strips read-only attributes on Windows to allow unlinking
void make_writable(const fs::path& path) {
#ifdef _WIN32
    DWORD attribs = GetFileAttributesW(path.c_str());
    if (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(path.c_str(), attribs & ~FILE_ATTRIBUTE_READONLY);
    }
#else
    std::error_code ec;
    auto perms = fs::status(path, ec).permissions();
    if (!ec) {
        fs::permissions(path, perms | fs::perms::owner_write, fs::perm_options::add, ec);
    }
#endif
}

// Emulates BSD -P secure file overwriting (1 pass with pseudo-random bytes)
void overwrite_securely(const fs::path& file_path) {
    std::error_code ec;
    auto size = fs::file_size(file_path, ec);
    if (ec || size == 0) return;

    // Open file stream in read/write/binary mode
    std::ofstream out(file_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!out) return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distrib(0, 255);

    // 64KB chunk buffer filled with random values
    std::vector<char> buffer(65536);
    for (auto& byte : buffer) {
        byte = static_cast<char>(distrib(gen));
    }

    uint64_t remaining = size;
    while (remaining > 0) {
        uint64_t chunk = (std::min)(remaining, static_cast<uint64_t>(buffer.size()));
        out.write(buffer.data(), chunk);
        remaining -= chunk;
    }
    out.flush();
    out.close();
}

// Generic interactive confirmation prompt
bool confirm_interactive(const std::wstring& prompt_text) {
    std::wcout << prompt_text << L" (y/n [n])? ";
    std::wstring response;
    if (std::getline(std::wcin, response)) {
        if (!response.empty() && (response[0] == L'y' || response[0] == L'Y')) {
            return true;
        }
    }
    return false;
}

// Recursively processes and deletes directories
bool remove_directory_recursive(const fs::path& dir_path, const RmOptions& opts) {
    if (opts.interactive) {
        if (!confirm_interactive(L"examine files in directory " + dir_path.wstring())) {
            return true; // Skip this branch of the hierarchy
        }
    }

    std::error_code ec;
    bool success = true;

    // Pre-cache directory items to avoid iterator invalidation during deletes
    std::vector<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
        entries.push_back(entry.path());
    }
    if (ec) {
        if (!opts.force) {
            std::wcerr << L"rm: directory_iterator failed on " << dir_path.wstring() << L": " << fs_error_to_wstring(ec) << std::endl;
        }
        return false;
    }

    for (const auto& entry_path : entries) {
        if (fs::is_directory(entry_path, ec)) {
            if (!remove_directory_recursive(entry_path, opts)) {
                success = false;
            }
        } else {
            if (opts.interactive) {
                if (!confirm_interactive(L"remove " + entry_path.wstring())) {
                    continue;
                }
            }
            if (opts.secure) {
                overwrite_securely(entry_path);
            }
            make_writable(entry_path);
            fs::remove(entry_path, ec);
            if (ec) {
                if (!opts.force) {
                    std::wcerr << L"rm: " << entry_path.wstring() << L": " << fs_error_to_wstring(ec) << std::endl;
                }
                success = false;
            } else if (opts.verbose) {
                std::wcout << entry_path.wstring() << std::endl;
            }
        }
    }

    // Attempt to delete the parent directory once children are processed
    if (opts.interactive) {
        if (!confirm_interactive(L"remove directory " + dir_path.wstring())) {
            return success;
        }
    }

    make_writable(dir_path);
    fs::remove(dir_path, ec);
    if (ec) {
        if (!opts.force) {
            std::wcerr << L"rm: " << dir_path.wstring() << L": " << fs_error_to_wstring(ec) << std::endl;
        }
        success = false;
    } else if (opts.verbose) {
        std::wcout << dir_path.wstring() << std::endl;
    }

    return success;
}

// Dispatches removal actions based on file type
bool remove_item(const fs::path& path, const RmOptions& opts) {
    std::error_code ec;
    
    // Skip checking if file exists if forced, keeping parity with standard -f behavior
    if (!fs::exists(path, ec)) {
        if (!opts.force) {
            std::wcerr << L"rm: " << path.wstring() << L": No such file or directory\n";
            return false;
        }
        return true; 
    }

    // Safety checks
    if (path.filename() == L"." || path.filename() == L"..") {
        std::wcerr << L"rm: \".\" and \"..\" may not be removed\n";
        return false;
    }
    if (path.has_root_path() && path == path.root_path()) {
        std::wcerr << L"rm: it is forbidden to remove root directories\n";
        return false;
    }

    if (fs::is_directory(path, ec)) {
        if (!opts.recursive && !opts.dir_only) {
            std::wcerr << L"rm: " << path.wstring() << L": is a directory\n";
            return false;
        }

        if (opts.recursive) {
            return remove_directory_recursive(path, opts);
        } else if (opts.dir_only) {
            if (opts.interactive) {
                if (!confirm_interactive(L"remove directory " + path.wstring())) {
                    return true;
                }
            }
            make_writable(path);
            fs::remove(path, ec);
            if (ec) {
                if (!opts.force) {
                    std::wcerr << L"rm: " << path.wstring() << L": " << fs_error_to_wstring(ec) << std::endl;
                }
                return false;
            }
            if (opts.verbose) {
                std::wcout << path.wstring() << std::endl;
            }
            return true;
        }
    } else {
        // Handle regular files
        if (opts.interactive) {
            if (!confirm_interactive(L"remove " + path.wstring())) {
                return true;
            }
        }

        if (opts.secure) {
            overwrite_securely(path);
        }

        make_writable(path);
        fs::remove(path, ec);
        if (ec) {
            if (!opts.force) {
                std::wcerr << L"rm: " << path.wstring() << L": " << fs_error_to_wstring(ec) << std::endl;
            }
            return false;
        }
        if (opts.verbose) {
            std::wcout << path.wstring() << std::endl;
        }
        return true;
    }
    return false;
}

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::wstring> paths;
    RmOptions opts;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'd':
                        opts.dir_only = true;
                        break;
                    case L'f':
                        opts.force = true;
                        opts.interactive = false; // -f overrides -i
                        break;
                    case L'i':
                        opts.interactive = true;
                        opts.force = false;       // -i overrides -f
                        break;
                    case L'P':
                        opts.secure = true;
                        break;
                    case L'R':
                    case L'r':
                        opts.recursive = true;
                        break;
                    case L'v':
                        opts.verbose = true;
                        break;
                    default:
                        std::wcerr << L"rm: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: rm [-dfiPRrv] file ...\n";
                        return 1;
                }
            }
        } else {
            paths.push_back(arg);
        }
    }

    if (paths.empty()) {
        std::wcerr << L"usage: rm [-dfiPRrv] file ...\n";
        return 1;
    }

    bool overall_success = true;
    for (const auto& path_str : paths) {
        fs::path path(path_str);
        if (!remove_item(path, opts)) {
            overall_success = false;
        }
    }

    return overall_success ? 0 : 1;
}