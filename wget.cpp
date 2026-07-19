#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Link the WinINet library automatically when using MSVC compiler
#pragma comment(lib, "wininet.lib")

// Version: 2.0

// Helper function to extract a default filename from the URL if not provided
std::string GetFilenameFromUrl(const std::string& url) {
    size_t lastSlash = url.find_last_of("/\\");
    if (lastSlash != std::string::npos && lastSlash < url.length() - 1) {
        // Simple extraction, doesn't parse query parameters (e.g., ?id=1)
        size_t queryParam = url.find('?', lastSlash);
        if (queryParam != std::string::npos) {
            return url.substr(lastSlash + 1, queryParam - lastSlash - 1);
        }
        return url.substr(lastSlash + 1);
    }
    return "downloaded_file.bin";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: wget <URL> [output_filename]\n";
        return 1;
    }

    std::string url = argv[1];
    std::string filename = (argc > 2) ? argv[2] : GetFilenameFromUrl(url);
    std::string tempFilename = filename + ".part";

    const DWORD timeoutMs = 30000;
    const int maxAttempts = 3;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        HINTERNET hInternet = InternetOpenA(
            "Wget/1.0",                // User-Agent
            INTERNET_OPEN_TYPE_PRECONFIG, // Use registry settings for proxy config
            NULL,
            NULL,
            0
        );

        if (!hInternet) {
            std::cerr << "Error: InternetOpen failed. Error code: " << GetLastError() << "\n";
            return 1;
        }

        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
        InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));

        // Open the connection to the URL.
        HINTERNET hUrl = InternetOpenUrlA(
            hInternet,
            url.c_str(),
            NULL,
            0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE,
            0
        );

        if (!hUrl) {
            DWORD err = GetLastError();
            std::cerr << "Error: InternetOpenUrl failed. Error code: " << err << "\n";
            InternetCloseHandle(hInternet);
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        char statusCodeBuffer[16] = { 0 };
        DWORD statusCodeBufferSize = sizeof(statusCodeBuffer);
        if (!HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE, statusCodeBuffer, &statusCodeBufferSize, NULL)) {
            DWORD err = GetLastError();
            std::cerr << "Error: Unable to determine HTTP status code. Error code: " << err << "\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        long statusCode = std::strtol(statusCodeBuffer, nullptr, 10);
        if (statusCode < 200 || statusCode >= 300) {
            std::cerr << "Error: Server returned HTTP status " << statusCode << ".\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return 1;
        }

        // Attempt to get Content-Length header to show download size (optional)
        DWORD contentLength = 0;
        DWORD contentLengthSize = sizeof(contentLength);
        bool lengthAvailable = HttpQueryInfoA(
            hUrl,
            HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
            &contentLength,
            &contentLengthSize,
            NULL
        );

        // Open temporary local file for writing in binary mode.
        std::ofstream outFile(tempFilename, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "Error: Failed to open output file " << tempFilename << " for writing.\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return 1;
        }

        std::cout << "Downloading: " << url << "\n";
        std::cout << "Saving to  : " << filename << "\n";

        char buffer[4096];
        DWORD bytesRead = 0;
        size_t totalBytesDownloaded = 0;
        bool downloadSucceeded = true;

        // Read the data in chunks and write it to the file.
        while (true) {
            if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead)) {
                std::cerr << "\nError: InternetReadFile failed. Error code: " << GetLastError() << "\n";
                downloadSucceeded = false;
                break;
            }

            if (bytesRead == 0) {
                break;
            }

            outFile.write(buffer, bytesRead);
            if (!outFile.good()) {
                std::cerr << "\nError: Failed to write downloaded data to file.\n";
                downloadSucceeded = false;
                break;
            }

            totalBytesDownloaded += bytesRead;

            if (lengthAvailable && contentLength > 0) {
                double percent = (static_cast<double>(totalBytesDownloaded) / contentLength) * 100.0;
                std::printf("\rProgress: %.2f%% (%zu / %lu bytes)", percent, totalBytesDownloaded, contentLength);
            } else {
                std::printf("\rDownloaded: %zu bytes", totalBytesDownloaded);
            }
            std::fflush(stdout);
        }

        // Clean up resources
        outFile.close();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (!downloadSucceeded) {
            DeleteFileA(tempFilename.c_str());
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        if (!MoveFileExA(tempFilename.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            std::cerr << "\nError: Failed to finalize download file. Error code: " << GetLastError() << "\n";
            DeleteFileA(tempFilename.c_str());
            return 1;
        }

        std::cout << "\nDownload complete.\n";
        return 0;
    }

    return 1;
}