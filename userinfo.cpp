#include <windows.h>
#include <ntsecapi.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "Secur32.lib")

// Helper to convert std::wstring to lowercase
std::wstring ToLower(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

// Helper to convert LSA_UNICODE_STRING to std::wstring
std::wstring LsaStringToWString(const LSA_UNICODE_STRING& lsaStr) {
    if (lsaStr.Buffer == nullptr || lsaStr.Length == 0) {
        return L"";
    }
    return std::wstring(lsaStr.Buffer, lsaStr.Length / sizeof(wchar_t));
}

// Map logon types to readable names
std::wstring GetLogonTypeString(ULONG type) {
    switch (type) {
        case Interactive:             return L"Interactive (Local)";
        case Network:                 return L"Network";
        case Batch:                   return L"Batch";
        case Service:                 return L"Service";
        case Unlock:                  return L"Unlock";
        case NetworkCleartext:        return L"Network Cleartext";
        case NewCredentials:          return L"New Credentials";
        case RemoteInteractive:       return L"Remote (RDP)";
        case CachedInteractive:       return L"Cached Interactive";
        case CachedRemoteInteractive: return L"Cached Remote";
        case CachedUnlock:            return L"Cached Unlock";
        default:                      return L"Unknown (" + std::to_wstring(type) + L")";
    }
}

// Categorize as either a system service or a standard user account
std::wstring GetAccountCategory(const std::wstring& username, const std::wstring& domain, ULONG logonType) {
    std::wstring lowerUser = ToLower(username);
    std::wstring lowerDomain = ToLower(domain);

    if (lowerUser == L"system" || 
        lowerUser == L"local service" || 
        lowerUser == L"network service" || 
        lowerUser == L"anonymous logon" ||
        lowerDomain == L"nt authority" ||
        logonType == Service) {
        return L"System/Service";
    }
    return L"User Account";
}

// Print elapsed time in days, hours, minutes, and seconds
void PrintDuration(ULONGLONG totalSeconds) {
    ULONGLONG days = totalSeconds / 86400;
    ULONGLONG hours = (totalSeconds % 86400) / 3600;
    ULONGLONG minutes = (totalSeconds % 3600) / 60;
    ULONGLONG seconds = totalSeconds % 60;

    if (days > 0) std::wcout << days << L"d ";
    if (hours > 0 || days > 0) std::wcout << hours << L"h ";
    if (minutes > 0 || hours > 0 || days > 0) std::wcout << minutes << L"m ";
    std::wcout << seconds << L"s";
}

int main() {
    ULONG sessionCount = 0;
    PLUID sessions = nullptr;

    // Enumerate all active logon sessions
    NTSTATUS status = LsaEnumerateLogonSessions(&sessionCount, &sessions);
    if (status != 0) {
        std::cerr << "Failed to enumerate logon sessions. Please run as Administrator." << std::endl;
        return 1;
    }

    // Get current system time
    FILETIME ftNow;
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER now;
    now.LowPart = ftNow.dwLowDateTime;
    now.HighPart = ftNow.dwHighDateTime;

    // Output table headers
    std::wcout << "\n";
    std::wcout << std::left 
              << std::setw(22) << L"Account Name" 
              << std::setw(20) << L"Domain" 
              << std::setw(18) << L"Category" 
              << std::setw(22) << L"Logon Type" 
              << L"Active Duration" << std::endl;
    std::wcout << std::wstring(95, L'-') << std::endl;

    for (ULONG i = 0; i < sessionCount; ++i) {
        PSECURITY_LOGON_SESSION_DATA sessionData = nullptr;
        status = LsaGetLogonSessionData(&sessions[i], &sessionData);

        if (status == 0 && sessionData != nullptr) {
            std::wstring username = LsaStringToWString(sessionData->UserName);
            std::wstring domain = LsaStringToWString(sessionData->LogonDomain);

            // Handle empty username values representing anonymous or undefined system states
            if (username.empty()) {
                username = L"(anonymous)";
            }
            if (domain.empty()) {
                domain = L"(none)";
            }

            std::wstring category = GetAccountCategory(username, domain, sessionData->LogonType);
            std::wstring logonTypeStr = GetLogonTypeString(sessionData->LogonType);

            std::wcout << std::left 
                      << std::setw(22) << username 
                      << std::setw(20) << domain 
                      << std::setw(18) << category 
                      << std::setw(22) << logonTypeStr;

            // Calculate active duration
            ULARGE_INTEGER logonTime;
            logonTime.LowPart = sessionData->LogonTime.LowPart;
            logonTime.HighPart = sessionData->LogonTime.HighPart;

            if (now.QuadPart >= logonTime.QuadPart && logonTime.QuadPart != 0) {
                ULONGLONG diffSeconds = (now.QuadPart - logonTime.QuadPart) / 10000000ULL;
                PrintDuration(diffSeconds);
            } else {
                std::wcout << L"N/A"; // Some system-created sessions may not record a logon time
            }
            std::wcout << std::endl;

            LsaFreeReturnBuffer(sessionData);
        }
    }

    if (sessions != nullptr) {
        LsaFreeReturnBuffer(sessions);
    }

    return 0;
}