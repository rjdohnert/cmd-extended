#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h> // Contains GUID_DEVINTERFACE_USB_DEVICE
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

    // Get class devices for the USB device interface class that are currently present
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_USB_DEVICE,
        nullptr,
        nullptr,
        DIGCF_DEVICEINTERFACE | DIGCF_PRESENT
    );

    if (devInfo == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to retrieve device information set. Error code: " << GetLastError() << std::endl;
        return 1;
    }

    SP_DEVICE_INTERFACE_DATA devInterfaceData = {};
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD index = 0;
    std::wcout <<"\n";
    std::wcout << L"Listing connected USB devices:\n";
    std::wcout << L"\n";

    // Enumerate the device interfaces
    while (SetupDiEnumDeviceInterfaces(devInfo, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, index, &devInterfaceData)) {
        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        // Retrieve the buffer size needed for interface detail data.
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &devInterfaceData, nullptr, 0, &requiredSize, nullptr);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || requiredSize < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
            ++index;
            continue;
        }

        std::vector<BYTE> detailBuffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &devInterfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
            std::wcerr << L"Failed to query USB interface detail for device #" << (index + 1)
                       << L". Error code: " << GetLastError() << std::endl;
            ++index;
            continue;
        }

        // Fetch properties
        std::wstring friendlyName = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_FriendlyName);
        std::wstring deviceDesc   = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_DeviceDesc);
        std::wstring manufacturer = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_Manufacturer);
        std::wstring hardwareId   = GetDeviceProperty(devInfo, &devInfoData, &DEVPKEY_Device_HardwareIds);

        // Fall back to description if friendly name is not available
        std::wstring displayName = friendlyName.empty() ? deviceDesc : friendlyName;
        if (displayName.empty()) {
            displayName = L"Unknown USB Device";
        }

        std::wcout << L"Device #" << (index + 1) << L":\n";
        std::wcout << L"  Name:         " << displayName << L"\n";
        if (!manufacturer.empty()) {
            std::wcout << L"  Manufacturer: " << manufacturer << L"\n";
        }
        if (!hardwareId.empty()) {
            std::wcout << L"  Hardware ID:  " << hardwareId << L"\n";
        }
        std::wcout << L"--------------------------------------------------\n";

        index++;
    }

    DWORD enumError = GetLastError();
    if (enumError != ERROR_NO_MORE_ITEMS) {
        std::wcerr << L"USB enumeration failed. Error code: " << enumError << std::endl;
        SetupDiDestroyDeviceInfoList(devInfo);
        return 1;
    }

    if (index == 0) {
        std::wcout << L"No active USB devices found.\n";
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return 0;
}