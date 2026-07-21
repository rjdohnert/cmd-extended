#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#include <windows.h>
#define IsTerminal() (_isatty(_fileno(stdout)) != 0)
#else
#include <unistd.h>
#include <sys/ioctl.h>
#define IsTerminal() (isatty(fileno(stdout)) != 0)
#endif

struct LSOptions {
    bool long_format = false;
    bool show_all = false;         // -a
    bool show_almost_all = false;  // -A
    bool classify = false;         // -F
    bool human_readable = false;   // -h
    bool sort_by_time = false;     // -t
    bool sort_by_size = false;     // -S
    bool reverse_sort = false;     // -r
    bool single_column = false;    // -1
    bool color = false;
};

struct FileInfo {
    std::filesystem::path path;
    std::string name;
    bool is_dir = false;
    bool is_symlink = false;
    bool is_exec = false;
    bool is_hidden = false;
    uintmax_t size = 0;
    std::filesystem::file_time_type mtime;
};

// Enables virtual terminal processing for ANSI escape sequences on Windows consoles
void enable_ansi_colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#endif
}

// Queries current console terminal window width
int GetConsoleWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return csbi.dwSize.X;
    }
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        return w.ws_col;
    }
#endif
    return 80;
}

// Checks if a file path possesses the Windows system-level Hidden attribute
bool IsWindowsHidden(const std::filesystem::path& p) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(p.wstring().c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN));
#else
    return false;
#endif
}

// Identifies executables by checking standard binary/script extensions
bool IsExecutable(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".ps1");
}

// Maps Windows file attributes to simulated POSIX permissions
std::string GetPermissionsString(const FileInfo& info) {
    std::string p = "rwxr-xr-x";
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(info.path.wstring().c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
        p = "r-xr-xr-x"; // Read-only files remove the write flag
    }
#endif
    char type = '-';
    if (info.is_dir) type = 'd';
    else if (info.is_symlink) type = 'l';
    return type + p;
}

// Converts modification times into standardized, human-readable date formats
std::string FormatTime(std::filesystem::file_time_type ftime) {
#if defined(_MSC_VER)
    auto sct = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    time_t t = std::chrono::system_clock::to_time_t(sct);
#else
    time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::file_clock::to_sys(ftime)
    );
#endif

    tm ltm;
    localtime_s(&ltm, &t);
    char buf[64];
    time_t now = time(nullptr);
    
    // Switch between time and year display based on a 6-month threshold
    if (std::abs(difftime(now, t)) > 15778463) {
        strftime(buf, sizeof(buf), "%b %e  %Y", &ltm);
    } else {
        strftime(buf, sizeof(buf), "%b %e %H:%M", &ltm);
    }
    return buf;
}

// Formats file sizes into human-readable representation
std::string FormatSize(uintmax_t size, bool human) {
    if (!human) return std::to_string(size);
    const char* units[] = {"B", "K", "M", "G", "T"};
    int u = 0;
    double s = static_cast<double>(size);
    while (s >= 1024 && u < 4) {
        s /= 1024;
        u++;
    }
    if (u == 0) return std::to_string(size) + "B";
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << s << units[u];
    return ss.str();
}

// Color-codes output based on standard Unix environments
std::string ApplyColor(const FileInfo& info, const std::string& text) {
    if (info.is_dir) return "\x1b[1;34m" + text + "\x1b[0m";     // Bold Blue
    if (info.is_symlink) return "\x1b[1;36m" + text + "\x1b[0m"; // Bold Cyan
    if (info.is_exec) return "\x1b[1;32m" + text + "\x1b[0m";    // Bold Green
    return text;
}

// Scans target folders and extracts metadata
std::vector<FileInfo> ScanDirectory(const std::filesystem::path& dir, const LSOptions& opts) {
    std::vector<FileInfo> files;

    // Inject virtual dot directories if -a is parsed
    if (opts.show_all) {
        try {
            FileInfo dot, dotdot;
            dot.name = ".";
            dot.is_dir = true;
            dot.path = dir / ".";
            dot.mtime = std::filesystem::last_write_time(dir);
            
            dotdot.name = "..";
            dotdot.is_dir = true;
            dotdot.path = dir / "..";
            dotdot.mtime = std::filesystem::last_write_time(dir.parent_path().empty() ? dir : dir.parent_path());
            
            files.push_back(dot);
            files.push_back(dotdot);
        } catch (...) {}
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            FileInfo info;
            info.path = entry.path();
            info.name = entry.path().filename().string();
            info.is_dir = entry.is_directory();
            info.is_symlink = entry.is_symlink();
            info.is_exec = IsExecutable(info.path);
            info.is_hidden = (info.name.front() == '.' || IsWindowsHidden(info.path));
            
            try {
                info.size = entry.is_regular_file() ? entry.file_size() : 0;
                info.mtime = entry.last_write_time();
            } catch (...) {}

            // Filters hidden files based on options -a and -A
            if (info.is_hidden && !opts.show_all && !opts.show_almost_all) {
                continue;
            }
            files.push_back(info);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "wls: " << e.what() << "\n";
    }

    return files;
}

// Sorts contents according to target options
void SortFiles(std::vector<FileInfo>& files, const LSOptions& opts) {
    std::sort(files.begin(), files.end(), [&](const FileInfo& a, const FileInfo& b) {
        bool result = false;
        if (opts.sort_by_time) {
            result = a.mtime > b.mtime;
        } else if (opts.sort_by_size) {
            result = a.size > b.size;
        } else {
            // Case-insensitive alphabetical sort
            std::string sa = a.name, sb = b.name;
            std::transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
            std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
            result = sa < sb;
        }
        return opts.reverse_sort ? !result : result;
    });
}

// Displays directory listings formatted across multiple terminal columns
void DisplayMultiColumn(const std::vector<FileInfo>& files, const LSOptions& opts) {
    if (files.empty()) return;

    size_t max_len = 0;
    for (const auto& f : files) {
        size_t len = f.name.length() + (opts.classify ? 1 : 0);
        if (len > max_len) max_len = len;
    }

    int console_width = GetConsoleWidth();
    size_t col_width = max_len + 2; // Pad spacing between entries
    size_t num_cols = std::max<size_t>(1, console_width / col_width);
    size_t num_rows = (files.size() + num_cols - 1) / num_cols;

    // Output is ordered downwards in columns (column-major layout)
    for (size_t r = 0; r < num_rows; ++r) {
        for (size_t c = 0; c < num_cols; ++c) {
            size_t idx = c * num_rows + r;
            if (idx < files.size()) {
                const auto& f = files[idx];
                std::string display = f.name;
                if (opts.classify) {
                    if (f.is_dir) display += "/";
                    else if (f.is_symlink) display += "@";
                    else if (f.is_exec) display += "*";
                }
                
                std::string colored = opts.color ? ApplyColor(f, display) : display;
                std::cout << std::left << std::setw(static_cast<int>(col_width + (colored.length() - display.length()))) << colored;
            }
        }
        std::cout << "\n";
    }
}

// Outputs standard POSIX long-form details
void DisplayLongFormat(const std::vector<FileInfo>& files, const LSOptions& opts) {
    size_t max_size_len = 0;
    for (const auto& f : files) {
        size_t len = FormatSize(f.size, opts.human_readable).length();
        if (len > max_size_len) max_size_len = len;
    }

    for (const auto& f : files) {
        std::string perms = GetPermissionsString(f);
        std::string date = FormatTime(f.mtime);
        std::string size_str = FormatSize(f.size, opts.human_readable);

        std::cout << perms << " " 
                  << std::right << std::setw(static_cast<int>(max_size_len)) << size_str << " "
                  << date << " ";

        std::string display = f.name;
        if (opts.classify) {
            if (f.is_dir) display += "/";
            else if (f.is_symlink) display += "@";
            else if (f.is_exec) display += "*";
        }
        if (opts.color) {
            std::cout << ApplyColor(f, display) << "\n";
        } else {
            std::cout << display << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    LSOptions opts;
    std::vector<std::string> path_args;

    // Standard CLI option parser
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                switch (c) {
                    case 'l': opts.long_format = true; break;
                    case 'a': opts.show_all = true; break;
                    case 'A': opts.show_almost_all = true; break;
                    case 'F': opts.classify = true; break;
                    case 'h': opts.human_readable = true; break;
                    case 't': opts.sort_by_time = true; break;
                    case 'S': opts.sort_by_size = true; break;
                    case 'r': opts.reverse_sort = true; break;
                    case '1': opts.single_column = true; break;
                    default:
                        std::cerr << "ls: Unknown option: -" << c << "\n";
                        return 1;
                }
            }
        } else {
            path_args.push_back(arg);
        }
    }

    // Check terminal output to toggle coloring options
    opts.color = IsTerminal();
    if (opts.color) {
        enable_ansi_colors();
    }

    if (path_args.empty()) {
        path_args.push_back(".");
    }

    // Process target paths
    for (size_t i = 0; i < path_args.size(); ++i) {
        std::filesystem::path target(path_args[i]);
        if (path_args.size() > 1) {
            std::cout << target.string() << ":\n";
        }

        if (std::filesystem::is_directory(target)) {
            std::vector<FileInfo> files = ScanDirectory(target, opts);
            SortFiles(files, opts);

            if (opts.long_format) {
                DisplayLongFormat(files, opts);
            } else if (opts.single_column || !IsTerminal()) {
                for (const auto& f : files) {
                    std::string display = f.name;
                    if (opts.classify) {
                        if (f.is_dir) display += "/";
                        else if (f.is_symlink) display += "@";
                        else if (f.is_exec) display += "*";
                    }
                    std::cout << (opts.color ? ApplyColor(f, display) : display) << "\n";
                }
            } else {
                DisplayMultiColumn(files, opts);
            }
        } else {
            // Process target as a single file item
            FileInfo info;
            info.path = target;
            info.name = target.filename().string();
            info.is_dir = std::filesystem::is_directory(target);
            info.is_symlink = std::filesystem::is_symlink(target);
            info.is_exec = IsExecutable(info.path);
            
            try {
                info.size = std::filesystem::is_regular_file(target) ? std::filesystem::file_size(target) : 0;
                info.mtime = std::filesystem::last_write_time(target);
            } catch (...) {}

            std::vector<FileInfo> single_file_vec = { info };
            if (opts.long_format) {
                DisplayLongFormat(single_file_vec, opts);
            } else {
                std::cout << (opts.color ? ApplyColor(info, info.name) : info.name) << "\n";
            }
        }

        if (i + 1 < path_args.size()) {
            std::cout << "\n";
        }
    }

    return 0;
}