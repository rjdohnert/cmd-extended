#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winnetwk.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "mpr.lib") // Required for WNetGetConnectionW on MSVC

// Represents parsed data for a storage volume
struct DriveInfo {
    std::string filesystem; // Drive letter or UNC path
    std::string fs_type;    // NTFS, FAT32, etc.
    uint64_t size_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t avail_bytes = 0;
    int capacity_pct = 0;
    std::string mounted_on; // Drive root (e.g., "C:\")
    bool success = false;
};

// Represents a formatted output row
struct PrintRow {
    std::string filesystem;
    std::string type;
    std::string size;
    std::string used;
    std::string avail;
    std::string capacity;
    std::string mounted_on;
};

// Converts Wide String to UTF-8 String for correct console representation
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// Retrieves the UNC remote path for a mapped network drive
std::wstring GetNetworkSharePath(const std::wstring& drive_root) {
    if (drive_root.size() >= 2 && drive_root[1] == L':') {
        wchar_t local_name[3] = { drive_root[0], L':', L'\0' };
        wchar_t remote_name[MAX_PATH];
        DWORD buffer_len = MAX_PATH;
        DWORD res = WNetGetConnectionW(local_name, remote_name, &buffer_len);
        if (res == NO_ERROR) {
            return std::wstring(remote_name);
        }
    }
    return L"";
}

// Formats size bytes to a clean, human-readable string (BSD style)
std::string FormatHuman(uint64_t bytes, bool use_si) {
    double divisor = use_si ? 1000.0 : 1024.0;
    const char* suffixes[] = { "B", "K", "M", "G", "T", "P", "E" };
    double d_bytes = static_cast<double>(bytes);
    int idx = 0;
    while (d_bytes >= divisor && idx < 6) {
        d_bytes /= divisor;
        idx++;
    }
    char buf[32];
    if (idx == 0) {
        snprintf(buf, sizeof(buf), "%uB", static_cast<unsigned int>(bytes));
    } else {
        if (d_bytes < 10.0) {
            snprintf(buf, sizeof(buf), "%.1f%s", d_bytes, suffixes[idx]);
        } else {
            snprintf(buf, sizeof(buf), "%.0f%s", d_bytes, suffixes[idx]);
        }
    }
    return std::string(buf);
}

// Gathers space and system metadata for a targeted path
DriveInfo GetVolumeStats(const std::wstring& volume_root) {
    DriveInfo info;
    info.mounted_on = WStringToString(volume_root);

    UINT drive_type = GetDriveTypeW(volume_root.c_str());
    std::wstring filesystem_ws = volume_root;

    if (drive_type == DRIVE_REMOTE) {
        std::wstring remote_path = GetNetworkSharePath(volume_root);
        if (!remote_path.empty()) {
            filesystem_ws = remote_path;
        }
    } else {
        // Strip trailing backslash for local disk representation (e.g. "C:")
        if (filesystem_ws.size() >= 2 && filesystem_ws.back() == L'\\') {
            filesystem_ws.pop_back();
        }
    }
    info.filesystem = WStringToString(filesystem_ws);

    wchar_t volume_name[MAX_PATH + 1] = {0};
    wchar_t fs_name[MAX_PATH + 1] = {0};
    BOOL vol_res = GetVolumeInformationW(
        volume_root.c_str(), volume_name, MAX_PATH + 1, NULL, NULL, NULL, fs_name, MAX_PATH + 1
    );
    if (vol_res) {
        info.fs_type = WStringToString(fs_name);
    } else {
        info.fs_type = "unknown";
    }

    ULARGE_INTEGER free_bytes_avail = {0};
    ULARGE_INTEGER total_bytes = {0};
    ULARGE_INTEGER total_free_bytes = {0};

    BOOL space_res = GetDiskFreeSpaceExW(
        volume_root.c_str(), &free_bytes_avail, &total_bytes, &total_free_bytes
    );

    if (space_res) {
        info.size_bytes = total_bytes.QuadPart;
        info.avail_bytes = free_bytes_avail.QuadPart;
        info.used_bytes = total_bytes.QuadPart - total_free_bytes.QuadPart;

        if (info.size_bytes > 0) {
            info.capacity_pct = static_cast<int>((info.used_bytes * 100) / info.size_bytes);
        } else {
            info.capacity_pct = 0;
        }
        info.success = true;
    } else {
        info.success = false;
    }

    return info;
}

enum class DisplayMode {
    Blocks512,
    Blocks1K,
    Blocks1M,
    Blocks1G,
    Human1024,
    Human1000
};

int main(int argc, char* argv[]) {
    // Set Console Output to UTF-8 to display paths and drive letters seamlessly
    SetConsoleOutputCP(CP_UTF8);

    bool opt_a = false;
    bool opt_c = false;
    bool opt_l = false;
    bool opt_T = false;
    DisplayMode mode = DisplayMode::Blocks512; // Default BSD block size is 512 bytes
    std::vector<std::wstring> paths;

    // Command-line flag parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                char c = arg[j];
                if (c == 'a') opt_a = true;
                else if (c == 'c') opt_c = true;
                else if (c == 'l') opt_l = true;
                else if (c == 'T') opt_T = true;
                else if (c == 'b') mode = DisplayMode::Blocks512;
                else if (c == 'k') mode = DisplayMode::Blocks1K;
                else if (c == 'm') mode = DisplayMode::Blocks1M;
                else if (c == 'g') mode = DisplayMode::Blocks1G;
                else if (c == 'h') mode = DisplayMode::Human1024;
                else if (c == 'H') mode = DisplayMode::Human1000;
                else {
                    std::cerr << "df: unknown option -- " << c << "\n";
                    std::cerr << "usage: df [-abcgHhklmT] [file ...]\n";
                    return 1;
                }
            }
        } else {
            // Safe Unicode conversion for paths on Windows
            int wsize = MultiByteToWideChar(CP_UTF8, 0, arg.c_str(), -1, NULL, 0);
            if (wsize > 0) {
                std::wstring warg(wsize, 0);
                MultiByteToWideChar(CP_UTF8, 0, arg.c_str(), -1, &warg[0], wsize);
                if (!warg.empty() && warg.back() == L'\0') {
                    warg.pop_back();
                }
                paths.push_back(warg);
            }
        }
    }

    std::vector<DriveInfo> drives;
    bool has_errors = false;

    if (paths.empty()) {
        // Query all logical volumes
        std::vector<std::wstring> volume_roots;
        DWORD dwSize = GetLogicalDriveStringsW(0, NULL);
        if (dwSize > 0) {
            std::vector<wchar_t> buffer(dwSize);
            if (GetLogicalDriveStringsW(dwSize, buffer.data())) {
                wchar_t* p = buffer.data();
                while (*p) {
                    volume_roots.push_back(p);
                    p += wcslen(p) + 1;
                }
            }
        }

        for (const auto& root : volume_roots) {
            UINT drive_type = GetDriveTypeW(root.c_str());
            if (opt_l && drive_type == DRIVE_REMOTE) {
                continue;
            }

            DriveInfo info = GetVolumeStats(root);
            if (!info.success && !opt_a) {
                continue; // Skip offline/empty drives unless -a is requested
            }
            drives.push_back(info);
        }
    } else {
        // Query volumes containing specific paths passed by user
        for (const auto& path : paths) {
            wchar_t volume_path[MAX_PATH];
            if (!GetVolumePathNameW(path.c_str(), volume_path, MAX_PATH)) {
                std::cerr << "df: " << WStringToString(path) << ": No such file or directory\n";
                has_errors = true;
                continue;
            }

            UINT drive_type = GetDriveTypeW(volume_path);
            if (opt_l && drive_type == DRIVE_REMOTE) {
                std::cerr << "df: " << WStringToString(path) << ": File system is not local\n";
                has_errors = true;
                continue;
            }

            DriveInfo info = GetVolumeStats(volume_path);
            if (!info.success) {
                std::cerr << "df: " << WStringToString(path) << ": Device not ready or inaccessible\n";
                has_errors = true;
                continue;
            }
            drives.push_back(info);
        }
    }

    if (drives.empty()) {
        return has_errors ? 1 : 0;
    }

    // Set divisor for block calculations
    uint64_t divisor = 1;
    if (mode == DisplayMode::Blocks512) divisor = 512;
    else if (mode == DisplayMode::Blocks1K) divisor = 1024;
    else if (mode == DisplayMode::Blocks1M) divisor = 1048576;
    else if (mode == DisplayMode::Blocks1G) divisor = 1073741824;

    // Formatting values and calculating column widths dynamically
    PrintRow header;
    header.filesystem = "Filesystem";
    header.type = "Type";
    header.used = "Used";
    header.avail = "Avail";
    header.capacity = "Capacity";
    header.mounted_on = "Mounted on";

    if (mode == DisplayMode::Blocks512) header.size = "512-blocks";
    else if (mode == DisplayMode::Blocks1K) header.size = "1K-blocks";
    else if (mode == DisplayMode::Blocks1M) header.size = "1M-blocks";
    else if (mode == DisplayMode::Blocks1G) header.size = "1G-blocks";
    else header.size = "Size";

    size_t w_fs = header.filesystem.size();
    size_t w_type = header.type.size();
    size_t w_size = header.size.size();
    size_t w_used = header.used.size();
    size_t w_avail = header.avail.size();
    size_t w_cap = header.capacity.size();

    std::vector<PrintRow> rows;
    uint64_t grand_total_size = 0;
    uint64_t grand_total_used = 0;
    uint64_t grand_total_avail = 0;

    for (const auto& info : drives) {
        PrintRow row;
        row.filesystem = info.filesystem;
        row.type = info.fs_type;
        row.mounted_on = info.mounted_on;

        if (!info.success) {
            row.size = "-";
            row.used = "-";
            row.avail = "-";
            row.capacity = "-";
        } else {
            grand_total_size += info.size_bytes;
            grand_total_used += info.used_bytes;
            grand_total_avail += info.avail_bytes;

            if (mode == DisplayMode::Human1024) {
                row.size = FormatHuman(info.size_bytes, false);
                row.used = FormatHuman(info.used_bytes, false);
                row.avail = FormatHuman(info.avail_bytes, false);
            } else if (mode == DisplayMode::Human1000) {
                row.size = FormatHuman(info.size_bytes, true);
                row.used = FormatHuman(info.used_bytes, true);
                row.avail = FormatHuman(info.avail_bytes, true);
            } else {
                row.size = std::to_string(info.size_bytes / divisor);
                row.used = std::to_string(info.used_bytes / divisor);
                row.avail = std::to_string(info.avail_bytes / divisor);
            }
            row.capacity = std::to_string(info.capacity_pct) + "%";
        }

        w_fs = std::max(w_fs, row.filesystem.size());
        w_type = std::max(w_type, row.type.size());
        w_size = std::max(w_size, row.size.size());
        w_used = std::max(w_used, row.used.size());
        w_avail = std::max(w_avail, row.avail.size());
        w_cap = std::max(w_cap, row.capacity.size());

        rows.push_back(row);
    }

    // Append standard grand total row if -c is requested
    if (opt_c && grand_total_size > 0) {
        PrintRow total_row;
        total_row.filesystem = "total";
        total_row.type = "-";
        total_row.mounted_on = "";

        if (mode == DisplayMode::Human1024) {
            total_row.size = FormatHuman(grand_total_size, false);
            total_row.used = FormatHuman(grand_total_used, false);
            total_row.avail = FormatHuman(grand_total_avail, false);
        } else if (mode == DisplayMode::Human1000) {
            total_row.size = FormatHuman(grand_total_size, true);
            total_row.used = FormatHuman(grand_total_used, true);
            total_row.avail = FormatHuman(grand_total_avail, true);
        } else {
            total_row.size = std::to_string(grand_total_size / divisor);
            total_row.used = std::to_string(grand_total_used / divisor);
            total_row.avail = std::to_string(grand_total_avail / divisor);
        }

        int total_pct = static_cast<int>((grand_total_used * 100) / grand_total_size);
        total_row.capacity = std::to_string(total_pct) + "%";

        w_fs = std::max(w_fs, total_row.filesystem.size());
        w_size = std::max(w_size, total_row.size.size());
        w_used = std::max(w_used, total_row.used.size());
        w_avail = std::max(w_avail, total_row.avail.size());
        w_cap = std::max(w_cap, total_row.capacity.size());

        rows.push_back(total_row);
    }

    // Helper lambda to print formatted row
    auto print_line = [&](const PrintRow& r) {
        std::cout << std::left << std::setw(w_fs + 2) << r.filesystem;
        if (opt_T) {
            std::cout << std::left << std::setw(w_type + 2) << r.type;
        }
        std::cout << std::right << std::setw(w_size) << r.size << "  "
                  << std::right << std::setw(w_used) << r.used << "  "
                  << std::right << std::setw(w_avail) << r.avail << "  "
                  << std::right << std::setw(w_cap) << r.capacity << "  "
                  << std::left << r.mounted_on << "\n";
    };

    // Output table
    print_line(header);
    for (const auto& r : rows) {
        print_line(r);
    }

    return has_errors ? 1 : 0;
}