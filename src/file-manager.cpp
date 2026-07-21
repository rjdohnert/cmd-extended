#include <windows.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <winnetwk.h>

#pragma comment(lib, "mpr.lib")

std::wstring ToWide(const std::string& input) {
    if (input.empty()) {
        return L"";
    }

    int needed = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, nullptr, 0);
    if (needed <= 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(needed));
    if (MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, buffer.data(), needed) <= 0) {
        return L"";
    }

    return std::wstring(buffer.data());
}

std::string ToNarrow(const std::wstring& input) {
    if (input.empty()) {
        return "";
    }

    int needed = WideCharToMultiByte(CP_ACP, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return "";
    }

    std::vector<char> buffer(static_cast<size_t>(needed));
    if (WideCharToMultiByte(CP_ACP, 0, input.c_str(), -1, buffer.data(), needed, nullptr, nullptr) <= 0) {
        return "";
    }

    return std::string(buffer.data());
}

std::wstring GetCurrentDirectoryString() {
    DWORD length = GetCurrentDirectoryW(0, NULL);
    if (length == 0) {
        return L".";
    }

    std::wstring path(length, L'\0');
    DWORD written = GetCurrentDirectoryW(length, &path[0]);
    if (written == 0) {
        return L".";
    }

    path.resize(written);
    return path;
}

std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    if (base.empty()) {
        return child;
    }
    if (base[base.length() - 1] == L'\\' || base[base.length() - 1] == L'/') {
        return base + child;
    }
    return base + L"\\" + child;
}

std::wstring NormalizePath(const std::wstring& base, const std::wstring& input) {
    std::wstring combined = input;
    if (!(input.length() >= 2 && input[1] == L':') && !(input.length() >= 2 && input[0] == L'\\' && input[1] == L'\\')) {
        combined = JoinPath(base, input);
    }

    DWORD result = GetFullPathNameW(combined.c_str(), 0, nullptr, nullptr);
    if (result == 0) {
        return combined;
    }

    std::wstring fullPath(result, L'\0');
    DWORD written = GetFullPathNameW(combined.c_str(), result, &fullPath[0], nullptr);
    if (written == 0 || written >= result) {
        return combined;
    }

    fullPath.resize(written);
    return fullPath;
}

bool PathExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectoryPath(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::string GetWeekdayName(WORD dayOfWeek) {
    static const char* kWeekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };

    if (dayOfWeek > 6) {
        return "Unknown";
    }
    return kWeekdays[dayOfWeek];
}

void ListDrives() {
    DWORD driveMask = GetLogicalDrives();
    if (driveMask == 0) {
        std::cout << "Error listing drives (code " << GetLastError() << ").\n";
        return;
    }

    std::cout << "\nAvailable drives:\n";
    std::cout << "----------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(10) << "Drive"
              << std::setw(18) << "Type"
              << std::setw(14) << "Filesystem"
              << "Label\n";
    std::cout << "----------------------------------------------------------------------\n";

    for (int index = 0; index < 26; ++index) {
        if ((driveMask & (1u << index)) == 0) {
            continue;
        }

        wchar_t rootPath[] = L"A:\\";
        rootPath[0] = static_cast<wchar_t>(L'A' + index);

        const char* driveTypeName = "Unknown";
        switch (GetDriveTypeW(rootPath)) {
            case DRIVE_FIXED: driveTypeName = "Fixed"; break;
            case DRIVE_REMOVABLE: driveTypeName = "Removable"; break;
            case DRIVE_CDROM: driveTypeName = "CD-ROM"; break;
            case DRIVE_REMOTE: driveTypeName = "Network"; break;
            case DRIVE_RAMDISK: driveTypeName = "RAM Disk"; break;
            case DRIVE_NO_ROOT_DIR: driveTypeName = "Invalid"; break;
        }

        wchar_t volumeName[MAX_PATH] = L"";
        wchar_t fileSystemName[MAX_PATH] = L"";
        if (!GetVolumeInformationW(rootPath, volumeName, MAX_PATH, NULL, NULL,
                                   NULL, fileSystemName, MAX_PATH)) {
            volumeName[0] = L'\0';
            fileSystemName[0] = L'\0';
        }

        std::cout << std::left << std::setw(10) << ToNarrow(rootPath)
                  << std::setw(18) << driveTypeName
                  << std::setw(14)
                  << (fileSystemName[0] != L'\0' ? ToNarrow(fileSystemName)
                                                   : "-")
                  << (volumeName[0] != L'\0' ? ToNarrow(volumeName) : "-")
                  << "\n";
    }

    std::cout << "----------------------------------------------------------------------\n";
}

void ListNetworkShares() {
    std::cout << "\nAvailable Network Shares:\n";
    std::cout
        << "----------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(40) << "Share Path" << "Type\n";
    std::cout
        << "----------------------------------------------------------------------\n";

    HANDLE hEnum = NULL;
    NETRESOURCEW nr = {0};
    nr.dwType = RESOURCETYPE_DISK;
    nr.dwScope = RESOURCE_GLOBALNET;
    nr.dwUsage = RESOURCEUSAGE_CONNECTABLE;

    DWORD dwResult =
        WNetOpenEnumW(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, RESOURCEUSAGE_ALL,
                      NULL, &hEnum);
    if (dwResult != NO_ERROR) {
        std::cout << "Unable to enumerate network resources. Error: " << dwResult
                  << "\n";
        std::cout
            << "----------------------------------------------------------------------\n";
        return;
    }

    DWORD cbBuffer = 16384;
    LPNETRESOURCEW lpnrs =
        (LPNETRESOURCEW)GlobalAlloc(GPTR, cbBuffer);
    if (lpnrs == NULL) {
        WNetCloseEnum(hEnum);
        std::cout << "Memory allocation failed.\n";
        return;
    }

    bool foundShares = false;
    do {
        DWORD cEntries = (DWORD)-1;
        dwResult = WNetEnumResourceW(hEnum, &cEntries, lpnrs, &cbBuffer);
        if (dwResult == NO_ERROR) {
            for (DWORD i = 0; i < cEntries; ++i) {
                foundShares = true;
                std::string remotePath = "";
                std::string remoteType = "";

                if (lpnrs[i].lpRemoteName) {
                    remotePath = ToNarrow(lpnrs[i].lpRemoteName);
                }

                switch (lpnrs[i].dwType) {
                    case RESOURCETYPE_DISK:
                        remoteType = "Disk";
                        break;
                    case RESOURCETYPE_PRINT:
                        remoteType = "Printer";
                        break;
                    default:
                        remoteType = "Other";
                        break;
                }

                std::cout << std::left << std::setw(40) << remotePath
                          << remoteType << "\n";
            }
        } else if (dwResult != ERROR_NO_MORE_ITEMS) {
            break;
        }
    } while (dwResult == NO_ERROR);

    GlobalFree((HGLOBAL)lpnrs);
    WNetCloseEnum(hEnum);

    if (!foundShares) {
        std::cout << "  <No network shares available>\n";
    }

    std::cout
        << "----------------------------------------------------------------------\n";
}

void ListDirectory(const std::wstring& path) {
    std::wcout << L"\nContents of " << path << L":\n";
    std::wcout << L"---------------------------------------------------------------\n";
    std::wcout << std::left << std::setw(8) << L"Type"
               << std::setw(36) << L"Name"
               << std::right << std::setw(16) << L"Size(MB)\n";
    std::wcout << L"---------------------------------------------------------------\n";

    WIN32_FIND_DATAW findData;
    HANDLE findHandle = FindFirstFileW(JoinPath(path, L"*").c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        std::wcout << L"Error reading directory.\n";
        std::wcout << L"---------------------------------------------------------------\n";
        return;
    }

    bool foundAny = false;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        foundAny = true;
        std::wstring name = findData.cFileName;
        if (name.length() > 35) {
            name = name.substr(0, 32) + L"...";
        }

        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::wcout << std::left << std::setw(8) << L"DIR"
                       << std::setw(36) << name
                       << std::right << std::setw(16) << L"--\n";
        } else {
            unsigned long long size = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
            double sizeMb = static_cast<double>(size) / (1024.0 * 1024.0);
            std::wstringstream sizeText;
            sizeText << std::fixed << std::setprecision(2) << sizeMb;
            std::wcout << std::left << std::setw(8) << L"FILE"
                       << std::setw(36) << name
                       << std::right << std::setw(16) << sizeText.str() << L"\n";
        }
    } while (FindNextFileW(findHandle, &findData) != 0);

    FindClose(findHandle);

    if (!foundAny) {
        std::wcout << L"  <Directory is empty>\n";
    }

    std::wcout << L"---------------------------------------------------------------\n";
}

std::string TrimLeft(std::string value) {
    while (!value.empty() && value[0] == ' ') {
        value.erase(value.begin());
    }
    return value;
}

int main() {
    std::wstring current_path = GetCurrentDirectoryString();
    std::string input;
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);

    std::cout << "\n";
    std::cout << "File Manager\n";
    std::cout << "-------------\n\n";
    std::cout << "Copyright (C) 2026, PC/OpenSystems LLC\n";
    std::cout << "All rights reserved.\n\n";
    std::cout << "Type 'help' to see the available commands.\n\n";
    std::cout << "Date: "
              << std::setfill('0') << std::setw(2) << localTime.wMonth << "/"
              << std::setw(2) << localTime.wDay << "/"
              << std::setw(4) << localTime.wYear
              << "  Time: "
              << std::setw(2) << localTime.wHour << ":"
              << std::setw(2) << localTime.wMinute << ":"
              << std::setw(2) << localTime.wSecond
              << "  Day: " << GetWeekdayName(localTime.wDayOfWeek) << "\n";
    std::cout << std::setfill(' ');

    while (true) {
        std::cout << "\n" << ToNarrow(current_path) << " > ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        std::stringstream ss(input);
        std::string command;
        ss >> command;

        if (command == "exit" || command == "quit") {
            break;
        }
        else if (command == "drive") {
            ListDrives();
        }
        else if (command == "shares" || command == "net") {
            ListNetworkShares();
        }
        else if (command == "ls" || command == "dir") {
            ListDirectory(current_path);
        }
        else if (command == "cd") {
            std::string arg;
            std::getline(ss, arg);
            arg = TrimLeft(arg);

            if (arg.empty()) {
                std::cout << "Error: Directory path required.\n";
                continue;
            }

            std::wstring target_path = NormalizePath(current_path, ToWide(arg));

            if (PathExists(target_path) && IsDirectoryPath(target_path)) {
                current_path = target_path;
            } else {
                std::cout << "Error: Directory does not exist: " << arg << "\n";
            }
        }
        else if (command == "mkdir") {
            std::string arg;
            std::getline(ss, arg);
            arg = TrimLeft(arg);

            if (arg.empty()) {
                std::cout << "Error: Directory name required.\n";
                continue;
            }

            std::wstring newDir = JoinPath(current_path, ToWide(arg));
            if (CreateDirectoryW(newDir.c_str(), NULL) != 0) {
                std::cout << "Directory created successfully: " << arg << "\n";
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_ALREADY_EXISTS) {
                    std::cout << "Error: Directory already exists or creation failed.\n";
                } else {
                    std::cout << "Error creating directory (code " << err << ").\n";
                }
            }
        }
        else if (command == "rmdir") {
            std::string arg;
            std::getline(ss, arg);
            arg = TrimLeft(arg);

            if (arg.empty()) {
                std::cout << "Error: Directory name required.\n";
                continue;
            }

            std::wstring targetDir = NormalizePath(current_path, ToWide(arg));
            if (!PathExists(targetDir)) {
                std::cout << "Error: Directory does not exist: " << arg << "\n";
            } else if (!IsDirectoryPath(targetDir)) {
                std::cout << "Error: Target is not a directory: " << arg << "\n";
            } else if (RemoveDirectoryW(targetDir.c_str()) != 0) {
                std::cout << "Directory removed successfully: " << arg << "\n";
            } else {
                std::cout << "Error removing directory (code " << GetLastError() << ").\n";
            }
        }
        else if (command == "rm") {
            std::string arg;
            std::getline(ss, arg);
            arg = TrimLeft(arg);

            if (arg.empty()) {
                std::cout << "Error: File name required.\n";
                continue;
            }

            std::wstring targetFile = NormalizePath(current_path, ToWide(arg));
            if (!PathExists(targetFile)) {
                std::cout << "Error: File does not exist: " << arg << "\n";
            } else if (IsDirectoryPath(targetFile)) {
                std::cout << "Error: Target is a directory. Use rmdir instead.\n";
            } else if (DeleteFileW(targetFile.c_str()) != 0) {
                std::cout << "File removed successfully: " << arg << "\n";
            } else {
                std::cout << "Error removing file (code " << GetLastError() << ").\n";
            }
        }
        else if (command == "copy") {
            std::string sourceArg;
            std::string destinationArg;
            ss >> sourceArg >> destinationArg;

            if (sourceArg.empty() || destinationArg.empty()) {
                std::cout << "Usage: copy <source_file> <destination_file>\n";
                continue;
            }

            std::wstring sourcePath = NormalizePath(current_path, ToWide(sourceArg));
            std::wstring destinationPath = NormalizePath(current_path, ToWide(destinationArg));

            if (!PathExists(sourcePath)) {
                std::cout << "Error: Source file does not exist: " << sourceArg << "\n";
            } else if (IsDirectoryPath(sourcePath)) {
                std::cout << "Error: Source path is a directory. copy supports files only.\n";
            } else if (CopyFileW(sourcePath.c_str(), destinationPath.c_str(), TRUE) != 0) {
                std::cout << "File copied successfully.\n";
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {
                    std::cout << "Error: Destination file already exists.\n";
                } else {
                    std::cout << "Error copying file (code " << err << ").\n";
                }
            }
        }
        else if (command == "move") {
            std::string sourceArg;
            std::string destinationArg;
            ss >> sourceArg >> destinationArg;

            if (sourceArg.empty() || destinationArg.empty()) {
                std::cout << "Usage: move <source_file> <destination_file>\n";
                continue;
            }

            std::wstring sourcePath = NormalizePath(current_path, ToWide(sourceArg));
            std::wstring destinationPath = NormalizePath(current_path, ToWide(destinationArg));

            if (!PathExists(sourcePath)) {
                std::cout << "Error: Source file does not exist: " << sourceArg << "\n";
            } else if (IsDirectoryPath(sourcePath)) {
                std::cout << "Error: Source path is a directory. move supports files only.\n";
            } else if (MoveFileW(sourcePath.c_str(), destinationPath.c_str()) != 0) {
                std::cout << "File moved successfully.\n";
            } else {
                std::cout << "Error moving file (code " << GetLastError() << ").\n";
            }
        }
        else if (command == "help") {
            std::cout << "Available Commands:\n"
                      << "  drive             - List all available drives\n"
                      << "  shares            - List available network shares\n"
                      << "  ls                - List files and folders in the current directory\n"
                      << "  cd <directory>    - Move to a different directory (use '..' to go back)\n"
                      << "  mkdir <name>      - Create a new folder in the current directory\n"
                      << "  rmdir <name>      - Remove an empty folder from the current directory\n"
                      << "  rm <name>         - Remove a file from the current directory\n"
                      << "  copy <src> <dst>  - Copy a file to a new path\n"
                      << "  move <src> <dst>  - Move/rename a file to a new path\n"
                      << "  exit / quit       - Terminate the program\n";
        }
        else if (!command.empty()) {
            std::cout << "Unknown command: '" << command << "'. Type 'help' for options.\n";
        }
    }

    return 0;
}
