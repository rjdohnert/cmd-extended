#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <filesystem>

namespace fs = std::filesystem;

// Configuration Options
bool opt_all = false;
bool opt_grand_total = false;
bool opt_human_readable = false;
bool opt_no_junctions = true; 
uint64_t opt_block_size = 512; // BSD default is 512-byte blocks
int opt_max_depth = -1;

uint64_t grand_total_bytes = 0;
bool global_error = false;

// Format bytes into human-readable notation (e.g., 4.5M, 24K)
std::wstring format_human_readable(uint64_t bytes) {
    double size = static_cast<double>(bytes);
    const wchar_t* units[] = { L"B", L"K", L"M", L"G", L"T", L"P", L"E" };
    int unit_index = 0;

    while (size >= 1024.0 && unit_index < 6) {
        size /= 1024.0;
        unit_index++;
    }

    std::wstringstream ss;
    if (unit_index == 0) {
        ss << static_cast<uint64_t>(size) << units[unit_index];
    } else if (size < 10.0) {
        ss << std::fixed << std::setprecision(1) << size << units[unit_index];
    } else {
        ss << std::fixed << std::setprecision(0) << size << units[unit_index];
    }
    return ss.str();
}

// Convert byte size to rounded-up blocks of the specified size
uint64_t bytes_to_blocks(uint64_t bytes, uint64_t block_size) {
    if (bytes == 0) return 0;
    return (bytes + block_size - 1) / block_size;
}

// Print single line formatted as: [Size] \t [Path]
void print_item(uint64_t bytes, const std::wstring& path) {
    if (opt_human_readable) {
        std::wcout << format_human_readable(bytes) << L"\t" << path << L"\n";
    } else {
        std::wcout << bytes_to_blocks(bytes, opt_block_size) << L"\t" << path << L"\n";
    }
}

// Recursively calculates disk usage for directories
uint64_t calculate_disk_usage(const std::wstring& path, int current_depth) {
    uint64_t dir_total_bytes = 0;
    std::wstring search_path = path + L"\\*";
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(search_path.c_str(), &find_data);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::wcerr << L"du: " << path << L": Permission denied" << std::endl;
        global_error = true;
        return 0;
    }

    std::vector<std::wstring> subdirs;
    std::vector<std::pair<std::wstring, uint64_t>> files;

    do {
        std::wstring name = find_data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }

        std::wstring full_child_path = path + L"\\" + name;

        // Skip reparse points (symlinks/junctions) to avoid infinite traversal loops on Windows
        if (opt_no_junctions && (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            subdirs.push_back(full_child_path);
        } else {
            ULARGE_INTEGER file_size;
            // GetCompressedFileSizeW fetches the actual disk usage of compressed or sparse files
            file_size.LowPart = GetCompressedFileSizeW(full_child_path.c_str(), &file_size.HighPart);
            if (file_size.LowPart == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
                // Fallback to logical file size if query fails
                file_size.LowPart = find_data.nFileSizeLow;
                file_size.HighPart = find_data.nFileSizeHigh;
            }
            files.push_back({ full_child_path, file_size.QuadPart });
            dir_total_bytes += file_size.QuadPart;
        }
    } while (FindNextFileW(hFind, &find_data));

    FindClose(hFind);

    // Recursively process and add sizes of subdirectories
    for (const auto& subdir : subdirs) {
        dir_total_bytes += calculate_disk_usage(subdir, current_depth + 1);
    }

    // Print individual files if '-a' option is enabled and depth limits are met
    if (opt_all && (opt_max_depth == -1 || current_depth + 1 <= opt_max_depth)) {
        for (const auto& file : files) {
            print_item(file.second, file.first);
        }
    }

    // Print directory entry if depth limits are met
    if (opt_max_depth == -1 || current_depth <= opt_max_depth) {
        print_item(dir_total_bytes, path);
    }

    return dir_total_bytes;
}

// Processes direct targets (files or directories) passed via CLI
void process_argument(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"du: " << path << L": No such file or directory" << std::endl;
        global_error = true;
        return;
    }

    std::wstring abs_path = path;
    try {
        abs_path = fs::absolute(path).wstring();
    } catch (...) {
        // Fallback to relative path if absolute resolution fails
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        uint64_t total = calculate_disk_usage(abs_path, 0);
        if (opt_grand_total) {
            grand_total_bytes += total;
        }
    } else {
        ULARGE_INTEGER file_size;
        file_size.LowPart = GetCompressedFileSizeW(abs_path.c_str(), &file_size.HighPart);
        if (file_size.LowPart == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
            WIN32_FILE_ATTRIBUTE_DATA file_info;
            if (GetFileAttributesExW(abs_path.c_str(), GetFileExInfoStandard, &file_info)) {
                file_size.LowPart = file_info.nFileSizeLow;
                file_size.HighPart = file_info.nFileSizeHigh;
            } else {
                file_size.QuadPart = 0;
            }
        }
        print_item(file_size.QuadPart, abs_path);
        if (opt_grand_total) {
            grand_total_bytes += file_size.QuadPart;
        }
    }
}

void print_help() {
    std::wcout << L"Usage: du [-a | -s] [-c] [-h | -k | -m | -g] [-d depth] [-x] [file ...]\n";
}

int wmain(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);

    std::vector<std::wstring> paths;
    int i = 1;

    // Manual POSIX-like argument parsing
    while (i < argc) {
        std::wstring arg = argv[i];
        if (arg == L"--") {
            i++;
            while (i < argc) {
                paths.push_back(argv[i]);
                i++;
            }
            break;
        } else if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                wchar_t opt = arg[j];
                if (opt == L'a') {
                    opt_all = true;
                } else if (opt == L'c') {
                    opt_grand_total = true;
                } else if (opt == L'h') {
                    opt_human_readable = true;
                } else if (opt == L'k') {
                    opt_block_size = 1024;
                    opt_human_readable = false;
                } else if (opt == L'm') {
                    opt_block_size = 1024 * 1024;
                    opt_human_readable = false;
                } else if (opt == L'g') {
                    opt_block_size = 1024 * 1024 * 1024;
                    opt_human_readable = false;
                } else if (opt == L's') {
                    opt_max_depth = 0;
                } else if (opt == L'x') {
                    opt_no_junctions = true;
                } else if (opt == L'd') {
                    // Check if depth number is grouped (e.g. -d2) or separate (e.g. -d 2)
                    if (j + 1 < arg.size()) {
                        std::wstring depth_str = arg.substr(j + 1);
                        try {
                            opt_max_depth = std::stoi(depth_str);
                        } catch (...) {
                            std::wcerr << L"du: invalid depth value: " << depth_str << std::endl;
                            return 1;
                        }
                        break;
                    } else {
                        if (i + 1 < argc) {
                            std::wstring depth_str = argv[i + 1];
                            try {
                                opt_max_depth = std::stoi(depth_str);
                            } catch (...) {
                                std::wcerr << L"du: invalid depth value: " << depth_str << std::endl;
                                return 1;
                            }
                            i++;
                            break;
                        } else {
                            std::wcerr << L"du: option requires an argument -- d" << std::endl;
                            return 1;
                        }
                    }
                } else {
                    std::wcerr << L"du: unknown option: -" << opt << std::endl;
                    print_help();
                    return 1;
                }
            }
            i++;
        } else {
            paths.push_back(arg);
            i++;
        }
    }

    if (paths.empty()) {
        paths.push_back(L".");
    }

    for (const auto& path : paths) {
        process_argument(path);
    }

    if (opt_grand_total) {
        print_item(grand_total_bytes, L"total");
    }

    return global_error ? 1 : 0;
}