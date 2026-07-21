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

struct CopyOptions {
    bool recursive = false;
    bool force = false;
    bool interactive = false;
    bool no_clobber = false;
    bool verbose = false;
    bool preserve = false;
};

#ifdef _WIN32
// Preserves Windows file attributes (e.g., Read-Only, Hidden, System, Archive)
void preserve_windows_attributes(const fs::path& src, const fs::path& dest) {
    DWORD attribs = GetFileAttributesW(src.c_str());
    if (attribs != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(dest.c_str(), attribs);
    }
}

// Preserves file/directory creation, access, and modification times on Windows
bool preserve_windows_times(const fs::path& src, const fs::path& dest) {
    HANDLE hSrc = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) return false;

    FILETIME creationTime, lastAccessTime, lastWriteTime;
    bool success = false;
    if (GetFileTime(hSrc, &creationTime, &lastAccessTime, &lastWriteTime)) {
        HANDLE hDest = CreateFileW(dest.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, 
                                  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hDest != INVALID_HANDLE_VALUE) {
            if (SetFileTime(hDest, &creationTime, &lastAccessTime, &lastWriteTime)) {
                success = true;
            }
            CloseHandle(hDest);
        }
    }
    CloseHandle(hSrc);
    return success;
}
#endif

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

// Copy a single file from source to destination
bool copy_file_to_file(const fs::path& src, const fs::path& dest, const CopyOptions& opts) {
    if (!fs::exists(src)) {
        std::wcerr << L"cp: " << src.wstring() << L": No such file or directory\n";
        return false;
    }

    if (fs::is_directory(src)) {
        std::wcerr << L"cp: " << src.wstring() << L" is a directory (not copied).\n";
        return false;
    }

    fs::path target = dest;
    if (fs::is_directory(dest)) {
        target = dest / src.filename();
    }

    if (fs::exists(target)) {
        if (opts.no_clobber) {
            return true; // Skip copy without warning
        }
        if (opts.interactive) {
            if (!should_overwrite(target)) {
                return true; // Skip copy, count as handled
            }
        }
    }

    try {
        fs::copy_options c_opts = fs::copy_options::none;
        if (opts.force) {
            if (fs::exists(target)) {
                fs::remove(target); // Force unlink destination first
            }
        } else {
            c_opts |= fs::copy_options::overwrite_existing;
        }

        fs::copy_file(src, target, c_opts);

        if (opts.preserve) {
#ifdef _WIN32
            preserve_windows_attributes(src, target);
            preserve_windows_times(src, target);
#else
            fs::last_write_time(target, fs::last_write_time(src));
            fs::permissions(target, fs::status(src).permissions());
#endif
        }

        if (opts.verbose) {
            std::wcout << src.wstring() << L" -> " << target.wstring() << std::endl;
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        std::wcerr << L"cp: " << e.what() << std::endl;
        return false;
    }
}

// Recursively copy contents of a directory
bool copy_directory(const fs::path& src, const fs::path& dest, const CopyOptions& opts) {
    if (!fs::exists(src)) {
        std::wcerr << L"cp: " << src.wstring() << L": No such file or directory\n";
        return false;
    }

    fs::path target_dir = dest;
    if (fs::exists(dest) && fs::is_directory(dest)) {
        target_dir = dest / src.filename();
    }

    try {
        if (!fs::exists(target_dir)) {
            if (!fs::create_directories(target_dir)) {
                std::wcerr << L"cp: failed to create directory " << target_dir.wstring() << std::endl;
                return false;
            }
            if (opts.verbose) {
                std::wcout << src.wstring() << L" -> " << target_dir.wstring() << L" (directory)" << std::endl;
            }
            if (opts.preserve) {
#ifdef _WIN32
                preserve_windows_attributes(src, target_dir);
                preserve_windows_times(src, target_dir);
#else
                fs::permissions(target_dir, fs::status(src).permissions());
#endif
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::wcerr << L"cp: " << e.what() << std::endl;
        return false;
    }

    bool success = true;
    for (const auto& entry : fs::directory_iterator(src)) {
        const auto& path = entry.path();
        if (fs::is_directory(path)) {
            if (opts.recursive) {
                if (!copy_directory(path, target_dir, opts)) {
                    success = false;
                }
            } else {
                std::wcerr << L"cp: " << path.wstring() << L" is a directory (not copied).\n";
                success = false;
            }
        } else {
            if (!copy_file_to_file(path, target_dir, opts)) {
                success = false;
            }
        }
    }
    return success;
}

int wmain(int argc, wchar_t* argv[]) {
    std::vector<std::wstring> sources;
    CopyOptions opts;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'R':
                    case L'r':
                        opts.recursive = true;
                        break;
                    case L'f':
                        opts.force = true;
                        opts.interactive = false; // -f overrides -i
                        break;
                    case L'i':
                        opts.interactive = true;
                        opts.force = false; // -i overrides -f
                        break;
                    case L'n':
                        opts.no_clobber = true;
                        break;
                    case L'v':
                        opts.verbose = true;
                        break;
                    case L'p':
                        opts.preserve = true;
                        break;
                    default:
                        std::wcerr << L"cp: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: cp [-Rripfv] source_file target_file\n";
                        std::wcerr << L"       cp [-Rripfv] source_file ... target_directory\n";
                        return 1;
                }
            }
        } else {
            sources.push_back(arg);
        }
    }

    if (sources.size() < 2) {
        std::wcerr << L"cp: usage: cp [-Rripfv] source_file target_file\n";
        std::wcerr << L"           cp [-Rripfv] source_file ... target_directory\n";
        return 1;
    }

    fs::path dest(sources.back());
    sources.pop_back();

    bool multiple_sources = (sources.size() > 1);

    if (multiple_sources && (!fs::exists(dest) || !fs::is_directory(dest))) {
        std::wcerr << L"cp: target '" << dest.wstring() << L"' is not an existing directory\n";
        return 1;
    }

    bool overall_success = true;
    for (const auto& src_str : sources) {
        fs::path src(src_str);
        if (!fs::exists(src)) {
            std::wcerr << L"cp: " << src.wstring() << L": No such file or directory\n";
            overall_success = false;
            continue;
        }

        if (fs::is_directory(src)) {
            if (!opts.recursive) {
                std::wcerr << L"cp: " << src.wstring() << L" is a directory (not copied).\n";
                overall_success = false;
            } else {
                if (!copy_directory(src, dest, opts)) {
                    overall_success = false;
                }
            }
        } else {
            if (!copy_file_to_file(src, dest, opts)) {
                overall_success = false;
            }
        }
    }

    return overall_success ? 0 : 1;
}