#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <cstdint>

#pragma comment(lib, "Shlwapi.lib")

namespace fs = std::filesystem;

// Configuration Options
struct Config {
    bool recursive = false;      // -r
    bool archive = false;        // -a (-rlpt)
    bool verbose = false;        // -v
    bool quiet = false;          // -q
    bool dry_run = false;        // -n
    bool update = false;         // -u
    bool checksum = false;       // -c
    bool preserve_times = false; // -t
    bool preserve_attrs = false; // -p
    bool delete_dest = false;    // --delete
    bool size_only = false;      // --size-only
    bool existing_only = false;  // --existing
    bool ignore_existing = false;// --ignore-existing
    bool progress = false;       // --progress
    uint64_t bwlimit = 0;        // --bwlimit (in KB/s)

    std::vector<std::wstring> excludes;
    std::vector<std::wstring> includes;

    std::wstring src_path;
    std::wstring dest_path;
};

// Rolling Adler-32 Checksum for fast block matching
struct RollingHash {
    uint16_t a = 0;
    uint16_t b = 0;
    size_t count = 0;

    void init(const uint8_t* data, size_t len) {
        a = 0; b = 0; count = len;
        for (size_t i = 0; i < len; ++i) {
            a += data[i];
            b += a;
        }
    }

    uint32_t get_value() const {
        return (static_cast<uint32_t>(b) << 16) | a;
    }

    void roll(uint8_t out_byte, uint8_t in_byte) {
        a = static_cast<uint16_t>(a - out_byte + in_byte);
        b = static_cast<uint16_t>(b - (count * out_byte) + a);
    }
};

// 64-bit FNV-1a Hash for strong block validation
uint64_t fnv1a_64(const uint8_t* data, size_t len) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

struct BlockChecksum {
    uint64_t offset;
    uint32_t weak_hash;
    uint64_t strong_hash;
};

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... SRC... DEST\n"
              << "A fast, versatile local file-copying and synchronization tool.\n\n"
              << "Options:\n"
              << "  -r, --recursive          recurse into directories\n"
              << "  -a, --archive            archive mode (-r -t -p)\n"
              << "  -v, --verbose            increase verbosity\n"
              << "  -q, --quiet              suppress non-error messages\n"
              << "  -n, --dry-run            perform a trial run with no changes made\n"
              << "  -u, --update             skip files that are newer on destination\n"
              << "  -c, --checksum           skip based on checksum, not mod-time & size\n"
              << "  -t, --times              preserve modification times\n"
              << "  -p, --perms              preserve file attributes\n"
              << "      --delete             delete extraneous files from destination dirs\n"
              << "      --size-only          skip files that match in size\n"
              << "      --existing           skip creating new files on destination\n"
              << "      --ignore-existing    skip updating files that exist on destination\n"
              << "      --progress           show progress during transfer\n"
              << "      --bwlimit=RATE       limit I/O bandwidth; RATE in KB/s\n"
              << "      --exclude=PATTERN    exclude files matching PATTERN\n"
              << "      --include=PATTERN    don't exclude files matching PATTERN\n"
              << "  -h, --help               display this help text and exit\n";
}

bool matches_wildcard(const std::wstring& filename, const std::wstring& pattern) {
    return PathMatchSpecW(filename.c_str(), pattern.c_str()) == TRUE;
}

bool is_excluded(const std::wstring& filename, const Config& cfg) {
    for (const auto& inc : cfg.includes) {
        if (matches_wildcard(filename, inc)) return false;
    }
    for (const auto& exc : cfg.excludes) {
        if (matches_wildcard(filename, exc)) return true;
    }
    return false;
}

// Generate checksum table for existing destination file
std::unordered_multimap<uint32_t, BlockChecksum> generate_dest_checksums(const fs::path& dest_path, size_t block_size) {
    std::unordered_multimap<uint32_t, BlockChecksum> table;
    std::ifstream file(dest_path, std::ios::binary);
    if (!file.is_open()) return table;

    std::vector<uint8_t> buffer(block_size);
    uint64_t offset = 0;

    while (file.read(reinterpret_cast<char*>(buffer.data()), block_size) || file.gcount() > 0) {
        size_t bytes_read = static_cast<size_t>(file.gcount());
        RollingHash r;
        r.init(buffer.data(), bytes_read);

        BlockChecksum bc;
        bc.offset = offset;
        bc.weak_hash = r.get_value();
        bc.strong_hash = fnv1a_64(buffer.data(), bytes_read);

        table.insert({bc.weak_hash, bc});
        offset += bytes_read;
    }
    return table;
}

// Rsync Delta Transfer: Transfers only modified blocks using rolling checksums
bool rsync_delta_copy(const fs::path& src, const fs::path& dest, const Config& cfg) {
    const size_t BLOCK_SIZE = 2048;
    
    // If dest doesn't exist, fall back to direct copy
    if (!fs::exists(dest) || fs::file_size(dest) == 0) {
        if (cfg.dry_run) return true;
        try {
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
            return true;
        } catch (...) { return false; }
    }

    auto checksum_table = generate_dest_checksums(dest, BLOCK_SIZE);

    std::ifstream src_file(src, std::ios::binary);
    std::ifstream dest_file(dest, std::ios::binary);
    
    fs::path temp_dest = dest.wstring() + L".rsync.tmp";
    std::ofstream out_file;
    if (!cfg.dry_run) {
        out_file.open(temp_dest, std::ios::binary);
        if (!out_file.is_open()) return false;
    }

    src_file.seekg(0, std::ios::end);
    uint64_t src_total_size = src_file.tellg();
    src_file.seekg(0, std::ios::beg);

    std::vector<uint8_t> window(BLOCK_SIZE);
    std::vector<uint8_t> unmatched_buffer;

    auto start_time = std::chrono::high_resolution_clock::now();
    uint64_t bytes_processed = 0;

    while (src_file) {
        src_file.read(reinterpret_cast<char*>(window.data()), BLOCK_SIZE);
        size_t window_len = static_cast<size_t>(src_file.gcount());
        if (window_len == 0) break;

        RollingHash r;
        r.init(window.data(), window_len);

        size_t pos = 0;
        while (pos < window_len) {
            uint32_t weak = r.get_value();
            bool match_found = false;
            BlockChecksum matched_block;

            auto range = checksum_table.equal_range(weak);
            for (auto it = range.first; it != range.second; ++it) {
                // Confirm weak match with strong 64-bit FNV-1a hash
                uint64_t strong = fnv1a_64(window.data() + pos, window_len - pos);
                if (it->second.strong_hash == strong) {
                    match_found = true;
                    matched_block = it->second;
                    break;
                }
            }

            if (match_found) {
                // Flush unmatched raw bytes
                if (!unmatched_buffer.empty() && !cfg.dry_run) {
                    out_file.write(reinterpret_cast<char*>(unmatched_buffer.data()), unmatched_buffer.size());
                    unmatched_buffer.clear();
                }

                // Copy matching chunk directly from target destination file
                if (!cfg.dry_run) {
                    dest_file.seekg(matched_block.offset, std::ios::beg);
                    std::vector<uint8_t> block_buf(window_len - pos);
                    dest_file.read(reinterpret_cast<char*>(block_buf.data()), block_buf.size());
                    out_file.write(reinterpret_cast<char*>(block_buf.data()), dest_file.gcount());
                }

                pos += (window_len - pos);
                bytes_processed += (window_len - pos);
            } else {
                unmatched_buffer.push_back(window[pos]);
                pos++;
                bytes_processed++;

                if (pos < window_len) {
                    r.roll(window[pos - 1], window[pos]);
                }
            }

            // Bandwidth Limiting (--bwlimit)
            if (cfg.bwlimit > 0 && bytes_processed % 65536 == 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start_time).count();
                if (elapsed > 0) {
                    uint64_t current_rate_kb = (bytes_processed / 1024) * 1000 / elapsed;
                    if (current_rate_kb > cfg.bwlimit) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            }

            // Progress meter (--progress)
            if (cfg.progress && src_total_size > 0 && bytes_processed % 1048576 == 0) {
                int pct = static_cast<int>((bytes_processed * 100) / src_total_size);
                std::cout << "\r" << dest.filename().string() << " [" << pct << "%] " 
                          << bytes_processed << "/" << src_total_size << " bytes" << std::flush;
            }
        }
    }

    if (!unmatched_buffer.empty() && !cfg.dry_run) {
        out_file.write(reinterpret_cast<char*>(unmatched_buffer.data()), unmatched_buffer.size());
    }

    src_file.close();
    dest_file.close();
    if (out_file.is_open()) out_file.close();

    if (!cfg.dry_run) {
        fs::rename(temp_dest, dest);
    }

    if (cfg.progress) {
        std::cout << "\r" << dest.filename().string() << " [100%] " 
                  << src_total_size << "/" << src_total_size << " bytes\n";
    }

    return true;
}

// Preserve modification times and file attributes (-t, -p)
void preserve_metadata(const fs::path& src, const fs::path& dest, const Config& cfg) {
    if (cfg.dry_run) return;

    if (cfg.preserve_times || cfg.archive) {
        try {
            auto src_time = fs::last_write_time(src);
            fs::last_write_time(dest, src_time);
        } catch (...) {}
    }

    if (cfg.preserve_attrs || cfg.archive) {
        DWORD attrs = GetFileAttributesW(src.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesW(dest.c_str(), attrs);
        }
    }
}

// Check if source file needs to be updated on target
bool should_transfer_file(const fs::path& src, const fs::path& dest, const Config& cfg) {
    if (!fs::exists(dest)) {
        return !cfg.existing_only;
    }

    if (cfg.ignore_existing) return false;
    if (cfg.size_only) return fs::file_size(src) != fs::file_size(dest);

    if (cfg.update) {
        return fs::last_write_time(src) > fs::last_write_time(dest);
    }

    if (cfg.checksum) {
        std::ifstream f1(src, std::ios::binary), f2(dest, std::ios::binary);
        std::vector<uint8_t> b1(65536), b2(65536);
        f1.read(reinterpret_cast<char*>(b1.data()), b1.size());
        f2.read(reinterpret_cast<char*>(b2.data()), b2.size());
        return fnv1a_64(b1.data(), f1.gcount()) != fnv1a_64(b2.data(), f2.gcount());
    }

    // Default: Check size and modification time
    return (fs::file_size(src) != fs::file_size(dest)) || 
           (fs::last_write_time(src) != fs::last_write_time(dest));
}

void sync_directory(const fs::path& src_dir, const fs::path& dest_dir, const Config& cfg) {
    if (!cfg.dry_run && !fs::exists(dest_dir)) {
        fs::create_directories(dest_dir);
    }

    // Delete extraneous files from destination (--delete)
    if (cfg.delete_dest && fs::exists(dest_dir)) {
        for (const auto& entry : fs::directory_iterator(dest_dir)) {
            fs::path rel_path = fs::relative(entry.path(), dest_dir);
            fs::path corresponding_src = src_dir / rel_path;

            if (!fs::exists(corresponding_src)) {
                if (cfg.verbose || cfg.dry_run) {
                    std::cout << "deleting " << entry.path().string() << "\n";
                }
                if (!cfg.dry_run) {
                    fs::remove_all(entry.path());
                }
            }
        }
    }

    // Process source directory entries
    for (const auto& entry : fs::directory_iterator(src_dir)) {
        fs::path src_item = entry.path();
        std::wstring filename = src_item.filename().wstring();

        if (is_excluded(filename, cfg)) continue;

        fs::path dest_item = dest_dir / src_item.filename();

        if (fs::is_directory(src_item)) {
            if (cfg.recursive || cfg.archive) {
                sync_directory(src_item, dest_item, cfg);
            }
        } else if (fs::is_regular_file(src_item)) {
            if (should_transfer_file(src_item, dest_item, cfg)) {
                if (cfg.verbose || cfg.dry_run) {
                    std::cout << src_item.string() << "\n";
                }
                if (rsync_delta_copy(src_item, dest_item, cfg)) {
                    preserve_metadata(src_item, dest_item, cfg);
                }
            }
        }
    }
}

int wmain(int argc, wchar_t* argv[]) {
    Config cfg;
    std::vector<std::wstring> positional;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help") {
            print_usage("rsync");
            return 0;
        } else if (arg == L"-r" || arg == L"--recursive") cfg.recursive = true;
        else if (arg == L"-a" || arg == L"--archive") { cfg.archive = true; cfg.recursive = true; cfg.preserve_times = true; cfg.preserve_attrs = true; }
        else if (arg == L"-v" || arg == L"--verbose") cfg.verbose = true;
        else if (arg == L"-q" || arg == L"--quiet") cfg.quiet = true;
        else if (arg == L"-n" || arg == L"--dry-run") cfg.dry_run = true;
        else if (arg == L"-u" || arg == L"--update") cfg.update = true;
        else if (arg == L"-c" || arg == L"--checksum") cfg.checksum = true;
        else if (arg == L"-t" || arg == L"--times") cfg.preserve_times = true;
        else if (arg == L"-p" || arg == L"--perms") cfg.preserve_attrs = true;
        else if (arg == L"--delete") cfg.delete_dest = true;
        else if (arg == L"--size-only") cfg.size_only = true;
        else if (arg == L"--existing") cfg.existing_only = true;
        else if (arg == L"--ignore-existing") cfg.ignore_existing = true;
        else if (arg == L"--progress") cfg.progress = true;
        else if (arg.rfind(L"--bwlimit=", 0) == 0) cfg.bwlimit = std::stoull(arg.substr(10));
        else if (arg.rfind(L"--exclude=", 0) == 0) cfg.excludes.push_back(arg.substr(10));
        else if (arg.rfind(L"--include=", 0) == 0) cfg.includes.push_back(arg.substr(10));
        else if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"rsync: unknown option '" << arg << L"'\n";
            return 1;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.size() < 2) {
        std::cerr << "rsync: missing destination file/directory operand\n";
        std::cerr << "Try 'rsync --help' for more information.\n";
        return 1;
    }

    cfg.src_path = positional[0];
    cfg.dest_path = positional[1];

    fs::path src(cfg.src_path);
    fs::path dest(cfg.dest_path);

    if (!fs::exists(src)) {
        std::wcerr << L"rsync: link_stat \"" << cfg.src_path << L"\" failed: No such file or directory\n";
        return 1;
    }

    // Support Rsync trailing slash semantics: "dir/" vs "dir"
    bool src_has_trailing_slash = (cfg.src_path.back() == L'/' || cfg.src_path.back() == L'\\');

    if (fs::is_directory(src)) {
        fs::path target_dest = dest;
        if (!src_has_trailing_slash && fs::exists(dest)) {
            target_dest = dest / src.filename();
        }
        sync_directory(src, target_dest, cfg);
    } else {
        fs::path target_dest = dest;
        if (fs::exists(dest) && fs::is_directory(dest)) {
            target_dest = dest / src.filename();
        }

        if (should_transfer_file(src, target_dest, cfg)) {
            if (cfg.verbose || cfg.dry_run) {
                std::cout << src.string() << "\n";
            }
            if (rsync_delta_copy(src, target_dest, cfg)) {
                preserve_metadata(src, target_dest, cfg);
            }
        }
    }

    return 0;
}