#include <iostream>
#include <string>
#include <algorithm>
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

// Helper function to convert input to uppercase
std::string to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

// Windows requires processes to explicitly enable the shutdown privilege
bool EnableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0)) {
        CloseHandle(hToken);
        return false;
    }

    if (GetLastError() != ERROR_SUCCESS) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;

    if (AllocateAndInitializeSid(
            &ntAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0,
            0,
            0,
            0,
            0,
            0,
            &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

bool RelaunchElevated() {
    char exePath[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        return false;
    }

    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = "runas";
    sei.lpFile = exePath;
    sei.lpParameters = "--elevated";
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) {
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    // Set the command prompt window title
    SetConsoleTitleA("System Reboot");

    bool elevatedRun = false;
    if (argc > 1 && std::string(argv[1]) == "--elevated") {
        elevatedRun = true;
    }

    std::cout << "\n";
    std::cout << " WARNING: You are about to reboot this System.\n";
    std::cout << "\n";

    std::cout << "Are you sure you want to proceed? (Y/N): ";
    std::string input;
    std::cin >> input;

    input = to_upper(input);

    if (input == "Y" || input == "YES") {
        if (!IsRunningAsAdmin() && !elevatedRun) {
            std::cout << "\nAdministrator rights are required to reboot this system." << std::endl;
            std::cout << "Showing UAC prompt for admin approval..." << std::endl;

            if (RelaunchElevated()) {
                std::cout << "Elevated instance launched." << std::endl;
            } else {
                std::cerr << "Unable to request administrator privileges. Error code: " << GetLastError() << std::endl;
            }

            return 0;
        }

        std::cout << "\nAttempting to reboot the system..." << std::endl;

        if (EnableShutdownPrivilege()) {
            // EWX_REBOOT: Restarts the system.
            // EWX_FORCEIFHUNG: Forces applications to close if they are unresponsive.
            if (ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER)) {
                return 0; // Reboot successfully initiated
            } else {
                std::cerr << "ExitWindowsEx failed. Error code: " << GetLastError() << std::endl;
            }
        } else {
            std::cerr << "Failed to acquire necessary reboot privileges." << std::endl;
        }

        // Fallback option in case API privileges fail under standard user limits
        std::cout << "Attempting fallback reboot method..." << std::endl;
        system("shutdown /r /t 5");
    } else {
        std::cout << "\nReboot canceled." << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.ignore(10000, '\n'); // Clear the input buffer
        std::cin.get(); // Wait for keypress
    }

    return 0;
}