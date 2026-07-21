#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cwchar>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

struct TeeOptions {
    bool append = false;
    bool ignore_interrupts = false;
};

int wmain(int argc, wchar_t* argv[]) {
    // Optimize standard streams performance for faster pipes
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    TeeOptions opts;
    std::vector<std::wstring> file_paths;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'a':
                        opts.append = true;
                        break;
                    case L'i':
                        opts.ignore_interrupts = true;
                        break;
                    default:
                        std::wcerr << L"tee: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: tee [-ai] [file ...]\n";
                        return 1;
                }
            }
        } else {
            file_paths.push_back(arg);
        }
    }

    // Configure standard streams to binary mode on Windows to prevent translation issues
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Ignore CTRL+C signals if -i is requested
    if (opts.ignore_interrupts) {
#ifdef _WIN32
        SetConsoleCtrlHandler(nullptr, TRUE);
#endif
    }

    // Open target files
    std::vector<std::ofstream> streams;
    std::ios_base::openmode mode = std::ios::out | std::ios::binary;
    if (opts.append) {
        mode |= std::ios::app;
    }

    for (const auto& path_str : file_paths) {
        fs::path p(path_str);
        std::ofstream stream(p, mode);
        if (!stream) {
            std::wcerr << L"tee: " << path_str << L": Failed to open file\n";
        } else {
            streams.push_back(std::move(stream));
        }
    }

    // Process stdin and duplicate to stdout/files in blocks
    char buffer[16384];
    while (std::cin) {
        std::cin.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = std::cin.gcount();
        if (bytes_read > 0) {
            std::cout.write(buffer, bytes_read);
            std::cout.flush(); // Flush stdout to support live pipe streaming

            for (auto& fs : streams) {
                if (fs.is_open()) {
                    fs.write(buffer, bytes_read);
                    fs.flush(); // Flush files to ensure data is written immediately
                }
            }
        }
    }

    return 0;
}