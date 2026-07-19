#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>
#include <cctype>
#include <limits>
#include <windows.h>

// Version: 2.0

// Function declarations
bool isLeapYear(int year);
int getDaysInMonth(int month, int year);
int getFirstDayOfMonth(int month, int year);
void printCalendar(int month, int year);
std::string getMonthName(int month);
void ClearConsole();
bool ReadIntWithPrompt(const char* prompt, int& value);

void ClearConsole() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        return;
    }

    const DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    COORD home = { 0, 0 };
    DWORD written = 0;

    FillConsoleOutputCharacterA(hConsole, ' ', cellCount, home, &written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cellCount, home, &written);
    SetConsoleCursorPosition(hConsole, home);
}

bool ReadIntWithPrompt(const char* prompt, int& value) {
    std::cout << prompt;
    if (std::cin >> value) {
        return true;
    }

    std::cin.clear();
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    return false;
}

int main() {
    // Get the current system date
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);

    int currentMonth = localTime->tm_mon + 1; // tm_mon is 0-11
    int currentYear = localTime->tm_year + 1900;

    char choice;
    bool running = true;

    while (running) {
        // Clear the console without invoking an external shell.
        ClearConsole();

        printCalendar(currentMonth, currentYear);

        std::cout << "\n Options:\n";
        std::cout << " [n] Next Month   [p] Previous Month\n";
        std::cout << " [g] Go to Date   [q] Quit\n";
        std::cout << "\n Enter choice: ";
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            continue;
        }

        switch (std::tolower(static_cast<unsigned char>(choice))) {
            case 'n':
                currentMonth++;
                if (currentMonth > 12) {
                    currentMonth = 1;
                    currentYear++;
                }
                break;
            case 'p':
                currentMonth--;
                if (currentMonth < 1) {
                    currentMonth = 12;
                    currentYear--;
                }
                break;
            case 'g': {
                int tempMonth, tempYear;
                if (!ReadIntWithPrompt("\n Enter month (1-12): ", tempMonth) ||
                    !ReadIntWithPrompt(" Enter year (e.g., 2026): ", tempYear)) {
                    std::cout << " Invalid numeric input. Press Enter to continue...";
                    std::cin.clear();
                    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                    std::cin.get();
                    break;
                }
                if (tempMonth >= 1 && tempMonth <= 12 && tempYear >= 1) {
                    currentMonth = tempMonth;
                    currentYear = tempYear;
                } else {
                    std::cout << " Invalid date values. Press Enter to continue...";
                    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                    std::cin.get();
                }
                break;
            }
            case 'q':
                running = false;
                break;
            default:
                break;
        }
    }

    return 0;
}

// Check if the year is a leap year
bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// Get total days in a given month
int getDaysInMonth(int month, int year) {
    const std::vector<int> daysInMonths = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return daysInMonths[month - 1];
}

// Find the day of the week for the 1st of the month (0 = Sunday, 1 = Monday, etc.)
int getFirstDayOfMonth(int month, int year) {
    std::tm time_in = { 0 };
    time_in.tm_mday = 1;
    time_in.tm_mon = month - 1;
    time_in.tm_year = year - 1900;
    std::mktime(&time_in);
    return time_in.tm_wday;
}

// Get the name of the month
std::string getMonthName(int month) {
    const std::vector<std::string> monthNames = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    return monthNames[month - 1];
}

// Render the calendar grid to the console
void printCalendar(int month, int year) {
    std::string monthYearStr = getMonthName(month) + " " + std::to_string(year);
    
    // Centering the header text
    int padding = (20 - static_cast<int>(monthYearStr.length())) / 2;
    if (padding > 0) {
        std::cout << std::string(padding, ' ');
    }
    std::cout << monthYearStr << "\n";
    std::cout << "--------------------\n";
    std::cout << "Su Mo Tu We Th Fr Sa\n";

    int firstDay = getFirstDayOfMonth(month, year);
    int days = getDaysInMonth(month, year);

    // Print leading spaces for the first week
    for (int i = 0; i < firstDay; ++i) {
        std::cout << "   ";
    }

    // Print days of the month
    for (int day = 1; day <= days; ++day) {
        std::cout << std::setw(2) << day << " ";
        
        // Break line at the end of Saturday (index 6)
        if ((day + firstDay) % 7 == 0) {
            std::cout << "\n";
        }
    }
    std::cout << "\n--------------------\n";
}