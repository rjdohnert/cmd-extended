#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cstdlib>

namespace fs = std::filesystem;

struct ZipOptions {
    bool recursive = false;
    bool junk_paths = false;
    bool quiet = false;
    int compression_level = 6;
    std::string zip_filename;
    std::vector<std::string> input_paths;
};

std::string escape_ps_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\'') {
            out += "''";
        } else {
            out += c;
        }
    }
    return out;
}

std::string ps_quote(const std::string& s) {
    return "'" + escape_ps_single_quotes(s) + "'";
}

std::string compression_level_to_ps(int level) {
    if (level <= 0) return "NoCompression";
    if (level <= 3) return "Fastest";
    return "Optimal";
}

bool is_zip_ext(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".zip";
}

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [-r] [-j] [-q] [-0..9] archive.zip [file1 dir1 ...]\n\n"
              << "Options:\n"
              << "  -r       Recurse into directories\n"
              << "  -j       Junk (don't record) directory names (store only file names)\n"
              << "  -q       Quiet mode\n"
              << "  -0..-9   Set compression level (0=store, 9=best, default=6)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    ZipOptions opts;
    bool parsing_flags = true;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (parsing_flags && arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                if (c == 'r') opts.recursive = true;
                else if (c == 'j') opts.junk_paths = true;
                else if (c == 'q') opts.quiet = true;
                else if (c >= '0' && c <= '9') opts.compression_level = c - '0';
                else if (c == '-') { parsing_flags = false; break; }
                else {
                    std::cerr << "zip error: Invalid option -" << c << "\n";
                    return 1;
                }
            }
        } else {
            if (opts.zip_filename.empty()) {
                opts.zip_filename = arg;
                // Auto-append .zip extension if missing
                if (!is_zip_ext(fs::path(opts.zip_filename))) {
                    opts.zip_filename += ".zip";
                }
            } else {
                opts.input_paths.push_back(arg);
            }
        }
    }

    if (opts.zip_filename.empty() || opts.input_paths.empty()) {
        std::cerr << "zip error: Nothing to do! (must specify zipfile and input files)\n";
        return 1;
    }

    std::vector<fs::path> archive_inputs;
    std::vector<fs::path> files_for_junk_mode;
    size_t files_added = 0;

    for (const auto& input_str : opts.input_paths) {
        fs::path p(input_str);

        if (!fs::exists(p)) {
            std::cerr << "zip warning: name not matched: " << input_str << "\n";
            continue;
        }

        if (fs::is_directory(p)) {
            if (!opts.recursive) {
                std::cerr << "zip warning: " << input_str << " is a directory (use -r to recurse)\n";
                continue;
            }

            archive_inputs.push_back(p);
            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_regular_file()) {
                    files_added++;
                    files_for_junk_mode.push_back(entry.path());
                }
            }
            continue;
        }

        if (fs::is_regular_file(p)) {
            archive_inputs.push_back(p);
            files_for_junk_mode.push_back(p);
            files_added++;
        }
    }

    if (archive_inputs.empty() || files_added == 0) {
        std::cerr << "zip error: Nothing to do!\n";
        return 1;
    }

    fs::path temp_stage_dir;
    std::vector<fs::path> ps_inputs;

    if (opts.junk_paths) {
        temp_stage_dir = fs::temp_directory_path() /
            (std::string("cmdext-zip-stage-") + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));

        try {
            fs::create_directories(temp_stage_dir);

            for (const auto& src : files_for_junk_mode) {
                fs::path candidate = temp_stage_dir / src.filename();
                int idx = 1;
                while (fs::exists(candidate)) {
                    candidate = temp_stage_dir /
                        (src.stem().string() + "_" + std::to_string(idx++) + src.extension().string());
                }
                fs::copy_file(src, candidate, fs::copy_options::overwrite_existing);
                ps_inputs.push_back(candidate);

                if (!opts.quiet) {
                    std::cout << "  adding: " << candidate.filename().string();
                    if (opts.compression_level == 0) {
                        std::cout << " (stored)\n";
                    } else {
                        std::cout << " (deflated)\n";
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "zip error: Failed preparing junk-path staging area: " << ex.what() << "\n";
            if (!temp_stage_dir.empty()) {
                std::error_code ec;
                fs::remove_all(temp_stage_dir, ec);
            }
            return 1;
        }
    } else {
        ps_inputs = archive_inputs;
        if (!opts.quiet) {
            for (const auto& in : ps_inputs) {
                std::cout << "  adding: " << in.string();
                if (opts.compression_level == 0) {
                    std::cout << " (stored)\n";
                } else {
                    std::cout << " (deflated)\n";
                }
            }
        }
    }

    std::ostringstream path_array;
    path_array << "@(";
    for (size_t i = 0; i < ps_inputs.size(); ++i) {
        if (i != 0) path_array << ",";
        path_array << ps_quote(ps_inputs[i].string());
    }
    path_array << ")";

    std::string psCommand =
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ErrorActionPreference='Stop'; "
        "Compress-Archive -Path " + path_array.str() +
        " -DestinationPath " + ps_quote(fs::absolute(opts.zip_filename).string()) +
        " -CompressionLevel " + compression_level_to_ps(opts.compression_level) +
        " -Force\"";

    int rc = std::system(psCommand.c_str());

    if (!temp_stage_dir.empty()) {
        std::error_code ec;
        fs::remove_all(temp_stage_dir, ec);
    }

    if (rc != 0) {
        std::cerr << "zip error: Failed to create archive via PowerShell Compress-Archive.\n";
        return 1;
    }

    if (!opts.quiet) {
        std::cout << "Created " << opts.zip_filename << " (" << files_added << " file(s) added)\n";
    }

    return 0;
}