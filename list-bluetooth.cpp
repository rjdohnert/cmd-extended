#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <bluetoothapis.h>

// Automatically link libraries when compiling with MSVC
#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "Ws2_32.lib")

// Version: 2.0

// Helper function to print a Bluetooth address in standard XX:XX:XX:XX:XX:XX format
void PrintBluetoothAddress(const BLUETOOTH_ADDRESS& address) {
    for (int i = 5; i >= 0; --i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(address.rgBytes[i]);
        if (i > 0) {
            std::cout << ":";
        }
    }
    std::cout << std::dec << std::setfill(' '); // Reset manipulators
}

int main() {
    std::cout << "\n";
    std::cout << "Initializing Bluetooth Device Scan...\n";
    std::cout << "This may take a few seconds...\n\n";

    // Configure search parameters
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) };
    searchParams.fReturnAuthenticated = TRUE;  // Return authenticated (paired) devices
    searchParams.fReturnRemembered = TRUE;      // Return remembered devices
    searchParams.fReturnUnknown = TRUE;         // Return unknown/unpaired devices
    searchParams.fReturnConnected = TRUE;       // Return connected devices
    searchParams.fIssueInquiry = TRUE;          // Issue a new active inquiry (scan)
    searchParams.cTimeoutMultiplier = 4;        // Timeout: 4 * 1.28 seconds (~5 seconds)

    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(BLUETOOTH_DEVICE_INFO) };

    // Start enumerating devices
    HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);

    if (hFind == NULL) {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NO_MORE_ITEMS) {
            std::cout << "No Bluetooth devices found.\n";
        } else {
            std::cerr << "Failed to start Bluetooth device search. Error code: " << lastError << "\n";
            std::cerr << "Ensure Bluetooth is enabled on your Windows machine.\n";
        }
        return 1;
    }

    int deviceCount = 0;
    std::cout << "--------------------------------------------------------\n";
    std::cout << std::left << std::setw(25) << "Device Name" 
              << std::setw(20) << "Address" 
              << "Status\n";
    std::cout << "--------------------------------------------------------\n";

    do {
        deviceCount++;
        
        // Print device name (stored as WCHAR/wide characters)
        std::wcout << std::left << std::setw(25) << deviceInfo.szName;
        
        // Print device address
        PrintBluetoothAddress(deviceInfo.Address);
        std::cout << "   ";

        // Print device status flags
        if (deviceInfo.fConnected) {
            std::cout << "[Connected] ";
        }
        if (deviceInfo.fRemembered) {
            std::cout << "[Paired]";
        }
        if (!deviceInfo.fConnected && !deviceInfo.fRemembered) {
            std::cout << "[Unpaired]";
        }
        
        std::cout << "\n";

    } while (BluetoothFindNextDevice(hFind, &deviceInfo));

    // Clean up the search handle
    BluetoothFindDeviceClose(hFind);

    std::cout << "--------------------------------------------------------\n";
    std::cout << "Scan finished. Total devices found: " << deviceCount << "\n";

    return 0;
}