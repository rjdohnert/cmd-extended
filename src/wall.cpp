#include <windows.h>
#include <wtsapi32.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <lmcons.h>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "advapi32.lib")

// Helper to convert UTF-8/ANSI narrow string to UTF-16 wide string
std::wstring ConvertToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Retrieves the active process's username
std::wstring GetCurrentUsername() {
    wchar_t buffer[UNLEN + 1];
    DWORD size = UNLEN + 1;
    if (GetUserNameW(buffer, &size)) {
        return std::wstring(buffer);
    }
    return L"UnknownUser";
}

// Retrieves the local computer name
std::wstring GetCurrentComputerName() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(buffer, &size)) {
        return std::wstring(buffer);
    }
    return L"UnknownHost";
}

// Retrieves the Remote Desktop Services Session ID for the current process
DWORD GetCurrentSessionId() {
    DWORD sessionId = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
        return sessionId;
    }
    return 0;
}

// Returns the formatted local system date and time
std::wstring GetFormattedTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t time_buf[128];
    swprintf_s(time_buf, L"%04d-%02d-%02d %02d:%02d:%02d", 
               st.wYear, st.wMonth, st.wDay, 
               st.wHour, st.wMinute, st.wSecond);
    return std::wstring(time_buf);
}

// Reads the contents of a text file into a wide string
std::wstring ReadFileToWString(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << filepath << "\n";
        return L"";
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return ConvertToWide(content);
}

void PrintHelp() {
    std::cout << "Usage: wall [file]\n"
              << "Broadcast a message to all active logged-in Windows sessions.\n\n"
              << "If no file is specified, the message is read from standard input.\n"
              << "To finish typing on standard input, press Ctrl+Z and then Enter.\n";
}

int main(int argc, char* argv[]) {
    std::wstring user_message;

    // Command-line parsing
    if (argc > 1) {
        std::string first_arg = argv[1];
        if (first_arg == "-h" || first_arg == "--help" || first_arg == "/?") {
            PrintHelp();
            return 0;
        }
        user_message = ReadFileToWString(first_arg);
        if (user_message.empty()) {
            return 1;
        }
    } else {
        // Read message from standard input
        std::wstring line;
        std::wcout << L"Type message, then press Ctrl+Z followed by Enter to broadcast:\n";
        while (std::getline(std::wcin, line)) {
            user_message += line + L"\n";
        }
    }

    if (user_message.empty()) {
        std::cerr << "Error: Cannot broadcast an empty message.\n";
        return 1;
    }

    // Gather session metadata
    std::wstring username = GetCurrentUsername();
    std::wstring computer = GetCurrentComputerName();
    std::wstring time_str = GetFormattedTime();
    DWORD my_session = GetCurrentSessionId();

    // Construct the formatted broadcast message
    std::wstringstream title;
    title << L"Broadcast Message from " << username << L"@" << computer;

    std::wstringstream body;
    body << L"Broadcast message from " << username << L"@" << computer 
         << L" (Session " << my_session << L") at " << time_str << L"...\n\n"
         << user_message;

    std::wstring final_title = title.str();
    std::wstring final_body = body.str();

    // Enumerate active sessions on the local system
    PWTS_SESSION_INFOW pSessionInfo = NULL;
    DWORD dwCount = 0;
    
    if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &dwCount)) {
        bool broadcast_attempted = false;

        for (DWORD i = 0; i < dwCount; ++i) {
            // Skip Session 0 (non-interactive services session)
            if (pSessionInfo[i].SessionId == 0) {
                continue;
            }

            // Target active or connected desktop sessions
            if (pSessionInfo[i].State == WTSActive || pSessionInfo[i].State == WTSConnected) {
                broadcast_attempted = true;
                DWORD response = 0;
                
                // WTSSendMessageW takes lengths in bytes, not character count
                DWORD title_bytes = static_cast<DWORD>(final_title.size() * sizeof(wchar_t));
                DWORD body_bytes = static_cast<DWORD>(final_body.size() * sizeof(wchar_t));

                BOOL result = WTSSendMessageW(
                    WTS_CURRENT_SERVER_HANDLE,
                    pSessionInfo[i].SessionId,
                    const_cast<LPWSTR>(final_title.c_str()),
                    title_bytes,
                    const_cast<LPWSTR>(final_body.c_str()),
                    body_bytes,
                    MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST,
                    0, // Timeout (0 waits indefinitely until dismissed)
                    &response,
                    FALSE // bWait = FALSE to send asynchronously without blocking
                );

                if (!result) {
                    DWORD err = GetLastError();
                    std::wcerr << L"Failed to send broadcast to Session " << pSessionInfo[i].SessionId;
                    if (err == ERROR_ACCESS_DENIED) {
                        std::wcerr << L" (Access Denied. Run as Administrator to message other users.)\n";
                    } else {
                        std::wcerr << L" (Error Code: " << err << L")\n";
                    }
                }
            }
        }
        
        if (!broadcast_attempted) {
            std::wcout << L"No active interactive user sessions were found to broadcast to.\n";
        }
        
        WTSFreeMemory(pSessionInfo);
    } else {
        std::cerr << "Error: Failed to enumerate system sessions (Error: " << GetLastError() << ")\n";
        return 1;
    }

    return 0;
}