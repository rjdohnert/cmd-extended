#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>

enum class ShellType {
    Bourne,
    CShell,
    PowerShell,
    Cmd,
    PrintDatabase
};

// Map GNU dircolors config keys to LS_COLORS identifiers
const std::unordered_map<std::string, std::string> KEY_MAP = {
    {"RESET", "rs"},
    {"DIR", "di"},
    {"LINK", "ln"},
    {"MULTIHARDLINK", "mh"},
    {"FIFO", "fi"},
    {"SOCK", "so"},
    {"DOOR", "do"},
    {"BLK", "bd"},
    {"CHR", "cd"},
    {"ORPHAN", "or"},
    {"MISSING", "mi"},
    {"SETUID", "su"},
    {"SETGID", "sg"},
    {"CAPABILITY", "ca"},
    {"STICKY_OTHER_WRITABLE", "tw"},
    {"OTHER_WRITABLE", "ow"},
    {"STICKY", "st"},
    {"EXEC", "ex"},
    {"FILE", "fi"}
};

// Default embedded dircolors configuration database
const char* DEFAULT_DATABASE = R"(
# Default dircolors configuration for Windows
RESET 0
DIR 01;34
LINK 01;36
EXEC 01;32
FIFO 40;33
SOCK 01;35
BLK 40;33;01
CHR 40;33;01
ORPHAN 40;31;01
MISSING 00

# Executables & Scripts on Windows
.exe 01;32
.bat 01;32
.cmd 01;32
.ps1 01;32
.com 01;32
.msi 01;32

# Archives / Compressed files
.tar 01;31
.tgz 01;31
.zip 01;31
.z 01;31
.gz 01;31
.bz2 01;31
.7z 01;31
.rar 01;31
.jar 01;31

# Images
.jpg 01;35
.jpeg 01;35
.png 01;35
.gif 01;35
.bmp 01;35
.svg 01;35
.ico 01;35

# Audio / Video
.mp3 01;36
.wav 01;36
.flac 01;36
.mp4 01;35
.mkv 01;35
.avi 01;35
)";

// Utility: Trim whitespace from a string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Parses input stream into LS_COLORS format
std::string parse_config(std::istream& stream) {
    std::string line;
    std::vector<std::string> entries;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key, val;
        if (!(iss >> key >> val)) continue;

        std::string ls_key;
        if (key[0] == '.') {
            ls_key = "*" + key;
        } else if (key[0] == '*') {
            ls_key = key;
        } else {
            std::string key_upper = key;
            for (auto& c : key_upper) c = static_cast<char>(std::toupper(c));
            
            auto it = KEY_MAP.find(key_upper);
            if (it != KEY_MAP.end()) {
                ls_key = it->second;
            } else {
                ls_key = key;
            }
        }
        entries.push_back(ls_key + "=" + val);
    }

    std::string result;
    for (size_t i = 0; i < entries.size(); ++i) {
        result += entries[i];
        if (i + 1 < entries.size()) {
            result += ":";
        }
    }
    return result;
}

void print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... [FILE]\n"
              << "Output commands to set the LS_COLORS environment variable.\n\n"
              << "Options:\n"
              << "  -b, --sh, --bourne-shell    Output Bourne shell code (default)\n"
              << "  -c, --csh, --c-shell        Output C shell code\n"
              << "  -ps, --powershell           Output PowerShell code\n"
              << "  -cmd                        Output Windows Command Prompt (CMD) code\n"
              << "  -p, --print-database        Output default configuration database\n"
              << "  -h, --help                  Display this help message\n\n"
              << "Examples:\n"
              << "  PowerShell:  dircolors -ps | Invoke-Expression\n"
              << "  CMD:         for /f \"tokens=*\" %i in ('dircolors -cmd') do %i\n"
              << "  Git Bash:    eval $(dircolors -b)\n";
}

int main(int argc, char* argv[]) {
    ShellType shell = ShellType::Bourne;
    std::string filename = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-b" || arg == "--sh" || arg == "--bourne-shell") {
            shell = ShellType::Bourne;
        } else if (arg == "-c" || arg == "--csh" || arg == "--c-shell") {
            shell = ShellType::CShell;
        } else if (arg == "-ps" || arg == "--powershell") {
            shell = ShellType::PowerShell;
        } else if (arg == "-cmd") {
            shell = ShellType::Cmd;
        } else if (arg == "-p" || arg == "--print-database") {
            shell = ShellType::PrintDatabase;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg[0] != '-') {
            filename = arg;
        } else {
            std::cerr << "dircolors: unrecognized option '" << arg << "'\n";
            std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
            return 1;
        }
    }

    if (shell == ShellType::PrintDatabase) {
        std::cout << DEFAULT_DATABASE;
        return 0;
    }

    std::string ls_colors;

    if (!filename.empty()) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "dircolors: cannot open '" << filename << "': No such file or directory\n";
            return 1;
        }
        ls_colors = parse_config(file);
    } else {
        std::istringstream stream(DEFAULT_DATABASE);
        ls_colors = parse_config(stream);
    }

    // Output command based on target shell
    switch (shell) {
        case ShellType::Bourne:
            std::cout << "LS_COLORS='" << ls_colors << "';\nexport LS_COLORS;\n";
            break;
        case ShellType::CShell:
            std::cout << "setenv LS_COLORS '" << ls_colors << "';\n";
            break;
        case ShellType::PowerShell:
            std::cout << "$env:LS_COLORS = '" << ls_colors << "'\n";
            break;
        case ShellType::Cmd:
            std::cout << "set \"LS_COLORS=" << ls_colors << "\"\n";
            break;
        default:
            break;
    }

    return 0;
}