#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <locale>
#include <cwchar>

// Version: 2.0

// Helper structure to hold drive details
struct DriveInfo {
    std::wstring letter;
    std::wstring volumeName;
    std::wstring driveType;
    std::wstring fileSystem;
    ULONGLONG totalBytes = 0;
    ULONGLONG freeBytes = 0;
    ULONGLONG usedBytes = 0;
    double usagePercentage = 0.0;
    bool hasMedia = false;
};

// Formats byte sizes into human-readable units (KB, MB, GB, TB)
std::wstring FormatBytes(ULONGLONG bytes) {
    double size = static_cast<double>(bytes);
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unitIndex = 0;
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    wchar_t buffer[64];
    swprintf_s(buffer, L"%.2f %s", size, units[unitIndex]);
    return buffer;
}

// Attempts to determine if the drive is an SSD or HDD using the Seek Penalty property
std::wstring GetPhysicalMediaType(wchar_t driveLetter) {
    wchar_t volumePath[] = L"\\\\.\\C:";
    volumePath[4] = driveLetter;

    HANDLE hDevice = CreateFileW(
        volumePath,
        0, // No access to drive is needed for this query
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        return L"";
    }

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenaltyDescriptor = {};
    DWORD bytesReturned = 0;

    BOOL success = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query,
        sizeof(query),
        &seekPenaltyDescriptor,
        sizeof(seekPenaltyDescriptor),
        &bytesReturned,
        NULL
    );

    CloseHandle(hDevice);

    if (success) {
        // If there is no seek penalty, it is typically classified as an SSD
        return seekPenaltyDescriptor.IncursSeekPenalty ? L" (HDD)" : L" (SSD)";
    }

    return L"";
}

// Resolves the general Windows drive classification
std::wstring GetDriveTypeString(const wchar_t* driveRoot, wchar_t driveLetter) {
    UINT type = GetDriveTypeW(driveRoot);
    switch (type) {
        case DRIVE_REMOVABLE: return L"Removable" + GetPhysicalMediaType(driveLetter);
        case DRIVE_FIXED:     return L"Fixed" + GetPhysicalMediaType(driveLetter);
        case DRIVE_REMOTE:    return L"Network";
        case DRIVE_CDROM:     return L"CD-ROM";
        case DRIVE_RAMDISK:   return L"RAM Disk";
        case DRIVE_UNKNOWN:
        default:              return L"Unknown";
    }
}

int main() {
    // Set console output to support UTF-8/Unicode formatting
    std::wcout.imbue(std::locale(""));

    // Get the buffer size needed for drive strings
    DWORD bufferLength = GetLogicalDriveStringsW(0, NULL);
    if (bufferLength == 0) {
        std::wcerr << L"Failed to retrieve logical drive information." << std::endl;
        return 1;
    }

    std::vector<wchar_t> buffer(bufferLength);
    if (GetLogicalDriveStringsW(bufferLength, buffer.data()) == 0) {
        std::wcerr << L"Failed to retrieve logical drive strings." << std::endl;
        return 1;
    }

    std::vector<DriveInfo> drives;
    wchar_t* drivePtr = buffer.data();

    // Iterate through the null-terminated drive strings
    while (*drivePtr) {
        DriveInfo info;
        info.letter = drivePtr; // E.g., "C:\"
        
        wchar_t letterChar = drivePtr[0];
        info.driveType = GetDriveTypeString(drivePtr, letterChar);

        wchar_t volName[MAX_PATH + 1] = { 0 };
        wchar_t fsName[MAX_PATH + 1] = { 0 };
        
        // Retrieve volume and file system information
        BOOL gotVolInfo = GetVolumeInformationW(
            drivePtr,
            volName, ARRAYSIZE(volName),
            NULL, NULL, NULL,
            fsName, ARRAYSIZE(fsName)
        );

        if (gotVolInfo) {
            info.volumeName = volName;
            info.fileSystem = fsName;
            info.hasMedia = true;
        } else {
            info.fileSystem = L"N/A";
            info.hasMedia = false;
        }

        // Retrieve space information if media is present
        ULARGE_INTEGER freeBytesAvail, totalBytes, totalFreeBytes;
        if (info.hasMedia && GetDiskFreeSpaceExW(drivePtr, &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
            info.totalBytes = totalBytes.QuadPart;
            info.freeBytes = totalFreeBytes.QuadPart;
            info.usedBytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
            if (info.totalBytes > 0) {
                info.usagePercentage = (static_cast<double>(info.usedBytes) / info.totalBytes) * 100.0;
            }
        }

        drives.push_back(info);
        drivePtr += wcslen(drivePtr) + 1;
    }

    // Output table headers
    std::wcout <<"\n";
    std::wcout << std::left 
               << std::setw(6)  << L"Drive"
               << std::setw(18) << L"Type"
               << std::setw(12) << L"Format"
               << std::setw(15) << L"Total Size"
               << std::setw(15) << L"Used Space"
               << std::setw(15) << L"Free Space"
               << std::setw(8)  << L"Usage %" 
               << std::endl;

    std::wcout << std::wstring(89, L'-') << std::endl;

    // Output data for each drive
    for (const auto& drive : drives) {
        std::wcout << std::left << std::setw(6) << drive.letter;
        std::wcout << std::left << std::setw(18) << drive.driveType;
        std::wcout << std::left << std::setw(12) << drive.fileSystem;

        if (drive.hasMedia && drive.totalBytes > 0) {
            std::wcout << std::left << std::setw(15) << FormatBytes(drive.totalBytes);
            std::wcout << std::left << std::setw(15) << FormatBytes(drive.usedBytes);
            std::wcout << std::left << std::setw(15) << FormatBytes(drive.freeBytes);
            std::wcout << std::left << std::fixed << std::setprecision(1) << drive.usagePercentage << L"%";
        } else {
            if (drive.driveType == L"CD-ROM" || drive.driveType == L"Removable") {
                std::wcout << L"No Media Available";
            } else {
                std::wcout << L"Unreadable / Not Ready";
            }
        }
        std::wcout << std::endl;
    }

    return 0;
}