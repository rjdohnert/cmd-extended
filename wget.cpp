#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>

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

    // Initialize WinINet
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

    // Open the connection to the URL
    // This supports both HTTP and HTTPS out of the box
    HINTERNET hUrl = InternetOpenUrlA(
        hInternet, 
        url.c_str(), 
        NULL, 
        0, 
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, 
        0
    );

    if (!hUrl) {
        std::cerr << "Error: InternetOpenUrl failed. Error code: " << GetLastError() << "\n";
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

    // Open the local file for writing in binary mode
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Error: Failed to open output file " << filename << " for writing.\n";
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return 1;
    }

    std::cout << "Downloading: " << url << "\n";
    std::cout << "Saving to  : " << filename << "\n";

    char buffer[4096];
    DWORD bytesRead = 0;
    size_t totalBytesDownloaded = 0;

    // Read the data in chunks and write it to the file
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        totalBytesDownloaded += bytesRead;

        if (lengthAvailable && contentLength > 0) {
            double percent = (static_cast<double>(totalBytesDownloaded) / contentLength) * 100.0;
            std::printf("\rProgress: %.2f%% (%zu / %lu bytes)", percent, totalBytesDownloaded, contentLength);
        } else {
            std::printf("\rDownloaded: %zu bytes", totalBytesDownloaded);
        }
        std::fflush(stdout);
    }

    std::cout << "\nDownload complete.\n";

    // Clean up resources
    outFile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return 0;
}