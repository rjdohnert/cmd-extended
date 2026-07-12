#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

// Automatically link Shell32.lib for ShellExecuteEx
#pragma comment(lib, "Shell32.lib")

// Version: 2.0

void PrintUsage() {
    std::cout << "Usage: sudo [option]\n";
    std::cout << "Options:\n";
    std::cout << "  (no option)   Launches Administrator Command Prompt (cmd.exe)\n";
    std::cout << "  -cmd          Launches Administrator Command Prompt (cmd.exe)\n";
    std::cout << "  -ps           Launches Administrator Windows PowerShell (powershell.exe)\n";
    std::cout << "  -pwsh         Launches Administrator PowerShell 7 (pwsh.exe)\n";
}

int main(int argc, char* argv[]) {
    std::string targetShell = "cmd.exe"; // Default target

    // Parse command line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        // Convert argument to lowercase
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        if (arg == "-h" || arg == "--help") {
            PrintUsage();
            return 0;
        } else if (arg == "-cmd" || arg == "cmd" || arg == "cmd.exe") {
            targetShell = "cmd.exe";
        } else if (arg == "-ps" || arg == "powershell" || arg == "powershell.exe") {
            targetShell = "powershell.exe";
        } else if (arg == "-pwsh" || arg == "pwsh" || arg == "pwsh.exe") {
            targetShell = "pwsh.exe";
        } else {
            std::cout << "Unknown option: " << argv[1] << "\n";
            PrintUsage();
            return 1;
        }
    }

    // Get the current directory so the elevated terminal starts in the same folder
    char currentDirectory[MAX_PATH];
    DWORD dirLen = GetCurrentDirectoryA(MAX_PATH, currentDirectory);
    const char* workingDir = (dirLen > 0 && dirLen < MAX_PATH) ? currentDirectory : nullptr;

    // Configure the shell execution parameters
    SHELLEXECUTEINFOA sei = { sizeof(SHELLEXECUTEINFOA) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = "runas";            // The "runas" verb triggers the UAC elevation prompt
    sei.lpFile = targetShell.c_str();
    sei.lpParameters = nullptr;      // No initial parameters passed to the target shell
    sei.lpDirectory = workingDir;    // Set target working directory to current directory
    sei.nShow = SW_SHOWNORMAL;       // Display window normally

    std::cout << "Requesting administrator elevation for " << targetShell << "...\n";

    // Launch the elevated process
    if (ShellExecuteExA(&sei)) {
        std::cout << "Elevated terminal launched successfully.\n";
        // Close the handle to the process as we do not need to track it
        if (sei.hProcess != NULL) {
            CloseHandle(sei.hProcess);
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            // Error code 1223: The user cancelled the UAC prompt
            std::cout << "Elevation request was denied by the user.\n";
        } else {
            std::cout << "Failed to launch elevated terminal. Error code: " << error << "\n";
        }
        return 1;
    }

    return 0;
}