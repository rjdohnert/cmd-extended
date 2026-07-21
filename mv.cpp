#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cwchar>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

struct MoveOptions {
    bool force = false;
    bool interactive = false;
    bool no_clobber = false;
    bool verbose = false;
};

// Checks if the path string ends with a slash (to enforce directory target validation)
bool ends_with_slash(const std::wstring& s) {
    if (s.empty()) return false;
    wchar_t c = s.back();
    return (c == L'/' || c == L'\\');
}

// Handles user confirmation prompts for interactive mode (-i)
bool should_overwrite(const fs::path& target) {
    std::wcout << L"overwrite " << target.wstring() << L"? (y/n [n]) ";
    std::wstring input;
    if (std::getline(std::wcin, input)) {
        if (!input.empty() && (input[0] == L'y' || input[0] == L'Y')) {
            return true;
        }
    }
    return false;
}

// Fallback logic to copy and remove when moving objects across different physical drives
bool move_cross_device(const fs::path& src, const fs::path& target, const MoveOptions& opts) {
    try {
        fs::copy_options c_opts = fs::copy_options::recursive;
        if (opts.force) {
            c_opts |= fs::copy_options::overwrite_existing;
        }
        
        fs::copy(src, target, c_opts);
        fs::remove_all(src);
        
        if (opts.verbose) {
            std::wcout << src.wstring() << L" -> " << target.wstring() << L" (cross-device)" << std::endl;
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        std::wcerr << L"mv: cross-device move failed: " << e.what() << std::endl;
        return false;
    }
}

// Executes the move/rename operation
bool execute_move(const fs::path& src, const fs::path& target, const MoveOptions& opts) {
    try {
        fs::rename(src, target);
        if (opts.verbose) {
            std::wcout << src.wstring() << L" -> " << target.wstring() << std::endl;
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        bool is_cross_device = false;
        if (e.code() == std::errc::cross_device_link) {
            is_cross_device = true;
        }
#ifdef _WIN32
        if (e.code().value() == ERROR_NOT_SAME_DEVICE) {
            is_cross_device = true;
        }
#endif
        
        if (is_cross_device) {
            return move_cross_device(src, target, opts);
        } else {
            std::wcerr << L"mv: rename failed: " << e.what() << std::endl;
            return false;
        }
    }
}

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::wstring> sources;
    MoveOptions opts;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'f':
                        opts.force = true;
                        opts.interactive = false;
                        opts.no_clobber = false;
                        break;
                    case L'i':
                        opts.interactive = true;
                        opts.force = false;
                        opts.no_clobber = false;
                        break;
                    case L'n':
                        opts.no_clobber = true;
                        opts.force = false;
                        opts.interactive = false;
                        break;
                    case L'v':
                        opts.verbose = true;
                        break;
                    default:
                        std::wcerr << L"mv: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: mv [-fivn] source_file target_file\n";
                        std::wcerr << L"       mv [-fivn] source_file ... target_directory\n";
                        return 1;
                }
            }
        } else {
            sources.push_back(arg);
        }
    }

    if (sources.size() < 2) {
        std::wcerr << L"mv: usage: mv [-fivn] source_file target_file\n";
        std::wcerr << L"           mv [-fivn] source_file ... target_directory\n";
        return 1;
    }

    fs::path dest(sources.back());
    sources.pop_back();

    // Enforce that trailing slash destinations must refer to an existing directory
    if (ends_with_slash(dest.wstring()) && !fs::is_directory(dest)) {
        std::wcerr << L"mv: " << dest.wstring() << L": Not a directory\n";
        return 1;
    }

    bool multiple_sources = (sources.size() > 1);

    if (multiple_sources && (!fs::exists(dest) || !fs::is_directory(dest))) {
        std::wcerr << L"mv: target '" << dest.wstring() << L"' is not an existing directory\n";
        return 1;
    }

    bool overall_success = true;
    for (const auto& src_str : sources) {
        fs::path src(src_str);
        if (!fs::exists(src)) {
            std::wcerr << L"mv: " << src.wstring() << L": No such file or directory\n";
            overall_success = false;
            continue;
        }

        fs::path target = dest;
        if (fs::is_directory(dest)) {
            target = dest / src.filename();
        }

        // Prevent moving an object onto itself
        if (fs::exists(target) && fs::equivalent(src, target)) {
            std::wcerr << L"mv: '" << src.wstring() << L"' and '" << target.wstring() << L"' are the same file\n";
            overall_success = false;
            continue;
        }

        if (fs::exists(target)) {
            if (opts.no_clobber) {
                continue; // Skip without warning
            }
            if (opts.interactive) {
                if (!should_overwrite(target)) {
                    continue; // User cancelled
                }
            }
            
            // BSD rules check: Do not replace directories with files or vice versa
            if (fs::is_directory(target) && !fs::is_directory(src)) {
                std::wcerr << L"mv: cannot overwrite directory " << target.wstring() << L" with non-directory\n";
                overall_success = false;
                continue;
            }
            if (!fs::is_directory(target) && fs::is_directory(src)) {
                std::wcerr << L"mv: cannot overwrite non-directory " << target.wstring() << L" with directory\n";
                overall_success = false;
                continue;
            }

            // Remove the target cleanly beforehand so the rename call can execute
            try {
                if (fs::is_directory(target)) {
                    if (!fs::is_empty(target)) {
                        std::wcerr << L"mv: rename " << src.wstring() << L" to " << target.wstring() << L": Directory not empty\n";
                        overall_success = false;
                        continue;
                    }
                    fs::remove(target);
                } else {
                    fs::remove(target);
                }
            } catch (const fs::filesystem_error& e) {
                std::wcerr << L"mv: cannot remove existing target: " << e.what() << std::endl;
                overall_success = false;
                continue;
            }
        }

        if (!execute_move(src, target, opts)) {
            overall_success = false;
        }
    }

    return overall_success ? 0 : 1;
}