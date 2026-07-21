#include <windows.h>
#include <winevt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "shell32.lib")

struct EventOptions {
    DWORD maxEvents = 100;
    int level = -1;
    std::wstring provider;
    int sinceHours = -1;
    bool fullMessage = true;
    std::wstring savePath;
};

bool IsRunAsAdmin() {
    return IsUserAnAdmin() != FALSE;
}

bool RequestElevation() {
    wchar_t szPath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;
        if (ShellExecuteExW(&sei)) {
            return true;
        }
    }
    return false;
}

const wchar_t* GetLevelString(BYTE level) {
    switch (level) {
        case 1:  return L"CRIT";
        case 2:  return L"ERR ";
        case 3:  return L"WARN";
        case 5:  return L"DEBG";
        default: return L"INFO";
    }
}

void PrintError(const wchar_t* message) {
    wprintf(L"[ERROR] %ls (Error code: %lu)\n", message, GetLastError());
}

std::wstring EscapeXPathValue(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t ch : value) {
        if (ch == L'\'') {
            out += L"''";
        } else {
            out += ch;
        }
    }
    return out;
}

std::wstring BuildEventQuery(const EventOptions& options) {
    std::vector<std::wstring> clauses;

    if (options.level > 0) {
        clauses.push_back(L"Level=" + std::to_wstring(options.level));
    }

    if (!options.provider.empty()) {
        clauses.push_back(L"Provider[@Name='" + EscapeXPathValue(options.provider) + L"']");
    }

    if (options.sinceHours > 0) {
        ULONGLONG ms = static_cast<ULONGLONG>(options.sinceHours) * 60ULL * 60ULL * 1000ULL;
        clauses.push_back(L"TimeCreated[timediff(@SystemTime) <= " + std::to_wstring(ms) + L"]");
    }

    if (clauses.empty()) {
        return L"*";
    }

    std::wstring query = L"*[System[";
    for (size_t i = 0; i < clauses.size(); ++i) {
        query += clauses[i];
        if (i + 1 < clauses.size()) {
            query += L" and ";
        }
    }
    query += L"]]";
    return query;
}

bool ParseLevel(const std::wstring& value, int& outLevel) {
    std::wstring lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });

    if (lower == L"critical" || lower == L"crit" || lower == L"1") {
        outLevel = 1;
        return true;
    }
    if (lower == L"error" || lower == L"err" || lower == L"2") {
        outLevel = 2;
        return true;
    }
    if (lower == L"warning" || lower == L"warn" || lower == L"3") {
        outLevel = 3;
        return true;
    }
    if (lower == L"info" || lower == L"information" || lower == L"4") {
        outLevel = 4;
        return true;
    }
    if (lower == L"debug" || lower == L"5") {
        outLevel = 5;
        return true;
    }

    return false;
}

void PrintUsage(const wchar_t* exeName, const wchar_t* defaultFile) {
    wprintf(L"Usage: %ls [options]\n", exeName);
    wprintf(L"Options:\n");
    wprintf(L"  --count <N>        Number of events to read (default: 100)\n");
    wprintf(L"  --level <value>    critical|error|warning|info|debug or 1-5\n");
    wprintf(L"  --provider <name>  Filter by provider name\n");
    wprintf(L"  --since-hours <N>  Filter to last N hours\n");
    wprintf(L"  --no-message       Skip expensive provider message formatting\n");
    wprintf(L"  --save <path>      Save output to file (skip prompt)\n");
    wprintf(L"  --help             Show help\n");
    wprintf(L"Default save name: %ls\n", defaultFile);
}

int ParseArguments(int argc, wchar_t* argv[], EventOptions& options, const wchar_t* defaultFile) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--help" || arg == L"-h") {
            PrintUsage(argv[0], defaultFile);
            return 1;
        }
        if (arg == L"--count" && i + 1 < argc) {
            options.maxEvents = static_cast<DWORD>(_wtoi(argv[++i]));
            if (options.maxEvents == 0) {
                options.maxEvents = 100;
            }
            continue;
        }
        if (arg == L"--level" && i + 1 < argc) {
            int level = -1;
            if (!ParseLevel(argv[++i], level)) {
                wprintf(L"Invalid --level value.\n");
                return -1;
            }
            options.level = level;
            continue;
        }
        if (arg == L"--provider" && i + 1 < argc) {
            options.provider = argv[++i];
            continue;
        }
        if (arg == L"--since-hours" && i + 1 < argc) {
            options.sinceHours = _wtoi(argv[++i]);
            continue;
        }
        if (arg == L"--save" && i + 1 < argc) {
            options.savePath = argv[++i];
            continue;
        }
        if (arg == L"--no-message") {
            options.fullMessage = false;
            continue;
        }

        wprintf(L"Unknown option: %ls\n", arg.c_str());
        PrintUsage(argv[0], defaultFile);
        return -1;
    }

    return 0;
}

std::wstring FormatEvent(EVT_HANDLE hEvent, EVT_HANDLE hContext, std::map<std::wstring, EVT_HANDLE>& publisherCache, bool fullMessage) {
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD dwPropertyCount = 0;
    PEVT_VARIANT pRenderedValues = NULL;

    if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
        DWORD status = GetLastError();
        if (status == ERROR_INSUFFICIENT_BUFFER) {
            dwBufferSize = dwBufferUsed;
            pRenderedValues = (PEVT_VARIANT)malloc(dwBufferSize);
            if (!pRenderedValues) {
                return L"";
            }

            if (!EvtRender(hContext, hEvent, EvtRenderEventValues, dwBufferSize, pRenderedValues, &dwBufferUsed, &dwPropertyCount)) {
                free(pRenderedValues);
                return L"";
            }
        } else {
            return L"";
        }
    }

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

    wchar_t headerBuf[512];
    swprintf_s(
        headerBuf,
        L"[%04d-%02d-%02d %02d:%02d:%02d] [%ls] [%ls] (ID:%u) ",
        stLocal.wYear,
        stLocal.wMonth,
        stLocal.wDay,
        stLocal.wHour,
        stLocal.wMinute,
        stLocal.wSecond,
        GetLevelString(level),
        providerName,
        eventID
    );

    std::wstring logLine = headerBuf;

    if (!fullMessage) {
        free(pRenderedValues);
        return logLine;
    }

    std::wstring providerKey = providerName;
    EVT_HANDLE hPublisher = NULL;
    std::map<std::wstring, EVT_HANDLE>::iterator it = publisherCache.find(providerKey);
    if (it == publisherCache.end()) {
        hPublisher = EvtOpenPublisherMetadata(NULL, providerName, NULL, 0, 0);
        publisherCache[providerKey] = hPublisher;
    } else {
        hPublisher = it->second;
    }

    LPWSTR pMessage = NULL;
    if (hPublisher != NULL) {
        DWORD dwMessageBufferSize = 0;
        DWORD dwMessageBufferUsed = 0;
        if (!EvtFormatMessage(hPublisher, hEvent, 0, 0, NULL, EvtFormatMessageEvent, dwMessageBufferSize, pMessage, &dwMessageBufferUsed)) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                dwMessageBufferSize = dwMessageBufferUsed;
                pMessage = (LPWSTR)malloc(dwMessageBufferSize * sizeof(WCHAR));
                if (pMessage != NULL) {
                    if (!EvtFormatMessage(hPublisher, hEvent, 0, 0, NULL, EvtFormatMessageEvent, dwMessageBufferSize, pMessage, &dwMessageBufferUsed)) {
                        free(pMessage);
                        pMessage = NULL;
                    }
                }
            }
        }
    }

    if (pMessage != NULL) {
        size_t len = wcslen(pMessage);
        while (len > 0 && (pMessage[len - 1] == L'\r' || pMessage[len - 1] == L'\n')) {
            pMessage[len - 1] = L'\0';
            --len;
        }
        logLine += pMessage;
        free(pMessage);
    } else {
        logLine += L"<No message description found>";
    }

    free(pRenderedValues);
    return logLine;
}

void ClosePublisherCache(std::map<std::wstring, EVT_HANDLE>& publisherCache) {
    for (std::map<std::wstring, EVT_HANDLE>::iterator it = publisherCache.begin(); it != publisherCache.end(); ++it) {
        if (it->second != NULL) {
            EvtClose(it->second);
        }
    }
    publisherCache.clear();
}

int wmain(int argc, wchar_t* argv[]) {
    const wchar_t* channelPath = L"Security";
    const wchar_t* defaultSaveName = L"security_log.txt";

    if (!IsRunAsAdmin()) {
        wprintf(L"This program requires administrator privileges to read security logs.\n");
        wprintf(L"Requesting User Account Control (UAC) elevation...\n");
        if (RequestElevation()) {
            return 0;
        }

        wprintf(L"Failed to request elevation. Exiting.\n");
        return 1;
    }

    EventOptions options;
    int parseStatus = ParseArguments(argc, argv, options, defaultSaveName);
    if (parseStatus > 0) {
        return 0;
    }
    if (parseStatus < 0) {
        return 1;
    }

    std::wstring query = BuildEventQuery(options);
    EVT_HANDLE hResults = EvtQuery(NULL, channelPath, query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection);
    if (hResults == NULL) {
        PrintError(L"Failed to query security event logs.");
        return 1;
    }

    wprintf(L"Parsing security events (newest first):\n\n");

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

    const DWORD batchSize = 64;
    EVT_HANDLE hEvents[batchSize] = { NULL };
    DWORD processedCount = 0;
    std::vector<std::wstring> logEntries;
    std::map<std::wstring, EVT_HANDLE> publisherCache;

    while (processedCount < options.maxEvents) {
        DWORD requestCount = options.maxEvents - processedCount;
        if (requestCount > batchSize) {
            requestCount = batchSize;
        }

        DWORD returnedCount = 0;
        if (!EvtNext(hResults, requestCount, hEvents, INFINITE, 0, &returnedCount)) {
            DWORD err = GetLastError();
            if (err != ERROR_NO_MORE_ITEMS) {
                PrintError(L"Failed to fetch next events.");
            }
            break;
        }

        for (DWORD i = 0; i < returnedCount; ++i) {
            std::wstring formattedLog = FormatEvent(hEvents[i], hContext, publisherCache, options.fullMessage);
            if (!formattedLog.empty()) {
                wprintf(L"%ls\n", formattedLog.c_str());
                logEntries.push_back(formattedLog);
                ++processedCount;
            }
            EvtClose(hEvents[i]);
            hEvents[i] = NULL;

            if (processedCount >= options.maxEvents) {
                break;
            }
        }
    }

    ClosePublisherCache(publisherCache);
    EvtClose(hContext);
    EvtClose(hResults);

    std::wstring savePath = options.savePath;
    if (savePath.empty()) {
        wprintf(L"\nDo you want to save this output to a text file? (y/n): ");
        wchar_t response;
        if (wscanf_s(L" %lc", &response, 1) == 1 && (response == L'y' || response == L'Y')) {
            wprintf(L"Enter filename/path (default: %ls): ", defaultSaveName);
            wchar_t filepath[MAX_PATH] = { 0 };
            while (getwchar() != L'\n');

            if (fgetws(filepath, MAX_PATH, stdin) != NULL) {
                size_t len = wcslen(filepath);
                if (len > 0 && filepath[len - 1] == L'\n') {
                    filepath[len - 1] = L'\0';
                }
                if (wcslen(filepath) == 0) {
                    savePath = defaultSaveName;
                } else {
                    savePath = filepath;
                }
            } else {
                savePath = defaultSaveName;
            }
        }
    }

    if (!savePath.empty()) {
        FILE* file = nullptr;
        if (_wfopen_s(&file, savePath.c_str(), L"w, ccs=UTF-8") == 0 && file != nullptr) {
            for (const auto& logLine : logEntries) {
                fwprintf(file, L"%ls\n", logLine.c_str());
            }
            fclose(file);
            wprintf(L"Output saved successfully to: %ls\n", savePath.c_str());
        } else {
            wprintf(L"Error opening file for writing.\n");
        }
    }

    wprintf(L"\nPress Enter to exit...");
    getchar();
    return 0;
}
