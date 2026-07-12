#include <iostream>
#include <locale>
#include <windows.h>
#include <lm.h>

// Link with the Netapi32.lib library
#pragma comment(lib, "netapi32.lib")

int main() {
    // Configure standard output to handle wide characters (Unicode)
    std::wcout.imbue(std::locale(""));

    LPUSER_INFO_0 pBuf = nullptr;
    DWORD dwEntriesRead = 0;
    DWORD dwTotalEntries = 0;
    DWORD dwResumeHandle = 0;
    NET_API_STATUS nStatus;

    // Retrieve user accounts. 
    // Level 0 returns basic user account names (USER_INFO_0 structure).
    nStatus = NetUserEnum(
        nullptr,                       // NULL targets the local computer
        0,                             // Information level 0 (username only)
        FILTER_NORMAL_ACCOUNT,         // Filter to list global user accounts
        reinterpret_cast<LPBYTE*>(&pBuf), // Buffer to receive the data
        MAX_PREFERRED_LENGTH,          // Preferred maximum length of returned data
        &dwEntriesRead,                // Pointer to count of elements actually enumerated
        &dwTotalEntries,               // Pointer to total approximate entries
        &dwResumeHandle                // Pointer to resume handle
    );

    // Check if the call was successful
    if (nStatus == NERR_Success || nStatus == ERROR_MORE_DATA) {
        if (pBuf != nullptr && dwEntriesRead > 0) {
            std::wcout << L"User Accounts Found:\n";
            std::wcout << L"\n";
            
            for (DWORD i = 0; i < dwEntriesRead; i++) {
                std::wcout << pBuf[i].usri0_name << L"\n";
            }
        } else {
            std::wcout << L"No user accounts found.\n";
        }
    } else {
        std::cerr << "Failed to retrieve user list. Error code: " << nStatus << std::endl;
    }

    // Free the allocated memory buffer
    if (pBuf != nullptr) {
        NetApiBufferFree(pBuf);
    }

    return 0;
}