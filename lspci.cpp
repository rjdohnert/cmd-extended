#include <windows.h>
#include <setupapi.h>
#include <initguid.h> // Must be included before devpkey.h
#include <devpkey.h>   // Contains DEVPKEY definitions
#include <iostream>
#include <vector>
#include <string>
#include <locale>

// Link with SetupAPI.lib
#pragma comment(lib, "setupapi.lib")

// Version: 2.0

// Helper function to retrieve a device property
std::wstring GetDeviceProperty(HDEVINFO devInfo, PSP_DEVINFO_DATA devInfoData, const DEVPROPKEY* propKey) {
    DEVPROPTYPE propType;
    DWORD requiredSize = 0;

    // Call first to determine the required buffer size
    SetupDiGetDevicePropertyW(devInfo, devInfoData, propKey, &propType, nullptr, 0, &requiredSize, 0);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return L"";
    }

    std::vector<BYTE> buffer(requiredSize);
    if (SetupDiGetDevicePropertyW(devInfo, devInfoData, propKey, &propType, buffer.data(), requiredSize, nullptr, 0)) {
        if (propType == DEVPROP_TYPE_STRING) {
            return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()));
        }
        else if (propType == DEVPROP_TYPE_STRING_LIST) {
            // Return the first string in the multi-string list (common for hardware IDs)
            return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()));
        }
    }
    return L"";
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::wcout.imbue(std::locale(""));

    // Retrieve all present PCI devices using the "PCI" enumerator
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        nullptr,
        L"PCI",
        nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (devInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to retrieve PCI device list. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    DWORD index = 0;

    std::wcout <<"\n";
    std::wcout << L"Listing connected PCI devices:\n";
    std::wcout << L"--------------------------------------------------\n";
    
    // Enumerate the devices in the information set
    while (SetupDiEnumDeviceInfo(devInfo, index, &devInfoData)) {
        // Fetch properties
        std::wstring friendlyName = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_FriendlyName);
        std::wstring deviceDesc   = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_DeviceDesc);
        std::wstring manufacturer = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_Manufacturer);
        std::wstring location     = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_LocationInfo);
        std::wstring hardwareId   = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_HardwareIds);

        // Fall back to description if friendly name is not available
        std::wstring displayName = friendlyName.empty() ? deviceDesc : friendlyName;
        if (displayName.empty()) {
            displayName = L"Unknown PCI Device";
        }

        std::wcout << L"Device #" << (index + 1) << L":\n";
        std::wcout << L"  Name:         " << displayName << L"\n";
        if (!manufacturer.empty()) {
            std::wcout << L"  Manufacturer: " << manufacturer << L"\n";
        }
        if (!location.empty()) {
            std::wcout << L"  Location:     " << location << L"\n";
        }
        if (!hardwareId.empty()) {
            std::wcout << L"  Hardware ID:  " << hardwareId << L"\n";
        }
        std::wcout << L"--------------------------------------------------\n";

        index++;
    }

    DWORD enumError = GetLastError();
    if (enumError != ERROR_NO_MORE_ITEMS) {
        std::wcerr << L"PCI enumeration failed. Error code: " << enumError << std::endl;
        SetupDiDestroyDeviceInfoList(devInfo);
        return 1;
    }

    if (index == 0) {
        std::wcout << L"No active PCI devices found.\n";
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return 0;
}