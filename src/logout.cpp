#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#pragma comment(lib, "user32.lib")
#elif defined(__linux__) || defined(__APPLE__)
#include <cstdlib>
#include <unistd.h>
#endif

bool LogoutUser() {
#if defined(_WIN32)
    // EWX_LOGOFF shuts down all processes running in the logon session,
    // then logs the user off.
    // If you wish to force applications to close (which may cause data loss),
    // you can use EWX_LOGOFF | EWX_FORCE.
    if (ExitWindowsEx(EWX_LOGOFF, 0)) {
        return true;
    } else {
        std::cerr << "Windows ExitWindowsEx failed. Error code: " << GetLastError() << std::endl;
        return false;
    }
#elif defined(__linux__)
    // On modern Linux distributions using systemd, 'loginctl' can terminate the current session.
    int result = std::system("loginctl terminate-session self");
    return (result == 0);
#elif defined(__APPLE__)
    // On macOS, we can use AppleScript via the osascript command to log out.
    int result = std::system("osascript -e 'tell application \"System Events\" to log out'");
    return (result == 0);
#else
    std::cerr << "Unsupported operating system." << std::endl;
    return false;
#endif
}

int main() {
    std::cout << "Are you sure you want to log out? (y/n): ";
    char response;
    std::cin >> response;

    if (response == 'y' || response == 'Y') {
        std::cout << "Attempting to log out..." << std::endl;
        if (!LogoutUser()) {
            std::cerr << "Could not initiate logout." << std::endl;
            return 1;
        }
    } else {
        std::cout << "Logout canceled." << std::endl;
    }

    return 0;
}