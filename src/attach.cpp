#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>
#include <windows.h>
#include <initguid.h>
#include <virtdisk.h>
#include <shlwapi.h>

#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

struct AttachOptions {
    bool detach = false;
    bool read_only = false;
    std::wstring disk_path = L"";
};

// Verify if the process is running with administrative privileges
bool IsUserAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin == TRUE;
}

// Mounts (attaches) the disk image
DWORD AttachDisk(const std::wstring& diskPath, bool readOnly) {
    VIRTUAL_STORAGE_TYPE storageType;
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
    openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    openParams.Version1.RWDepth = OPEN_VIRTUAL_DISK_RW_DEPTH_DEFAULT;

    VIRTUAL_DISK_ACCESS_MASK accessMask = VIRTUAL_DISK_ACCESS_NONE;

    // Detect file extension for appropriate access mapping
    std::wstring ext = L"";
    size_t dot_pos = diskPath.find_last_of(L'.');
    if (dot_pos != std::wstring::npos) {
        ext = diskPath.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    }

    if (ext == L".iso") {
        accessMask = VIRTUAL_DISK_ACCESS_READ;
        readOnly = true; // ISO files can only be attached read-only
    } else {
        accessMask = VIRTUAL_DISK_ACCESS_ALL;
    }

    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD status = OpenVirtualDisk(
        &storageType,
        diskPath.c_str(),
        accessMask,
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        &openParams,
        &handle
    );

    if (status != ERROR_SUCCESS) {
        return status;
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attachParams = {};
    attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    ATTACH_VIRTUAL_DISK_FLAG attachFlags = ATTACH_VIRTUAL_DISK_FLAG_NONE;
    if (readOnly) {
        attachFlags |= ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY;
    }

    status = AttachVirtualDisk(
        handle,
        NULL,
        attachFlags,
        0,
        &attachParams,
        NULL
    );

    CloseHandle(handle);
    return status;
}

// Unmounts (detaches) the disk image
DWORD DetachDisk(const std::wstring& diskPath) {
    VIRTUAL_STORAGE_TYPE storageType;
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
    storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
    openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    openParams.Version1.RWDepth = OPEN_VIRTUAL_DISK_RW_DEPTH_DEFAULT;

    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD status = OpenVirtualDisk(
        &storageType,
        diskPath.c_str(),
        VIRTUAL_DISK_ACCESS_DETACH,
        OPEN_VIRTUAL_DISK_FLAG_NONE,
        &openParams,
        &handle
    );

    if (status != ERROR_SUCCESS) {
        return status;
    }

    status = DetachVirtualDisk(handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
    CloseHandle(handle);
    return status;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        std::wcerr << L"usage: attach [-d] [-r] <file-path>\n";
        std::wcerr << L"       -d : Detach (unmount) the virtual disk\n";
        std::wcerr << L"       -r : Mount as read-only\n";
        return 1;
    }

    AttachOptions opts;

    // Parse options block
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg[0] == L'-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L'd':
                        opts.detach = true;
                        break;
                    case L'r':
                        opts.read_only = true;
                        break;
                    default:
                        std::wcerr << L"attach: unknown option -- " << arg[j] << std::endl;
                        std::wcerr << L"usage: attach [-d] [-r] <file-path>\n";
                        return 1;
                }
            }
        } else {
            if (!opts.disk_path.empty()) {
                std::wcerr << L"attach: too many arguments\n";
                std::wcerr << L"usage: attach [-d] [-r] <file-path>\n";
                return 1;
            }
            opts.disk_path = arg;
        }
    }

    if (opts.disk_path.empty()) {
        std::wcerr << L"attach: missing file path\n";
        std::wcerr << L"usage: attach [-d] [-r] <file-path>\n";
        return 1;
    }

    if (!IsUserAdmin()) {
        std::wcerr << L"attach: administrator privileges are required to modify virtual disk attachments.\n";
        std::wcerr << L"Please run this utility from an elevated (Administrator) command prompt.\n";
        return 1;
    }

    // Resolve absolute path (required by virtual disk interface)
    wchar_t fullPath[MAX_PATH];
    if (GetFullPathNameW(opts.disk_path.c_str(), MAX_PATH, fullPath, nullptr) == 0) {
        std::wcerr << L"attach: failed to resolve absolute path for: " << opts.disk_path << std::endl;
        return 1;
    }
    std::wstring resolved_path(fullPath);

    if (opts.detach) {
        std::wcout << L"Detaching virtual disk: " << resolved_path << L"...\n";
        DWORD status = DetachDisk(resolved_path);
        if (status == ERROR_SUCCESS) {
            std::wcout << L"Successfully detached virtual disk.\n";
            return 0;
        } else {
            std::wcerr << L"attach: failed to detach virtual disk. Error code: " << status << std::endl;
            return 1;
        }
    } else {
        std::wcout << L"Attaching virtual disk: " << resolved_path 
                   << (opts.read_only ? L" (read-only)" : L"") << L"...\n";
        DWORD status = AttachDisk(resolved_path, opts.read_only);
        if (status == ERROR_SUCCESS) {
            std::wcout << L"Successfully attached virtual disk.\n";
            return 0;
        } else {
            std::wcerr << L"attach: failed to attach virtual disk. Error code: " << status << std::endl;
            return 1;
        }
    }
}