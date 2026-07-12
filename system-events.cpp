#include <windows.h>
#include <winevt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>

// Direct MSVC to link the required libraries
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "shell32.lib")

// Check if the current process has Administrator privileges
bool IsRunAsAdmin() {
    return IsUserAnAdmin() != FALSE;
}

// Request UAC elevation to relaunch the program as Administrator
bool RequestElevation() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";         // Triggers the Windows UAC dialog
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        if (ShellExecuteExW(&sei)) {
            return true;
        }
    }
    return false;
}

// Map Event Level bytes to classic dmesg style severity tags
const wchar_t* GetLevelString(BYTE level) {
    switch (level) {
        case 1:  return L"CRIT";
        case 2:  return L"ERR ";
        case 3:  return L"WARN";
        case 5:  return L"DEBG";
        default: return L"INFO"; // Default/Level 4 (Information)
    }
}

void PrintError(const wchar_t* message) {
    wprintf(L"[ERROR] %s (Error code: %lu)\n", message, GetLastError());
}

// Formats event details into a single dmesg-style string
std::wstring FormatEvent(EVT_HANDLE hEvent, EVT_HANDLE hContext) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;

    // Discover the required buffer size
    if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        DWORD status = GetLastError();
        if (status == ERROR_INSUFFICIENT_BUFFER) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);
            if (!pRenderedValues) return L"";

            if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
                free(pRenderedValues);
                return L"";
            }
        } else {
            return L"";
        }
    }

    // Extract values matching our context query:
    // Index 0: TimeCreated (FileTimeVal)
    // Index 1: Provider Name (StringVal)
    // Index 2: Level (ByteVal)
    // Index 3: EventID (UInt16Val)

    SYSTEMTIME stLocal = { 0 };
    if (pRenderedValues[0].Type == EvtVarTypeFileTime) {
        FILETIME ft;
        ft.dwLowDateTime = (DWORD)(pRenderedValues[0].FileTimeVal & 0xFFFFFFFF);
        ft.dwHighDateTime = (DWORD)(pRenderedValues[0].FileTimeVal >> 32);
        SYSTEMTIME stUTC;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    }

    const wchar_t* providerName = L"Unknown";
    if (pRenderedValues[1].Type == EvtVarTypeString && pRenderedValues[1].StringVal != NULL) {
        providerName = pRenderedValues[1].StringVal;
    }

    BYTE level = 0;
    if (pRenderedValues[2].Type == EvtVarTypeByte) {
        level = pRenderedValues[2].ByteVal;
    }

    UINT16 eventID = 0;
    if (pRenderedValues[3].Type == EvtVarTypeUInt16) {
        eventID = pRenderedValues[3].UInt16Val;
    }

    // Resolve the human-readable description string from the provider DLL
    LPWSTR pMessage = NULL;
    EVT_HANDLE hPublisher = EvtOpenPublisherMetadata(NULL, providerName, NULL, 0, 0);
    if (hPublisher != NULL) {
        DWORD dwMessageBufferSize = 0;
        DWORD dwMessageBufferUsed = 0;
        if (!EvtFormatMessage(hPublisher, hEvent, 0, 0, NULL, EvtFormatMessageEvent, dwMessageBufferSize, pMessage, &dwMessageBufferUsed)) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                dwMessageBufferSize = dwMessageBufferUsed;
                pMessage = (LPWSTR)malloc(dwMessageBufferSize * sizeof(WCHAR));
                if (pMessage) {
                    if (!EvtFormatMessage(hPublisher, hEvent, 0, 0, NULL, EvtFormatMessageEvent, dwMessageBufferSize, pMessage, &dwMessageBufferUsed)) {
                        free(pMessage);
                        pMessage = NULL;
                    }
                }
            }
        }
        EvtClose(hPublisher);
    }

    // Build the finalized single-line log string
    wchar_t headerBuf[512];
    swprintf_s(headerBuf, L"[%04d-%02d-%02d %02d:%02d:%02d] [%ls] [%ls] (ID:%u) ",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond,
        GetLevelString(level),
        providerName,
        eventID);

    std::wstring logLine = headerBuf;

    if (pMessage) {
        // Strip out trailing Carriage Return / Line Feed to enforce single-line format
        size_t len = wcslen(pMessage);
        while (len > 0 && (pMessage[len - 1] == L'\r' || pMessage[len - 1] == L'\n')) {
            pMessage[len - 1] = L'\0';
            len--;
        }
        logLine += pMessage;
        free(pMessage);
    } else {
        logLine += L"<No message description found>";
    }

    free(pRenderedValues);
    return logLine;
}

int main() {
    // 1. Enforce admin check before execution
    if (!IsRunAsAdmin()) {
        wprintf(L"This program requires administrator privileges to read system logs.\n");
        wprintf(L"Requesting User Account Control (UAC) elevation...\n");
        if (RequestElevation()) {
            return 0; // Exits original process; elevated copy will run in new window
        } else {
            wprintf(L"Failed to request elevation. Exiting.\n");
            return 1;
        }
    }

    LPCWSTR channelPath = L"System";
    LPCWSTR query = L"*";

    // Query System log channel for newest events first
    EVT_HANDLE hResults = EvtQuery(NULL, channelPath, query, EvtQueryChannelPath | EvtQueryReverseDirection);
    if (hResults == NULL) {
        PrintError(L"Failed to query system event logs.");
        return 1;
    }

    wprintf(L"Parsing system events (newest first):\n\n");

    // Establish elements to render
    LPCWSTR ppValues[] = {
        L"Event/System/TimeCreated/@SystemTime",
        L"Event/System/Provider/@Name",
        L"Event/System/Level",
        L"Event/System/EventID"
    };
    DWORD count = sizeof(ppValues) / sizeof(LPCWSTR);
    EVT_HANDLE hContext = EvtCreateRenderContext(count, ppValues, EvtRenderContextValues);

    if (hContext == NULL) {
        PrintError(L"Failed to create render context.");
        EvtClose(hResults);
        return 1;
    }

    const DWORD batchSize = 500; // Output 500 events for demonstration
    EVT_HANDLE hEvents[batchSize] = { NULL };
    DWORD returnedCount = 0;
    std::vector<std::wstring> logEntries;

    if (EvtNext(hResults, batchSize, hEvents, INFINITE, 0, &returnedCount)) {
        for (DWORD i = 0; i < returnedCount; i++) {
            std::wstring formattedLog = FormatEvent(hEvents[i], hContext);
            if (!formattedLog.empty()) {
                wprintf(L"%s\n", formattedLog.c_str());
                logEntries.push_back(formattedLog);
            }
            EvtClose(hEvents[i]);
        }
    } else {
        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_ITEMS) {
            PrintError(L"Failed to fetch next events.");
        } else {
            wprintf(L"No events found.\n");
        }
    }

    EvtClose(hContext);
    EvtClose(hResults);

    // 2. Offer to save the output to a text file
    wprintf(L"\nDo you want to save this output to a text file? (y/n): ");
    wchar_t response;
    wscanf_s(L" %lc", &response, 1);
    
    // Clear console input stream to ensure getchar() functions properly later
    while (getwchar() != L'\n');

    if (response == L'y' || response == L'Y') {
        wprintf(L"Enter filename/path (default: system_log.txt): ");
        wchar_t filepath[MAX_PATH];
        if (fgetws(filepath, MAX_PATH, stdin) != NULL) {
            size_t len = wcslen(filepath);
            if (len > 0 && filepath[len - 1] == L'\n') {
                filepath[len - 1] = L'\0';
            }
            if (wcslen(filepath) == 0) {
                wcscpy_s(filepath, L"system_log.txt");
            }
        } else {
            wcscpy_s(filepath, L"system_log.txt");
        }

        // Open text file as UTF-8
        FILE* file = nullptr;
        if (_wfopen_s(&file, filepath, L"w, ccs=UTF-8") == 0 && file != nullptr) {
            for (const auto& logLine : logEntries) {
                fwprintf(file, L"%s\n", logLine.c_str());
            }
            fclose(file);
            wprintf(L"Output saved successfully to: %ls\n", filepath);
        } else {
            wprintf(L"Error opening file for writing.\n");
        }
    }

    // 3. Ask to press Enter to exit
    wprintf(L"\nPress Enter to exit...");
    getchar();
    return 0;
}