#include <windows.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>

std::string GetCurrentDirectoryString() {
    DWORD length = GetCurrentDirectoryA(0, NULL);
    if (length == 0) {
        return ".";
    }

    std::string path(length, '\0');
    DWORD written = GetCurrentDirectoryA(length, &path[0]);
    if (written == 0) {
        return ".";
    }

    path.resize(written);
    return path;
}

std::string JoinPath(const std::string& base, const std::string& child) {
    if (base.empty()) {
        return child;
    }
    if (base[base.length() - 1] == '\\' || base[base.length() - 1] == '/') {
        return base + child;
    }
    return base + "\\" + child;
}

std::string NormalizePath(const std::string& base, const std::string& input) {
    std::string combined = input;
    if (!(input.length() >= 2 && input[1] == ':') && !(input.length() >= 2 && input[0] == '\\' && input[1] == '\\')) {
        combined = JoinPath(base, input);
    }

    char buffer[MAX_PATH];
    DWORD result = GetFullPathNameA(combined.c_str(), MAX_PATH, buffer, NULL);
    if (result == 0 || result >= MAX_PATH) {
        return combined;
    }
    return std::string(buffer);
}

bool PathExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectoryPath(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
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

        char rootPath[] = "A:\\";
        rootPath[0] = static_cast<char>('A' + index);

        const char* driveTypeName = "Unknown";
        switch (GetDriveTypeA(rootPath)) {
            case DRIVE_FIXED: driveTypeName = "Fixed"; break;
            case DRIVE_REMOVABLE: driveTypeName = "Removable"; break;
            case DRIVE_CDROM: driveTypeName = "CD-ROM"; break;
            case DRIVE_REMOTE: driveTypeName = "Network"; break;
            case DRIVE_RAMDISK: driveTypeName = "RAM Disk"; break;
            case DRIVE_NO_ROOT_DIR: driveTypeName = "Invalid"; break;
        }

        char volumeName[MAX_PATH] = "";
        char fileSystemName[MAX_PATH] = "";
        if (!GetVolumeInformationA(rootPath, volumeName, MAX_PATH, NULL, NULL, NULL, fileSystemName, MAX_PATH)) {
            volumeName[0] = '\0';
            fileSystemName[0] = '\0';
        }

        std::cout << std::left << std::setw(10) << rootPath
                  << std::setw(18) << driveTypeName
                  << std::setw(14) << (fileSystemName[0] != '\0' ? fileSystemName : "-")
                  << (volumeName[0] != '\0' ? volumeName : "-") << "\n";
    }

    std::cout << "----------------------------------------------------------------------\n";
}

void ListDirectory(const std::string& path) {
    std::cout << "\nContents of " << path << ":\n";
    std::cout << "---------------------------------------------------------------\n";
    std::cout << std::left << std::setw(8) << "Type"
              << std::setw(36) << "Name"
              << std::right << std::setw(16) << "Size(MB)\n";
    std::cout << "---------------------------------------------------------------\n";

    WIN32_FIND_DATAA findData;
    HANDLE findHandle = FindFirstFileA(JoinPath(path, "*").c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        std::cout << "Error reading directory.\n";
        std::cout << "---------------------------------------------------------------\n";
        return;
    }

    bool foundAny = false;
    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        foundAny = true;
        std::string name = findData.cFileName;
        if (name.length() > 35) {
            name = name.substr(0, 32) + "...";
        }

        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::cout << std::left << std::setw(8) << "DIR"
                      << std::setw(36) << name
                      << std::right << std::setw(16) << "--\n";
        } else {
            unsigned long long size = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
            double sizeMb = static_cast<double>(size) / (1024.0 * 1024.0);
            std::ostringstream sizeText;
            sizeText << std::fixed << std::setprecision(2) << sizeMb;
            std::cout << std::left << std::setw(8) << "FILE"
                      << std::setw(36) << name
                      << std::right << std::setw(16) << sizeText.str() << "\n";
        }
    } while (FindNextFileA(findHandle, &findData) != 0);

    FindClose(findHandle);

    if (!foundAny) {
        std::cout << "  <Directory is empty>\n";
    }

    std::cout << "---------------------------------------------------------------\n";
}

int main() {
    std::string current_path = GetCurrentDirectoryString();
    std::string input;
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);

    std::cout << "\n";
    std::cout << "File Manager\n";
    std::cout << "\n";
    std::cout << "Copyright (C) 2026, PC/OpenSystems LLC\n";
    std::cout << "All rights reserved.\n";
    std::cout << "\n";
    std::cout << "Type 'help' to see the available commands.\n";
    std::cout << "\n";
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
        std::cout << "\n" << current_path << " > ";
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
        else if (command == "ls" || command == "dir") {
            ListDirectory(current_path);
        } 
        else if (command == "cd") {
            std::string arg;
            std::getline(ss, arg);
            
            // Trim leading spaces
            if (!arg.empty() && arg[0] == ' ') {
                arg = arg.substr(1);
            }

            if (arg.empty()) {
                std::cout << "Error: Directory path required.\n";
                continue;
            }

            // Evaluate '.' and '..' relative to the current path syntactically
            std::string target_path = NormalizePath(current_path, arg);

            if (PathExists(target_path) && IsDirectoryPath(target_path)) {
                current_path = target_path;
            } else {
                std::cout << "Error: Directory does not exist: " << arg << "\n";
            }
        } 
        else if (command == "mkdir") {
            std::string arg;
            std::getline(ss, arg);
            
            if (!arg.empty() && arg[0] == ' ') {
                arg = arg.substr(1);
            }

            if (arg.empty()) {
                std::cout << "Error: Directory name required.\n";
                continue;
            }

            std::string new_dir = JoinPath(current_path, arg);
            if (CreateDirectoryA(new_dir.c_str(), NULL) != 0) {
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

            if (!arg.empty() && arg[0] == ' ') {
                arg = arg.substr(1);
            }

            if (arg.empty()) {
                std::cout << "Error: Directory name required.\n";
                continue;
            }

            std::string target_dir = NormalizePath(current_path, arg);
            if (!PathExists(target_dir)) {
                std::cout << "Error: Directory does not exist: " << arg << "\n";
            } else if (!IsDirectoryPath(target_dir)) {
                std::cout << "Error: Target is not a directory: " << arg << "\n";
            } else if (RemoveDirectoryA(target_dir.c_str()) != 0) {
                std::cout << "Directory removed successfully: " << arg << "\n";
            } else {
                std::cout << "Error removing directory (code " << GetLastError() << ").\n";
            }
        }
        else if (command == "rm") {
            std::string arg;
            std::getline(ss, arg);

            if (!arg.empty() && arg[0] == ' ') {
                arg = arg.substr(1);
            }

            if (arg.empty()) {
                std::cout << "Error: File name required.\n";
                continue;
            }

            std::string target_file = NormalizePath(current_path, arg);
            if (!PathExists(target_file)) {
                std::cout << "Error: File does not exist: " << arg << "\n";
            } else if (IsDirectoryPath(target_file)) {
                std::cout << "Error: Target is a directory. Use rmdir instead.\n";
            } else if (DeleteFileA(target_file.c_str()) != 0) {
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

            std::string sourcePath = NormalizePath(current_path, sourceArg);
            std::string destinationPath = NormalizePath(current_path, destinationArg);

            if (!PathExists(sourcePath)) {
                std::cout << "Error: Source file does not exist: " << sourceArg << "\n";
            } else if (IsDirectoryPath(sourcePath)) {
                std::cout << "Error: Source path is a directory. copy supports files only.\n";
            } else if (CopyFileA(sourcePath.c_str(), destinationPath.c_str(), TRUE) != 0) {
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

            std::string sourcePath = NormalizePath(current_path, sourceArg);
            std::string destinationPath = NormalizePath(current_path, destinationArg);

            if (!PathExists(sourcePath)) {
                std::cout << "Error: Source file does not exist: " << sourceArg << "\n";
            } else if (IsDirectoryPath(sourcePath)) {
                std::cout << "Error: Source path is a directory. move supports files only.\n";
            } else if (MoveFileA(sourcePath.c_str(), destinationPath.c_str()) != 0) {
                std::cout << "File moved successfully.\n";
            } else {
                std::cout << "Error moving file (code " << GetLastError() << ").\n";
            }
        }
        else if (command == "help") {
            std::cout << "Available Commands:\n"
                      << "  drive             - List all available drives\n"
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